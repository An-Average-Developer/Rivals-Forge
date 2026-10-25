#include "Windows/Main/Updates/UpdatesView.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/VersionCompare.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Updates/PageKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;
using namespace PageKit;

bool Blank(const std::wstring& text) {
    return std::all_of(text.begin(), text.end(), [](wchar_t c) { return std::iswspace(c) != 0; });
}

// The state's colour, icon, title and message
struct Look {
    Color color;
    M::Icon icon;
};

TextStyle HeroTitle() {
    TextStyle t = Kit::Display(26.f, DWRITE_FONT_WEIGHT_BOLD);
    t.align = HAlign::Center;
    t.wrap = true;
    return t;
}

TextStyle HeroText() { return Kit::Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center); }

// Remembers when `state` last changed (0..1 pop since then)
float StatePop(UI::Ui& ui, const char* key, int state, double* since = nullptr) {
    struct Since {
        int state = -1;
        double since = -10.0;
    };
    Since& a = ui.Data<Since>(ui.MakeId(key));
    if (a.state != state) {
        a.state = state;
        a.since = ui.Time();
    }
    if (since) *since = a.since;
    const float k = Anim::Clamp01(static_cast<float>((ui.Time() - a.since) / 0.5));
    if (k < 1.f) ui.KeepAnimating();
    return k;
}

// Installed → new / published: two slanted boxes and an arrow sliding between them
void FromTo(UI::Ui& ui, const Rect& r, const std::wstring& fromLabel, const std::wstring& from, const std::wstring& toLabel,
            const std::wstring& to, bool known) {
    const float boxW = (r.w - 44.f) * 0.5f;
    const Rect a{ r.x, r.y, boxW, r.h }, b{ r.R() - boxW, r.y, boxW, r.h };
    Kit::FillSlant(ui, a, Kit::Palette::Ghost, 8.f);
    Kit::FillSlant(ui, b, Kit::Palette::Enabled.Fade(known ? 0.16f : 0.06f), 8.f);
    if (known) Kit::StrokeSlant(ui, b, Kit::Palette::Enabled.Fade(0.6f), 1.5f, 8.f);
    TextStyle cap = Kit::Display(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    cap.align = HAlign::Center;
    TextStyle v = Kit::Display(22.f, DWRITE_FONT_WEIGHT_BOLD);
    v.align = HAlign::Center;
    v.valign = VAlign::Center;
    ui.g.Text(Kit::Upper(fromLabel), { a.x, a.y + 7.f, a.w, 14.f }, Kit::Palette::TextDim, cap);
    ui.g.Text(Kit::Upper(toLabel), { b.x, b.y + 7.f, b.w, 14.f }, ChangelogCards::Readable(Kit::Palette::Enabled), cap);
    ui.g.Text(from, { a.x, a.y + 18.f, a.w, a.h - 20.f }, Kit::Palette::TextDim, v);
    ui.g.Text(to, { b.x, b.y + 18.f, b.w, b.h - 20.f }, Kit::Palette::Text, v);
    const float slide = 5.f * std::sin(static_cast<float>(ui.Time()) * 3.f);
    ui.KeepAnimatingThrottled();
    M::DrawIcon(ui.g, M::Icon::ArrowRight, r.CX() + slide * 0.5f, r.CY(), 24.f, Kit::Palette::Enabled);
}

// The ring of a download (0..100) with the percentage inside
void Ring(UI::Ui& ui, UI::Id id, float cx, float cy, float radius, int percent, Color textColor) {
    const float shown = ui.Animate(id, percent / 100.f, 8.f, 0.f);
    ui.g.StrokeEllipse(cx, cy, radius, radius, Kit::Palette::Line, 8.f);
    Arc(ui, cx, cy, radius, 0.f, std::max(2.f, 360.f * shown), Kit::Palette::Enabled, 8.f);
    TextStyle pct = Kit::Display(22.f, DWRITE_FONT_WEIGHT_BOLD, false);
    pct.align = HAlign::Center;
    pct.valign = VAlign::Center;
    ui.g.Text(std::to_wstring(percent) + L"%", { cx - radius, cy - 16.f, radius * 2.f, 32.f }, textColor, pct);
    ui.KeepAnimating();
}

} // namespace

void UpdatesView::Draw(UI::Ui& ui, const Rect& content) {
    MainState& s = main.State();
    // Switching between the overview, the installation and the manual update plays the entrance again
    const int view = s.updates.installView ? 1 : manualView ? 2 : 0;
    if (view != shownView) {
        shownView = view;
        shownAt = -1.0;
        UI::ScrollArea::ScrollTo(ui, ui.MakeId("upd-page"), 0.f);
    }
    if (shownAt < 0.0) shownAt = ui.Time();

    UI::ScrollArea scroll(ui, ui.MakeId("upd-page"), content);
    const float margin = MainLayout::SideMargin;
    const Rect area{ content.x + margin, content.y + 20.f, scroll.ContentWidth() - margin * 2.f, content.h - 40.f };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.45f), 0.f, -24.f);
        const Rect title{ area.x, area.y, area.w, kTitleH };
        const std::wstring installed = Loc::Fmt("updates_installed_pill", Versions::WithV(Versions::Current()));
        if (view == 1)
            TitleRow(ui, title, Loc::Get("updates_ready_title"), Loc::Fmt("updates_ready_subtitle", Versions::WithV(s.updates.latest)),
                     M::Icon::CloudDownload, Versions::WithV(Versions::Current()) + L"  \x2192  " + Versions::WithV(s.updates.latest), Kit::Palette::Enabled);
        else if (view == 2)
            TitleRow(ui, title, Loc::Get("manual_update_title"), Loc::Get("manual_update_subtitle"), M::Icon::CheckCircle, installed, Kit::Palette::Enabled);
        else TitleRow(ui, title, Loc::Get("updates_title"), Loc::Get("updates_subtitle"), M::Icon::CheckCircle, installed, Kit::Palette::Enabled);
    }
    const float top = area.y + kTitleH + kGap;
    const Rect cols{ area.x, top, area.w, area.B() - top };
    const float bottom = view == 1 ? DrawInstallation(ui, cols) : view == 2 ? DrawManual(ui, cols) : DrawOverview(ui, cols);
    scroll.SetContentSize(bottom - content.y + 20.f);
}

// ── Overview ──────────────────────────────────────────────────────────────────────────────────

float UpdatesView::DrawOverview(UI::Ui& ui, const Rect& area) {
    const std::wstring steps[] = { Loc::Get("updates_how_step1"), Loc::Get("updates_how_step2"), Loc::Get("updates_how_step3") };
    const float w = area.w < 680.f ? area.w : kRightW;
    const float versionH = VersionCard(ui, { 0.f, 0.f, w, 0.f }, false), sourcesH = SourcesCard(ui, { 0.f, 0.f, w, 0.f }, false);
    const float howH = StepsCard(ui, { 0.f, 0.f, w, 0.f }, false, Loc::Get("updates_how_title"), steps, 3);
    const float rightH = versionH + sourcesH + howH + 2.f * kGap;
    Columns c = Split(area, 420.f);
    if (!c.stacked) c.left.h = c.right.h = std::max(c.left.h, rightH);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.1, 0, 0.f, 0.5f), 18.f);
        StatusHero(ui, c.left);
    }
    float y = c.right.y;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        VersionCard(ui, { c.right.x, y, c.right.w, versionH }, true);
    }
    y += versionH + kGap;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        SourcesCard(ui, { c.right.x, y, c.right.w, sourcesH }, true);
    }
    y += sourcesH + kGap;
    const float last = c.stacked ? howH : std::max(howH, c.right.B() - y);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.34, 0, 0.f, 0.5f), 0.f, 28.f);
        StepsCard(ui, { c.right.x, y, c.right.w, last }, true, Loc::Get("updates_how_title"), steps, 3);
    }
    return std::max(c.left.B(), y + last);
}

// The big status: the badge (arcs spinning around it while checking, rings when there's news),
// title, message, the new version's details, the actions and the download's progress
void UpdatesView::StatusHero(UI::Ui& ui, const Rect& r) {
    MainState& s = main.State();
    const auto& u = s.updates;
    const float t = static_cast<float>(ui.Time());

    enum class State { Unknown, Checking, UpToDate, Available, Unreachable };
    State state = State::Unknown;
    if (u.checking) state = State::Checking;
    else if (u.available) state = State::Available;
    else if (u.checked) state = u.reachable ? State::UpToDate : State::Unreachable;
    Look look{ Kit::Palette::Disabled, M::Icon::Information };
    std::wstring title, message;
    switch (state) {
        case State::Checking:
            look = { Kit::Palette::Info, M::Icon::CloudSearch };
            title = Loc::Get("updates_checking_title");
            message = Loc::Get("updates_checking_message");
            break;
        case State::UpToDate:
            look = { Kit::Palette::Info, M::Icon::CheckCircle };
            title = Loc::Get("updates_up_to_date_title");
            message = Loc::Fmt("updates_up_to_date_message", Versions::WithV(Versions::Current()));
            break;
        case State::Available:
            look = { Kit::Palette::Enabled, M::Icon::CloudDownload };
            title = Loc::Fmt("updates_available_title", Versions::WithV(u.latest));
            message = Loc::Get("updates_available_message");
            break;
        case State::Unreachable:
            look = { Kit::Palette::Danger, M::Icon::WifiOff };
            title = Loc::Get("updates_unreachable_title");
            message = Loc::Get("updates_unreachable_message");
            break;
        default:
            title = Loc::Get("updates_check_title");
            message = Loc::Get("updates_check_message");
            break;
    }
    Panel(ui, r, look.color);
    const Rect c = r.Inset(Edges(24.f, 20.f));
    const float pop = StatePop(ui, "upd-hero-state", static_cast<int>(state));

    // Heights first, to centre the whole block in the card
    const TextStyle head = HeroTitle(), body = HeroText();
    const float textW = std::min(c.w - 20.f, 520.f);
    const float titleH = std::ceil(ui.g.Measure(Kit::Upper(title), head, textW).h);
    const float bodyH = std::ceil(ui.g.Measure(message, body, textW).h);
    const Kit::ButtonStyle go = Go();
    Kit::ButtonStyle check = Kit::Ghost();
    check.padding = go.padding;
    check.fontSize = go.fontSize;
    const float buttonH = Kit::ButtonHeight(ui, go);
    const bool chips = state == State::Available;
    const float total = 150.f + titleH + 8.f + bodyH + 22.f + (chips ? 52.f + 22.f : 0.f) + buttonH + (u.downloading ? 40.f : 0.f);
    float y = c.y + std::max(0.f, (c.h - 34.f - total) * 0.5f);

    // The badge
    {
        const float cx = c.CX(), cy = y + 70.f;
        if (state == State::Checking) {
            for (int i = 0; i < 2; ++i) Arc(ui, cx, cy, 62.f, t * 300.f + 180.f * static_cast<float>(i), 80.f, look.color, 3.f);
            ui.KeepAnimating();
        }
        float angle = 0.f, bob = 0.f;
        if (state == State::Checking) angle = 10.f * std::sin(t * 6.f);
        if (state == State::Available) bob = 4.f * std::sin(t * 3.f);
        Badge(ui, cx, cy, 84.f, look.color, look.icon, state == State::Available || state == State::UpToDate, pop, angle, bob);
        y += 150.f;
    }
    {
        M::Motion::Enter enter(ui, Anim::EaseOutCubic(pop), 10.f);
        ui.g.Text(Kit::Upper(title), { c.CX() - textW * 0.5f, y, textW, titleH }, Kit::Palette::Text, head);
        y += titleH + 8.f;
        ui.g.Text(message, { c.CX() - textW * 0.5f, y, textW, bodyH }, Kit::Palette::TextDim, body);
        y += bodyH + 22.f;
    }

    // The new version's details
    if (chips) {
        const float chipW = std::min(200.f, (c.w - 16.f) / 3.f);
        const float x0 = c.CX() - (chipW * 3.f + 16.f) * 0.5f;
        M::Motion::Enter enter(ui, Anim::EaseOutCubic(pop), 10.f);
        InfoChip(ui, { x0, y, chipW, 52.f }, M::Icon::StarCircle, Loc::Get("updates_latest_version"), Versions::WithV(u.latest), Kit::Palette::Enabled);
        InfoChip(ui, { x0 + chipW + 8.f, y, chipW, 52.f }, M::Icon::FileDocumentOutline, Loc::Get("updates_file_name"), u.fileName, Kit::Palette::Info);
        InfoChip(ui, { x0 + (chipW + 8.f) * 2.f, y, chipW, 52.f }, M::Icon::Download, Loc::Get("updates_file_size"), u.fileSize, Kit::Palette::Gold);
        y += 52.f + 22.f;
    }

    // Update now, Check for updates
    {
        M::Motion::Enter enter(ui, Anim::EaseOutCubic(pop), 10.f);
        const std::wstring updateText = Loc::Get("updates_update_now");
        const std::wstring checkText = Loc::Get(state == State::Checking ? "updates_checking_button" : "updates_check_button");
        const float uw = chips ? Kit::ButtonWidth(ui, updateText, go) : 0.f;
        const float cw = Kit::ButtonWidth(ui, checkText, check);
        float x = c.CX() - (uw + (chips ? 10.f : 0.f) + cw) * 0.5f;
        if (chips) {
            if (Kit::Button(ui, ui.MakeId("upd-now"), { x, y, uw, buttonH }, updateText, go, !u.downloading)) s.ShowUpdates(true);
            x += uw + 10.f;
        }
        if (Kit::Button(ui, ui.MakeId("upd-check"), { x, y, cw, buttonH }, checkText, check, state != State::Checking && !u.downloading))
            s.CheckForUpdates();
        y += buttonH;
    }

    // The download's progress (started from the installation)
    if (u.downloading) {
        const float barW = std::min(420.f, c.w - 40.f);
        Progress(ui, { c.CX() - barW * 0.5f, y + 18.f, barW, 8.f }, u.progress / 100.f, Kit::Palette::Enabled);
        TextStyle caption = Kit::Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(u.progress < 100 ? Loc::Fmt("updates_downloading", u.progress) : Loc::Get("updates_installing"), { c.x, y + 30.f, c.w, 16.f },
                  Kit::Palette::TextDim, caption);
    }

    // The manual update, at the bottom
    if (Link(ui, ui.MakeId("upd-manual"), { c.x, c.B() - 26.f, c.w, 26.f }, M::Icon::Wrench, Loc::Get("updates_manual"))) OpenManual();
}

// Your version, big, with the update state under it
float UpdatesView::VersionCard(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kH = kPad * 2.f + kHeadingH + 8.f + 46.f + 6.f + 18.f;
    if (!draw) return kH;
    const auto& u = main.State().updates;
    Panel(ui, r, Kit::Palette::Accent);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, Loc::Get("updates_your_version"), Kit::Palette::Accent);
    TextStyle big = Kit::Display(42.f, DWRITE_FONT_WEIGHT_BOLD);
    big.valign = VAlign::Center;
    ui.g.Text(Versions::WithV(Versions::Current()), { c.x, c.y + kHeadingH + 8.f, c.w, 46.f }, Kit::Palette::Text, big);
    const Color dot = u.available ? Kit::Palette::Enabled : u.checked && u.reachable ? Kit::Palette::Info : Kit::Palette::Disabled;
    const char* key = u.available ? "updates_state_newer" : u.checked ? (u.reachable ? "updates_state_latest" : "updates_state_unreachable") : "updates_state_not_checked";
    const float k = static_cast<float>(std::fmod(ui.Time(), 1.6) / 1.6);
    const float dy = c.B() - 9.f;
    ui.g.StrokeEllipse(c.x + 6.f, dy, 4.f + 6.f * k, 4.f + 6.f * k, dot.Fade(0.6f * (1.f - k)), 1.5f);
    ui.g.FillEllipse(c.x + 6.f, dy, 4.f, 4.f, dot);
    ui.KeepAnimatingThrottled();
    TextStyle tiny = Kit::Style(12.f);
    tiny.valign = VAlign::Center;
    ui.g.Text(Loc::Get(key), { c.x + 18.f, dy - 9.f, c.w - 18.f, 18.f }, Kit::Palette::TextDim, tiny);
    return kH;
}

// The releases page and the changelog, as clickable rows
float UpdatesView::SourcesCard(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kRowH = 48.f;
    constexpr float kH = kPad * 2.f + kHeadingH + 10.f + 2.f * kRowH + 6.f;
    if (!draw) return kH;
    MainState& s = main.State();
    Panel(ui, r, Kit::Palette::Accent);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, Loc::Get("updates_sources"), Kit::Palette::Accent);
    float y = c.y + kHeadingH + 10.f;
    const auto Row = [&](const char* key, const std::wstring& name, const std::wstring& hint, M::Icon icon, Color tint) {
        const Rect row{ c.x, y, c.w, kRowH };
        y += kRowH + 6.f;
        const UI::Id id = ui.MakeId(key);
        const UI::Interaction it = ui.Interact(id, row);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
        Kit::FillSlant(ui, row, Lerp(Kit::Palette::Ghost, tint.Fade(0.16f), hover), 8.f);
        const Rect tile{ row.x + 10.f, row.y + 8.f, 40.f, row.h - 16.f };
        Kit::FillSlant(ui, tile, tint, 6.f);
        M::DrawIcon(ui.g, icon, tile.CX(), tile.CY(), 18.f, Colors::White);
        TextStyle n = Kit::Display(14.f, DWRITE_FONT_WEIGHT_BOLD);
        n.ellipsis = true;
        TextStyle h = Kit::Style(11.f);
        h.ellipsis = true;
        ui.g.Text(Kit::Upper(name), { row.x + 60.f, row.y + 6.f, row.w - 100.f, 20.f }, Kit::Palette::Text, n);
        ui.g.Text(hint, { row.x + 60.f, row.y + 26.f, row.w - 100.f, 16.f }, Kit::Palette::TextDim, h);
        M::DrawIcon(ui.g, M::Icon::ChevronRight, row.R() - 22.f + 4.f * hover, row.CY(), 20.f, Lerp(Kit::Palette::TextFaint, tint, hover));
        return it.clicked;
    };
    if (Row("upd-src-releases", Loc::Get("updates_source_releases"), Loc::Get("updates_source_releases_hint"), M::Icon::Web, Kit::Palette::Info))
        Process::OpenUrl(Services::Updates::ReleasesUrl());
    if (Row("upd-src-changelog", Loc::Get("changelogs_title"), Loc::Get("updates_source_changelog_hint"), M::Icon::ClipboardTextClockOutline,
            Kit::Palette::Accent))
        s.view = MainState::View::Changelog;
    return kH;
}

float UpdatesView::StepsCard(UI::Ui& ui, const Rect& r, bool draw, const std::wstring& title, const std::wstring* steps, int count) {
    const float stepsH = Steps(ui, 0.f, 0.f, r.w - kPad * 2.f, steps, count, false, shownAt);
    const float h = kPad * 2.f + kHeadingH + 14.f + stepsH;
    if (!draw) return h;
    Panel(ui, r, Kit::Palette::Accent);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, title, Kit::Palette::Accent);
    Steps(ui, c.x, c.y + kHeadingH + 14.f, c.w, steps, count, true, shownAt);
    return h;
}

// ── Installation ──────────────────────────────────────────────────────────────────────────────

float UpdatesView::DrawInstallation(UI::Ui& ui, const Rect& area) {
    MainState& s = main.State();
    const std::wstring steps[] = { Loc::Get("updates_next_step1"), Loc::Get("updates_next_step2"), Loc::Get("updates_next_step3") };
    const float w = area.w < 680.f ? area.w : kRightW;
    const float installH = InstallCard(ui, { 0.f, 0.f, w, 0.f }, false);
    const float nextH = StepsCard(ui, { 0.f, 0.f, w, 0.f }, false, Loc::Get("updates_next_title"), steps, 3);
    Columns c = Split(area, 380.f);
    if (!c.stacked) c.left.h = c.right.h = std::max(c.left.h, installH + kGap + nextH);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.1, 0, 0.f, 0.5f), 18.f);
        Panel(ui, c.left, Kit::Palette::Enabled);
        news.Draw(ui, c.left.Inset(kPad), s.updates.changelog, Versions::Current(), s.updates.latest);
    }
    float y = c.right.y;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        InstallCard(ui, { c.right.x, y, c.right.w, installH }, true);
    }
    y += installH + kGap;
    const float last = c.stacked ? nextH : std::max(nextH, c.right.B() - y);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        StepsCard(ui, { c.right.x, y, c.right.w, last }, true, Loc::Get("updates_next_title"), steps, 3);
    }
    return std::max(c.left.B(), y + last);
}

// From → to, the file, Download & Install / Cancel, or the progress ring while downloading
float UpdatesView::InstallCard(UI::Ui& ui, const Rect& r, bool draw) {
    const Kit::ButtonStyle go = Go();
    const float buttonH = Kit::ButtonHeight(ui, go);
    const float h = kPad * 2.f + kHeadingH + 12.f + 60.f + 12.f + 52.f + 8.f + 52.f + 18.f + buttonH + 8.f + buttonH;
    if (!draw) return h;
    MainState& s = main.State();
    const auto& u = s.updates;
    Panel(ui, r, Kit::Palette::Enabled);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, Loc::Get("updates_install_from_github"), Kit::Palette::Enabled);
    float y = c.y + kHeadingH + 12.f;
    FromTo(ui, { c.x, y, c.w, 60.f }, Loc::Get("updates_installed"), Versions::WithV(Versions::Current()), Loc::Get("updates_new"),
           Versions::WithV(u.latest), true);
    y += 60.f + 12.f;
    InfoChip(ui, { c.x, y, c.w, 52.f }, M::Icon::FileDocumentOutline, Loc::Get("updates_file_name"), u.fileName, Kit::Palette::Info);
    y += 52.f + 8.f;
    InfoChip(ui, { c.x, y, c.w, 52.f }, M::Icon::Download, Loc::Get("updates_file_size"), u.fileSize, Kit::Palette::Gold);
    y += 52.f + 18.f;

    if (u.downloading) {
        const float radius = std::min(40.f, (c.B() - y) * 0.5f - 12.f);
        const float cy = y + radius + 4.f;
        Ring(ui, ui.MakeId("upd-install-ring"), c.CX(), cy, radius, u.progress, Kit::Palette::Text);
        TextStyle caption = Kit::Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(u.progress < 100 ? Loc::Fmt("updates_downloading", u.progress) : Loc::Get("updates_installing"),
                  { c.x, cy + radius + 8.f, c.w, 16.f }, Kit::Palette::TextDim, caption);
        return h;
    }
    if (Kit::Button(ui, ui.MakeId("upd-accept"), { c.x, y, c.w, buttonH }, Loc::Get("updates_download_install"), go)) s.InstallUpdate();
    y += buttonH + 8.f;
    Kit::ButtonStyle cancel = Kit::Ghost();
    cancel.padding = go.padding;
    cancel.fontSize = go.fontSize;
    if (Kit::Button(ui, ui.MakeId("upd-cancel"), { c.x, y, c.w, buttonH }, Loc::Get("cancel"), cancel)) s.updates.installView = false;
    return h;
}

// ── Manual update ─────────────────────────────────────────────────────────────────────────────

void UpdatesView::OpenManual() {
    manualView = true;
    if (manual != Manual::Downloading) ManualCheck();
}

void UpdatesView::ManualCheck() {
    manual = Manual::Checking;
    manualRelease.reset();
    Async::Run([] { return Services::Updates::LatestReleaseInfo(); }, [this](Services::Updates::UpdateInfo info) {
        manual = Blank(info.downloadUrl) ? Manual::Unreachable : Manual::Ready;
        manualRelease = std::move(info);
    });
}

void UpdatesView::ManualDownload() {
    if (!manualRelease || manual == Manual::Downloading) return;
    if (main.State().busy) {
        Dialogs::Message(Loc::Get("updates_wait_install"), Loc::Get("updates_title_plain"), Dialogs::MessageButtons::Ok,
                         Dialogs::MessageIcon::Information);
        return;
    }
    manual = Manual::Downloading;
    manualProgress = 0;
    const Services::Updates::UpdateInfo info = *manualRelease;
    Async::Run(
        [this, info] {
            return Services::Updates::DownloadAndInstall(info, [this](int percent) { Async::Post([this, percent] { manualProgress = percent; }); });
        },
        [this](bool success) {
            if (!success) manual = Manual::Failed;
        });
}

float UpdatesView::DrawManual(UI::Ui& ui, const Rect& area) {
    const std::wstring steps[] = { Loc::Get("manual_update_when1"), Loc::Get("manual_update_when2"), Loc::Get("manual_update_when3") };
    const float w = area.w < 680.f ? area.w : kRightW;
    const float releaseH = ReleaseCard(ui, { 0.f, 0.f, w, 0.f }, false);
    const float whenH = StepsCard(ui, { 0.f, 0.f, w, 0.f }, false, Loc::Get("manual_update_when_title"), steps, 3);
    Columns c = Split(area, 440.f);
    if (!c.stacked) c.left.h = c.right.h = std::max(c.left.h, releaseH + kGap + whenH);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.1, 0, 0.f, 0.5f), 18.f);
        ManualHero(ui, c.left);
    }
    float y = c.right.y;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        ReleaseCard(ui, { c.right.x, y, c.right.w, releaseH }, true);
    }
    y += releaseH + kGap;
    const float last = c.stacked ? whenH : std::max(whenH, c.right.B() - y);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        StepsCard(ui, { c.right.x, y, c.right.w, last }, true, Loc::Get("manual_update_when_title"), steps, 3);
    }
    return std::max(c.left.B(), y + last);
}

// Back, the badge (searching arcs / bouncing cloud / shaking alert / the progress ring), title,
// message, the release's details and the actions
void UpdatesView::ManualHero(UI::Ui& ui, const Rect& r) {
    const float t = static_cast<float>(ui.Time());
    const bool error = manual == Manual::Unreachable || manual == Manual::Failed;
    const Color color = error ? Kit::Palette::Danger : manual == Manual::Checking ? Kit::Palette::Info : Kit::Palette::Enabled;
    Panel(ui, r, color);
    const Rect c = r.Inset(Edges(24.f, 20.f));
    double since = 0.0;
    const float pop = StatePop(ui, "upd-manual-state", static_cast<int>(manual), &since);
    const float elapsed = static_cast<float>(ui.Time() - since);

    // Back to the overview (not while it's installing)
    {
        Kit::ButtonStyle back = Kit::Ghost();
        back.fontSize = 12.f;
        back.padding = Edges(12.f, 5.f);
        const std::wstring text = L"\x2190  " + Loc::Get("manual_update_back");
        const float bw = Kit::ButtonWidth(ui, text, back);
        if (Kit::Button(ui, ui.MakeId("upd-manual-back"), { c.x, c.y, bw, Kit::ButtonHeight(ui, back) }, text, back, manual != Manual::Downloading))
            manualView = false;
    }

    std::wstring title, message;
    switch (manual) {
        case Manual::Checking: title = Loc::Get("manual_update_checking"); message = Loc::Get("manual_update_checking_message"); break;
        case Manual::Ready:
            title = Loc::Fmt("manual_update_ready", Versions::WithV(manualRelease->latestVersion));
            message = Loc::Get("manual_update_ready_message");
            break;
        case Manual::Unreachable: title = Loc::Get("manual_update_unreachable_title"); message = Loc::Get("manual_update_unreachable_message"); break;
        case Manual::Downloading:
            title = manualProgress < 100 ? Loc::Fmt("updates_downloading", manualProgress) : Loc::Get("updates_installing");
            message = Loc::Get("updates_how_step3");
            break;
        case Manual::Failed: title = Loc::Get("manual_update_failed_title"); message = Loc::Get("manual_update_failed_message"); break;
    }
    const TextStyle head = HeroTitle(), body = HeroText();
    const float textW = std::min(c.w - 20.f, 520.f);
    const float titleH = std::ceil(ui.g.Measure(Kit::Upper(title), head, textW).h);
    const float bodyH = std::ceil(ui.g.Measure(message, body, textW).h);
    const Kit::ButtonStyle go = Go();
    const float buttonH = Kit::ButtonHeight(ui, go);
    const bool chips = manualRelease && !Blank(manualRelease->downloadUrl);
    const float total = 150.f + titleH + 8.f + bodyH + 22.f + (chips ? 52.f + 22.f : 0.f) + buttonH;
    float y = c.y + 34.f + std::max(0.f, (c.h - 34.f - total) * 0.5f);

    // The badge
    {
        const float cx = c.CX(), cy = y + 70.f;
        const float shake = error && elapsed < 0.5f ? 10.f * (1.f - elapsed / 0.5f) * std::sin(elapsed * 50.f) : 0.f;
        ui.PushTranslate(shake, 0.f);
        if (manual == Manual::Checking) {
            for (int i = 0; i < 2; ++i) Arc(ui, cx, cy, 62.f, t * 300.f + 180.f * static_cast<float>(i), 80.f, color, 3.f);
            ui.KeepAnimating();
        }
        if (manual == Manual::Downloading) {
            Ring(ui, ui.MakeId("upd-manual-ring"), cx, cy, 52.f, manualProgress, Kit::Palette::Text);
        } else {
            M::Icon icon = M::Icon::CloudSearch;
            float angle = 0.f, bob = 0.f;
            if (manual == Manual::Checking) angle = 10.f * std::sin(t * 6.f);
            else if (manual == Manual::Ready) {
                icon = M::Icon::CloudDownload;
                bob = 4.f * std::sin(t * 3.f);
            } else icon = M::Icon::AlertCircle;
            Badge(ui, cx, cy, 84.f, color, icon, manual == Manual::Ready, pop, angle, bob);
        }
        ui.PopTransform();
        y += 150.f;
    }
    {
        M::Motion::Enter enter(ui, Anim::EaseOutCubic(pop), 10.f);
        ui.g.Text(Kit::Upper(title), { c.CX() - textW * 0.5f, y, textW, titleH }, Kit::Palette::Text, head);
        y += titleH + 8.f;
        ui.g.Text(message, { c.CX() - textW * 0.5f, y, textW, bodyH }, Kit::Palette::TextDim, body);
        y += bodyH + 22.f;
    }
    // The release: version, file and size
    if (chips) {
        const float chipW = std::min(200.f, (c.w - 16.f) / 3.f);
        const float x0 = c.CX() - (chipW * 3.f + 16.f) * 0.5f;
        for (int i = 0; i < 3; ++i) {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, since + 0.1, i, 0.07f, 0.35f), 12.f);
            const Rect chip{ x0 + (chipW + 8.f) * static_cast<float>(i), y, chipW, 52.f };
            if (i == 0) InfoChip(ui, chip, M::Icon::StarCircle, Loc::Get("updates_latest_version"), Versions::WithV(manualRelease->latestVersion), Kit::Palette::Enabled);
            else if (i == 1) InfoChip(ui, chip, M::Icon::FileDocumentOutline, Loc::Get("updates_file_name"), manualRelease->fileName, Kit::Palette::Info);
            else
                InfoChip(ui, chip, M::Icon::Download, Loc::Get("updates_file_size"),
                         manualRelease->fileSize > 0 ? Text::FormatBytes(static_cast<unsigned long long>(manualRelease->fileSize)) : Loc::Get("updates_unknown_size"),
                         Kit::Palette::Gold);
        }
        y += 52.f + 22.f;
    }
    // Actions
    if (manual == Manual::Downloading) return;
    M::Motion::Enter enter(ui, Anim::EaseOutCubic(pop), 10.f);
    const std::wstring browser = Loc::Get("manual_update_open_browser");
    const std::wstring primary = Loc::Get(error ? "manual_update_retry" : "updates_download_install");
    Kit::ButtonStyle ghost = Kit::Ghost();
    ghost.padding = go.padding;
    ghost.fontSize = go.fontSize;
    const float pw = Kit::ButtonWidth(ui, primary, go), bw = Kit::ButtonWidth(ui, browser, ghost);
    float x = c.CX() - (pw + 10.f + bw) * 0.5f;
    if (Kit::Button(ui, ui.MakeId("upd-manual-go"), { x, y, pw, buttonH }, primary, go, manual != Manual::Checking)) {
        if (manual == Manual::Unreachable) ManualCheck();
        else if (manual == Manual::Failed) ManualDownload();
        else ManualDownload();
    }
    x += pw + 10.f;
    if (Kit::Button(ui, ui.MakeId("upd-manual-browser"), { x, y, bw, buttonH }, browser, ghost)) {
        const bool hasUrl = manualRelease && !Blank(manualRelease->releaseUrl);
        Process::OpenUrl(hasUrl ? manualRelease->releaseUrl : Services::Updates::ReleasesUrl());
    }
}

// Installed → published, and what downloading it will do
float UpdatesView::ReleaseCard(UI::Ui& ui, const Rect& r, bool draw) {
    const TextStyle text = Kit::Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const bool known = manualRelease && !Blank(manualRelease->latestVersion) && manual != Manual::Unreachable;
    const char* note = "manual_update_note_unknown";
    if (known) {
        if (Versions::IsNewer(Versions::Current(), manualRelease->latestVersion)) note = "manual_update_newer";
        else if (Versions::IsNewer(manualRelease->latestVersion, Versions::Current())) note = "manual_update_older";
        else note = "manual_update_same";
    }
    const std::wstring& noteText = Loc::Get(note);
    const float noteH = std::max(40.f, std::ceil(ui.g.Measure(noteText, text, r.w - kPad * 2.f - 46.f).h) + 18.f);
    const float h = kPad * 2.f + kHeadingH + 12.f + 60.f + 12.f + noteH;
    if (!draw) return h;
    const float t = static_cast<float>(ui.Time());
    Panel(ui, r, Kit::Palette::Enabled);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, Loc::Get("manual_update_release_caps"), Kit::Palette::Enabled);
    float y = c.y + kHeadingH + 12.f;
    const std::wstring published = known ? Versions::WithV(manualRelease->latestVersion)
                                         : manual == Manual::Checking ? std::wstring(1 + static_cast<int>(std::fmod(t * 3.f, 3.f)), L'.')
                                                                     : Loc::Get("updates_not_available");
    if (manual == Manual::Checking) ui.KeepAnimatingThrottled();
    FromTo(ui, { c.x, y, c.w, 60.f }, Loc::Get("updates_installed"), Versions::WithV(Versions::Current()), Loc::Get("manual_update_published"), published,
           known);
    y += 60.f + 12.f;
    const Rect box{ c.x, y, c.w, noteH };
    Kit::FillSlant(ui, box, Kit::Palette::Info.Fade(0.1f), 8.f);
    Kit::EdgeStripe(ui, box, 3.f, Kit::Palette::Info, 8.f);
    M::DrawIcon(ui.g, M::Icon::Information, box.x + 22.f, box.CY(), 18.f, Kit::Palette::Info);
    const float th = std::ceil(ui.g.Measure(noteText, text, box.w - 46.f).h);
    ui.g.Text(noteText, { box.x + 38.f, box.CY() - th * 0.5f, box.w - 46.f, th + 2.f }, Kit::Palette::Body, text);
    return h;
}

} // namespace RivalsForge::Main

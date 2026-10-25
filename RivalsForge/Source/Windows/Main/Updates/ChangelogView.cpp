#include "Windows/Main/Updates/ChangelogView.h"
#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/VersionCompare.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Updates/PageKit.h"

namespace RivalsForge::Main {

namespace {
namespace M = UI::Material;
namespace Kit = ModsKit;
using namespace PageKit;
} // namespace

// Counts the versions and the changes of every "## " section in changelog.txt text
void ChangelogView::Summarize(const std::wstring& text) {
    summarized = text;
    versions = changes = 0;
    latestVersion.clear();
    latestLabel.clear();
    types.clear();
    std::wstring section;
    for (const auto& raw : Text::SplitLines(text)) {
        const std::wstring line = Text::Trim(raw);
        if (line.empty()) continue;
        const size_t space = line.find(L' ');
        const std::wstring first = line.substr(0, space);
        if (!first.empty() && iswdigit(first.front()) && first.find(L'.') != std::wstring::npos) {
            if (versions++ == 0) {
                latestVersion = first;
                latestLabel = space == std::wstring::npos ? L"" : Text::Trim(line.substr(space + 1));
                if (latestLabel.size() >= 2 && latestLabel.front() == L'[' && latestLabel.back() == L']')
                    latestLabel = latestLabel.substr(1, latestLabel.size() - 2);
                if (!latestLabel.empty() && latestLabel.front() == L'|') latestLabel.clear();
            }
            section.clear();
            continue;
        }
        if (line.rfind(L"## ", 0) == 0) {
            section = Text::Trim(line.substr(3));
            continue;
        }
        ++changes;
        const std::wstring title = section.empty() ? L"Other" : section;
        auto it = std::find_if(types.begin(), types.end(), [&](const TypeCount& t) { return Text::Equals(t.title, title); });
        if (it == types.end()) types.push_back({ title, 1 });
        else ++it->count;
    }
    std::stable_sort(types.begin(), types.end(), [](const TypeCount& a, const TypeCount& b) { return a.count > b.count; });
}

void ChangelogView::Draw(UI::Ui& ui, const Rect& content) {
    MainState& s = main.State();
    // The entrance plays each time the page is opened
    const bool shown = s.view == MainState::View::Changelog;
    if (shown && !wasShown) shownAt = ui.Time();
    wasShown = shown;
    if (shownAt < 0.0) shownAt = ui.Time();
    const std::wstring& text = s.updates.changelog.empty() ? Services::Changelog::EmbeddedText() : s.updates.changelog;
    if (text != summarized) Summarize(text);

    UI::ScrollArea scroll(ui, ui.MakeId("chl-page"), content);
    const float margin = MainLayout::SideMargin;
    const Rect area{ content.x + margin, content.y + 20.f, scroll.ContentWidth() - margin * 2.f, content.h - 40.f };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.45f), 0.f, -24.f);
        TitleRow(ui, { area.x, area.y, area.w, kTitleH }, Loc::Get("changelogs_title"), Loc::Get("changelogs_subtitle"), M::Icon::ClipboardTextClockOutline,
                 Loc::Fmt("changelogs_pill", versions, changes), Kit::Palette::Accent);
    }
    const float top = area.y + kTitleH + kGap;
    const Rect cols{ area.x, top, area.w, area.B() - top };
    const float w = cols.w < 680.f ? cols.w : kRightW;
    const float glanceH = Glance(ui, { 0.f, 0.f, w, 0.f }, false), typesH = ByType(ui, { 0.f, 0.f, w, 0.f }, false);
    Columns c = Split(cols, 380.f);
    if (!c.stacked) c.left.h = c.right.h = std::max(c.left.h, glanceH + kGap + typesH);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.1, 0, 0.f, 0.5f), 18.f);
        Panel(ui, c.left, Kit::Palette::Accent);
        cards.DrawHistory(ui, c.left.Inset(kPad), text, Versions::Current());
    }
    float y = c.right.y;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        Glance(ui, { c.right.x, y, c.right.w, glanceH }, true);
    }
    y += glanceH + kGap;
    const float last = c.stacked ? typesH : std::max(typesH, c.right.B() - y);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        ByType(ui, { c.right.x, y, c.right.w, last }, true);
    }
    scroll.SetContentSize(std::max(c.left.B(), y + last) - content.y + 20.f);
}

// Versions and changes (counting up), the installed and the latest version
float ChangelogView::Glance(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kStatH = 64.f, kRowH = 34.f;
    constexpr float kH = kPad * 2.f + kHeadingH + 12.f + kStatH + 12.f + 2.f * kRowH;
    if (!draw) return kH;
    MainState& s = main.State();
    Panel(ui, r, Kit::Palette::Accent);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, Loc::Get("changelogs_glance"), Kit::Palette::Accent);
    float y = c.y + kHeadingH + 12.f;

    // Two big numbers counting up
    const float k = Anim::EaseOutCubic(M::Motion::Stagger(ui, shownAt + 0.3, 0, 0.f, 1.f));
    if (k < 1.f) ui.KeepAnimating();
    const auto Stat = [&](float x, float w, int value, const std::wstring& label, M::Icon icon, Color color) {
        const Rect box{ x, y, w, kStatH };
        Kit::FillSlant(ui, box, color.Fade(0.12f), 8.f);
        Kit::EdgeStripe(ui, box, 3.f, color, 8.f);
        M::DrawIcon(ui.g, icon, box.x + 26.f, box.CY(), 22.f, ChangelogCards::Readable(color));
        TextStyle number = Kit::Display(28.f, DWRITE_FONT_WEIGHT_BOLD);
        number.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(value * k))), { box.x + 48.f, box.y + 4.f, w - 54.f, 34.f }, Kit::Palette::Text, number);
        TextStyle caption = Kit::Display(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        caption.ellipsis = true;
        ui.g.Text(Kit::Upper(label), { box.x + 48.f, box.y + 40.f, w - 54.f, 16.f }, Kit::Palette::TextDim, caption);
    };
    const float half = (c.w - 8.f) * 0.5f;
    Stat(c.x, half, versions, Loc::Get("changelogs_versions"), M::Icon::TrophyAward, Kit::Palette::Accent);
    Stat(c.x + half + 8.f, half, changes, Loc::Get("changelogs_changes"), M::Icon::FileMultiple, Kit::Palette::Info);
    y += kStatH + 12.f;

    // Installed / latest rows
    const auto Row = [&](const std::wstring& label, const std::wstring& value, const std::wstring& tag, Color tagColor) {
        TextStyle name = Kit::Style(12.5f);
        name.valign = VAlign::Center;
        ui.g.Text(label, { c.x, y, 140.f, kRowH }, Kit::Palette::TextDim, name);
        TextStyle v = Kit::Display(17.f, DWRITE_FONT_WEIGHT_BOLD);
        v.valign = VAlign::Center;
        const float vw = std::ceil(ui.g.Measure(value, v).w) + 4.f;
        float x = c.R() - vw;
        if (!tag.empty()) {
            TextStyle t = Kit::Display(10.5f, DWRITE_FONT_WEIGHT_BOLD);
            t.align = HAlign::Center;
            t.valign = VAlign::Center;
            const std::wstring upper = Kit::Upper(tag);
            const float tw = std::ceil(ui.g.Measure(upper, t).w) + 20.f;
            x -= tw + 8.f;
            const Rect pill{ x, y + 7.f, tw, 20.f };
            Kit::FillSlant(ui, pill, tagColor.Fade(0.16f), 6.f);
            Kit::EdgeStripe(ui, pill, 3.f, tagColor, 6.f);
            ui.g.Text(upper, pill.Offset(1.f, 0.f), ChangelogCards::Readable(tagColor), t);
            x = pill.R() + 8.f;
        }
        ui.g.Text(value, { x, y, vw, kRowH }, Kit::Palette::Text, v);
        if (y + kRowH < c.B() - 1.f) ui.g.FillRect({ c.x, y + kRowH, c.w, 1.f }, Kit::Palette::Line);
        y += kRowH;
    };
    Row(Loc::Get("updates_installed"), Versions::WithV(Versions::Current()), Loc::Get("changelog_installed"), Kit::Palette::Enabled);
    std::wstring latest = latestVersion.empty() ? Versions::Current() : latestVersion;
    if (s.updates.available && Versions::IsNewer(latest, s.updates.latest)) latest = s.updates.latest;
    Row(Loc::Get("changelogs_latest"), Versions::WithV(latest), latestLabel.empty() ? Loc::Get("changelog_latest") : latestLabel, Kit::Palette::Accent);
    return kH;
}

// One bar per type of change, growing in turn from the left
float ChangelogView::ByType(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kRowH = 42.f;
    const float h = kPad * 2.f + kHeadingH + 12.f + std::max(1.f, static_cast<float>(types.size())) * kRowH;
    if (!draw) return h;
    Panel(ui, r, Kit::Palette::Accent);
    const Rect c = r.Inset(kPad);
    Heading(ui, c.x, c.y, c.w, Loc::Get("changelogs_by_type"), Kit::Palette::Accent);
    float y = c.y + kHeadingH + 12.f;
    const int most = types.empty() ? 1 : std::max(1, types.front().count);
    TextStyle name = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    TextStyle count = Kit::Display(14.f, DWRITE_FONT_WEIGHT_BOLD, false);
    count.align = HAlign::Right;
    count.valign = VAlign::Center;
    for (size_t i = 0; i < types.size() && y + kRowH <= c.B() + 1.f; ++i) {
        const TypeCount& type = types[i];
        const Color color = ChangelogCards::SectionColor(type.title);
        const float grow = Anim::EaseOutCubic(M::Motion::Stagger(ui, shownAt + 0.4, static_cast<int>(i), 0.08f, 0.7f));
        if (grow < 1.f) ui.KeepAnimating();
        const UI::Id id = ui.MakeId("chl-type", static_cast<int64_t>(i));
        const Rect row{ c.x, y, c.w, kRowH - 4.f };
        const float hover = M::Motion::Hover(ui, id, ui.MouseOver(row), false).hover;
        if (hover > 0.f) Kit::FillSlant(ui, row.Inset(-6.f, 0.f), color.Fade(0.08f * hover), 6.f);
        M::DrawIcon(ui.g, ChangelogCards::SectionIcon(type.title), c.x + 9.f, y + 10.f, 16.f * (1.f + 0.15f * hover), ChangelogCards::Readable(color));
        ui.g.Text(Kit::Upper(ChangelogCards::SectionName(type.title)), { c.x + 24.f, y, c.w - 80.f, 20.f }, Kit::Palette::Text, name);
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(type.count * grow))), { c.R() - 60.f, y, 60.f, 20.f }, ChangelogCards::Readable(color), count);
        const Rect track{ c.x, y + 24.f, c.w, 8.f };
        Kit::FillSlant(ui, track, Kit::Palette::Ghost, 3.f);
        const float bw = track.w * static_cast<float>(type.count) / static_cast<float>(most) * grow;
        if (bw > 0.5f) Kit::FillSlant(ui, { track.x, track.y, std::max(10.f, bw), track.h }, color, 3.f);
        y += kRowH;
    }
    return h;
}

} // namespace RivalsForge::Main

#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/VersionCompare.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainState.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"

// The status bar (was the #0A0A0A border with StatusLabel and the two folder buttons): a slanted tag
// on the left (a check at rest, red and pulsing while something runs), the status message sliding
// in whenever it changes, the progress of an installation as a slanted bar with stripes running
// along it, the version (green and breathing when an update is out: opens the
// Updates page), and the two folder buttons.
namespace RivalsForge::Main::StatusBar {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

// The progress bar: the track, the filled part and diagonal stripes moving through it
void Progress(UI::Ui& ui, const Rect& r, float value) {
    Kit::FillSlant(ui, r, Kit::Palette::Ghost, 4.f);
    Rect fill = r;
    fill.w = std::max(8.f, r.w * std::clamp(value, 0.f, 1.f));
    Kit::FillSlant(ui, fill, Kit::Palette::Accent, 4.f);
    Kit::PushSlantClip(ui, fill, 4.f);
    const float offset = std::fmod(static_cast<float>(ui.Time()) * 30.f, 12.f);
    for (float x = fill.x - 12.f + offset; x < fill.R(); x += 12.f) ui.g.Line(x, fill.B(), x + fill.h, fill.y, Colors::White.Fade(0.18f), 4.f);
    ui.g.PopLayer();
}

// The version: "v1.0.0" (a check once it's the latest, an arrow spinning while checking), or green
// and breathing "UPDATE v1.0.1" when a newer one is out; opens the Updates page. Returns its width.
float VersionChip(UI::Ui& ui, MainState& state, float right, const Rect& inner) {
    const auto& u = state.updates;
    const bool news = u.available;
    TextStyle t = Kit::Display(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.valign = VAlign::Center;
    const std::wstring label = news ? Kit::Upper(Loc::Fmt("status_update_chip", Versions::WithV(u.latest))) : Versions::WithV(Versions::Current());
    const float w = std::ceil(ui.g.Measure(label, t).w) + 44.f;
    const Rect r{ right - w, inner.y, w, inner.h };
    const UI::Id id = ui.MakeId("status-version");
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
    const float tt = static_cast<float>(ui.Time());
    if (news) {
        const float breathe = 0.5f + 0.5f * std::sin(tt * 3.f);
        ui.KeepAnimatingThrottled();
        ui.g.GlowRound(r.Inset(6.f, 2.f), 2.f, Kit::Palette::Enabled, 12.f, 0.25f + 0.3f * breathe);
        Kit::FillSlant(ui, r, Lerp(Hex(0x2FA862), Hex(0x3CC777), std::max(hover, 0.4f * breathe)), 6.f);
        M::DrawIcon(ui.g, M::Icon::CloudDownload, r.x + 18.f, r.CY() + 1.5f * std::sin(tt * 4.f), 14.f, Colors::White);
        ui.g.Text(label, { r.x + 30.f, r.y, r.w - 32.f, r.h }, Colors::White, t);
    } else {
        Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, hover), 6.f);
        Kit::StrokeSlant(ui, r, Kit::Palette::Line, 1.f, 6.f);
        const float cx = r.x + 18.f, cy = r.CY();
        if (u.checking) {
            ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(std::fmod(tt * 360.f, 360.f), D2D1::Point2F(cx, cy)));
            M::DrawIcon(ui.g, M::Icon::Refresh, cx, cy, 13.f, Kit::Palette::TextDim);
            ui.g.PopTransform();
            ui.KeepAnimating();
        } else {
            const bool latest = u.checked && u.reachable;
            M::DrawIcon(ui.g, latest ? M::Icon::CheckCircle : M::Icon::CloudDownload, cx, cy, 13.f, latest ? Kit::Palette::Enabled : Kit::Palette::TextDim);
        }
        ui.g.Text(label, { r.x + 30.f, r.y, r.w - 32.f, r.h }, Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, hover), t);
    }
    ui.Tooltip(id, news ? Loc::Fmt("status_update_tooltip", Versions::WithV(u.latest)) : Loc::Get("status_version_tooltip"));
    if (it.clicked) state.ShowUpdates(news);
    return w;
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar) {
    MainState& state = main.State();
    ui.Block(bar);
    ui.g.FillRect(bar, Kit::Palette::StatusBar);
    ui.g.FillRect({ bar.x, bar.y, bar.w, 1.f }, Kit::Palette::Line);
    const Rect inner = bar.Inset(Edges(MainLayout::SideMargin, 6.f));

    // Folder buttons (OpenModsFolder_Click / OpenPaksFolder_Click), small ghost buttons
    Kit::ButtonStyle folder = Kit::Ghost();
    folder.fontSize = 11.f;
    folder.padding = Edges(10.f, 3.f);
    folder.skew = 6.f;
    const std::wstring modsText = Loc::Get("status_mods_folder"), paksText = Loc::Get("status_paks_folder");
    const std::wstring introsText = Loc::Get("status_intros_folder");
    const float bh = inner.h;
    const float paksW = Kit::ButtonWidth(ui, paksText, folder), modsW = Kit::ButtonWidth(ui, modsText, folder);
    const float introsW = Kit::ButtonWidth(ui, introsText, folder);
    const Rect introsR{ inner.R() - introsW, inner.y, introsW, bh };
    const Rect paksR{ introsR.x - 6.f - paksW, inner.y, paksW, bh };
    const Rect modsR{ paksR.x - 6.f - modsW, inner.y, modsW, bh };
    const UI::Id modsId = ui.MakeId("status-mods-folder"), paksId = ui.MakeId("status-paks-folder");
    if (Kit::Button(ui, modsId, modsR, modsText, folder)) main.Mods().OpenModsFolder();
    ui.Tooltip(modsId, Loc::Get("status_mods_folder_tooltip"));
    if (Kit::Button(ui, paksId, paksR, paksText, folder)) main.Mods().OpenPaksFolder();
    ui.Tooltip(paksId, Loc::Get("status_paks_folder_tooltip"));
    const UI::Id introsId = ui.MakeId("status-intros-folder");
    if (Kit::Button(ui, introsId, introsR, introsText, folder)) main.Mods().OpenIntrosFolder();
    ui.Tooltip(introsId, Loc::Get("status_intros_folder_tooltip"));

    // The version, left of the folders
    const float versionX = modsR.x - 14.f - VersionChip(ui, state, modsR.x - 14.f, inner);

    // The tag: a check at rest, red and breathing while something runs
    const float busy = ui.Tween(ui.MakeId("status-busy"), state.busy ? 1.f : 0.f, 0.25f);
    const float t = static_cast<float>(ui.Time());
    const Rect tag{ inner.x, inner.y + 1.f, 30.f, inner.h - 2.f };
    const float breathe = busy * (0.5f + 0.5f * std::sin(t * 6.f));
    if (busy > 0.f) ui.KeepAnimating();
    Kit::FillSlant(ui, tag, Lerp(Kit::Palette::Ghost, Lerp(Kit::Palette::AccentPressed, Kit::Palette::AccentHover, breathe), busy), 6.f);
    if (busy > 0.5f) {
        // Three dots taking turns
        for (int i = 0; i < 3; ++i) {
            const float k = 0.5f + 0.5f * std::sin(t * 8.f - i * 1.2f);
            ui.g.FillEllipse(tag.CX() - 6.f + i * 6.f, tag.CY(), 1.6f + 0.8f * k, 1.6f + 0.8f * k, Colors::White.Fade(0.5f + 0.5f * k));
        }
    } else {
        Kit::DrawGlyph(ui, Kit::Glyph::Check, tag.CX(), tag.CY(), 12.f, Kit::Palette::Enabled);
    }

    // The progress of a background installation, left of the buttons
    const float barW = 170.f;
    float textRight = versionX - 16.f;
    if (busy > 0.01f) {
        const Rect pr{ versionX - 16.f - barW, inner.CY() - 4.f, barW, 8.f };
        ui.g.PushOpacity(busy);
        Progress(ui, pr, state.progress);
        ui.g.PopLayer();
        textRight = pr.x - 12.f;
    }

    // StatusLabel: a new message slides in from the left
    const std::wstring& text = state.statusMessage;
    auto& shown = ui.Data<std::wstring>(ui.MakeId("status-text"));
    double& changedAt = ui.Data<double>(ui.MakeId("status-changed"));
    if (shown != text) {
        shown = text;
        changedAt = ui.Time();
    }
    const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - changedAt) / 0.35)));
    if (k < 1.f) ui.KeepAnimating();
    TextStyle style = M::Text(11.5f);
    style.valign = VAlign::Center;
    style.ellipsis = true;
    const float x = tag.R() + 10.f;
    ui.g.PushOpacity(k);
    ui.g.Text(text, { x + 10.f * (1.f - k), bar.y, std::max(0.f, textRight - x), bar.h }, Kit::Palette::TextDim, style);
    ui.g.PopLayer();
}

} // namespace RivalsForge::Main::StatusBar

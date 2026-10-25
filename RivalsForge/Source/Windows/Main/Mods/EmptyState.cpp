#include <cmath>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"

// <StackPanel x:Name="EmptyStatePanel">: no mods yet. A slanted dark tile with a box floating
// gently over the halftone, "NO MODS INSTALLED" in condensed italic capitals and the hint, and
// "＋ Add Mod" right there too.
namespace RivalsForge::Main {

namespace {
namespace M = UI::Material;
namespace Kit = ModsKit;
constexpr float kTileW = 120.f, kTileH = 92.f;
} // namespace

void ModsView::EmptyState(UI::Ui& ui, const Rect& r) {
    TextStyle title = Kit::Display(26.f, DWRITE_FONT_WEIGHT_BOLD);
    title.align = HAlign::Center;
    TextStyle hint = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    const std::wstring titleText = Kit::Upper(Loc::Get("empty_title")), hintText = Loc::Get("empty_hint");
    const std::wstring addText = Loc::Get("header_add_mod");
    const Kit::ButtonStyle add = Kit::Primary();
    const float w = std::min(520.f, r.w - 2.f * MainLayout::SideMargin);
    const float x = r.CX() - w * 0.5f;
    const float titleH = Kit::LineH(ui, title), hintH = ui.g.Measure(hintText, hint, w).h, addH = Kit::ButtonHeight(ui, add);
    float y = r.CY() - (kTileH + 22.f + titleH + 6.f + hintH + 22.f + addH) * 0.5f;

    double& shownAt = ui.Data<double>(ui.MakeId("empty-shown"));
    if (shownAt == 0.0) shownAt = ui.Time();
    const auto pop = [&](int i) { return M::Motion::Stagger(ui, shownAt, i, 0.08f, 0.45f); };
    const float t = static_cast<float>(ui.Time());
    ui.KeepAnimatingThrottled();

    {
        M::Motion::Enter enter(ui, pop(0), 14.f);
        const Rect tile{ r.CX() - kTileW * 0.5f, y, kTileW, kTileH };
        Kit::FillSlant(ui, tile, Kit::Palette::Panel, 18.f);
        Kit::PushSlantClip(ui, tile, 18.f);
        Kit::Halftone(ui, tile, 0.07f);
        ui.g.FillLinear(tile, 0.f, { { 0.f, Kit::Palette::Accent.Fade(0.f) }, { 1.f, Kit::Palette::Accent.Fade(0.18f) } }, { 0.f, 0.f }, { 0.f, 1.f });
        ui.g.PopLayer();
        Kit::StrokeSlant(ui, tile, Kit::Palette::Line, 1.f, 18.f);
        const float bob = std::sin(t * 1.8f) * 4.f;
        Kit::DrawGlyph(ui, Kit::Glyph::Box, tile.CX(), tile.CY() + bob, 42.f, Lerp(Kit::Palette::TextFaint, Kit::Palette::TextDim, 0.5f + 0.5f * std::sin(t * 1.8f)));
        y += kTileH + 22.f;
    }
    {
        M::Motion::Enter enter(ui, pop(1), 10.f);
        ui.g.Text(titleText, { x, y, w, titleH }, Kit::Palette::Text, title);
        y += titleH + 6.f;
        ui.g.Text(hintText, { x, y, w, hintH }, Kit::Palette::TextDim, hint);
        y += hintH + 22.f;
    }
    M::Motion::Enter enter(ui, pop(2), 10.f);
    const float addW = Kit::ButtonWidth(ui, addText, add);
    if (Kit::Button(ui, ui.MakeId("empty-add"), { r.CX() - addW * 0.5f, y, addW, addH }, addText, add, !Busy())) AddMod();
}

} // namespace RivalsForge::Main

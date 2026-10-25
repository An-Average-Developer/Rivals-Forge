#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"

// The strip over the list (new), like the game's hero select: "All", then a slanted portrait tile
// per hero with mods (the same order as the cards), its role colour along the foot and its number
// of mods in a tag. A click shows that hero's card alone (again: all of them); the chosen tile
// rises with a red frame, the others dim. Scrolls sideways with the wheel when it doesn't fit.
namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kHeight = 88.f, kTileW = 58.f, kTileH = 62.f, kSkew = 10.f, kGap = 6.f, kTop = 14.f;

// The count in a small slanted tag on the tile's top right, popping when it changes
void Badge(UI::Ui& ui, const Rect& tile, int count, UI::Id id, Color color) {
    const float pop = Kit::Changed(ui, id, count, 0.45f);
    TextStyle t = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD, false);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    const std::wstring text = std::to_wstring(count);
    const float w = std::max(20.f, std::ceil(ui.g.Measure(text, t).w) + 12.f);
    const Rect tag{ tile.R() - w + 4.f, tile.y - 6.f, w, 15.f };
    ui.PushScale(1.f + 0.35f * pop, tag.CX(), tag.CY());
    Kit::FillSlant(ui, tag.Inset(-1.5f), Kit::Palette::Window, 4.f);
    Kit::FillSlant(ui, tag, color, 4.f);
    ui.g.Text(text, tag, Colors::White, t);
    ui.PopTransform();
}

} // namespace

float ModsView::HeroStrip(UI::Ui& ui, const Rect& r) {
    const auto groups = state.Groups();
    // A filter for a hero whose mods are all gone lets go
    if (!state.filter.empty() && std::none_of(groups.begin(), groups.end(), [&](const MainState::Group& g) { return MainState::FilterKey(g.hero) == state.filter; }))
        state.filter.clear();

    const Rect strip{ r.x, r.y, r.w, kHeight };
    // A thin line under it, and the tiles on a horizontal scroll
    ui.g.FillRect({ strip.x, strip.B() - 1.f, strip.w, 1.f }, Kit::Palette::Line);
    UI::ScrollArea scroll(ui, ui.MakeId("hero-strip"), { strip.x, strip.y, strip.w, strip.h - 1.f }, true, 60.f);
    double& shownAt = ui.Data<double>(ui.MakeId("strip-shown"));   // the tiles pop in once
    if (shownAt == 0.0) shownAt = ui.Time();

    float x = strip.x + MainLayout::SideMargin;
    const float top = strip.y + kTop;
    const bool filtering = !state.filter.empty();
    int index = 0;
    const auto item = [&](const std::wstring& hero, bool all, int count) {
        const Rect a{ x, top, kTileW, kTileH };
        x += kTileW + kGap;
        const float in = M::Motion::Stagger(ui, shownAt + 0.1, index++, 0.035f, 0.35f, 30);
        const UI::Id id = ui.MakeId("strip-item", all ? -1 : Key(hero));
        const UI::Interaction it = ui.Interact(id, a);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const bool chosen = all ? !filtering : state.filter == MainState::FilterKey(hero);
        const float sel = ui.Tween(UI::HashId(id, 0x5E1), chosen ? 1.f : 0.f, 0.2f);
        const float dim = filtering && !chosen ? 0.4f + 0.6f * fb.hover : 1.f;
        const Color role = all ? Kit::Palette::Accent : Kit::HeroColor(hero);

        UI::Material::Motion::Enter enter(ui, in, 12.f);
        ui.PushTranslate(0.f, -4.f * std::max(fb.hover, sel) + 1.5f * fb.press);
        if (sel > 0.f) ui.g.GlowRound(a.Inset(kSkew * 0.5f, 0.f), 2.f, Kit::Palette::Accent, 14.f, 0.6f * Kit::GlowStrength() * sel);
        else if (fb.hover > 0.f) ui.g.GlowRound(a.Inset(kSkew * 0.5f, 0.f), 2.f, role, 12.f, 0.35f * Kit::GlowStrength() * fb.hover);
        ui.g.PushOpacity(dim);
        if (all) {
            // "All": the Marvel red with the word and the halftone
            Kit::FillSlant(ui, a, Lerp(Kit::IsLight() ? Hex(0xE8A3B0) : Hex(0x2A1015), Kit::Palette::Accent, 0.35f + 0.65f * std::max(sel, fb.hover * 0.6f)), kSkew);
            Kit::PushSlantClip(ui, a, kSkew);
            Kit::Halftone(ui, a, 0.12f);
            ui.g.PopLayer();
            TextStyle t = Kit::Display(15.f, DWRITE_FONT_WEIGHT_BOLD);
            t.align = HAlign::Center;
            t.valign = VAlign::Center;
            ui.g.Text(Kit::Upper(Loc::Get("strip_all")), a, Colors::White, t);
        } else {
            Kit::SlantPortrait(ui, hero, a, kSkew, 1.1f + 0.1f * fb.hover);
            // The role colour along the foot
            Kit::PushSlantClip(ui, a, kSkew);
            ui.g.FillLinear(a, 0.f, { { 0.6f, role.Fade(0.f) }, { 1.f, role.Fade(0.65f) } }, { 0.f, 0.f }, { 0.f, 1.f });
            ui.g.FillRect({ a.x, a.B() - 3.f, a.w, 3.f }, role);
            ui.g.PopLayer();
        }
        ui.g.PopLayer();
        // The frame: red and thicker when chosen
        if (sel > 0.f || fb.hover > 0.f)
            Kit::StrokeSlant(ui, a, Lerp(Colors::White.Fade(0.5f * fb.hover), Kit::Palette::AccentHover, sel), 1.f + 1.5f * sel, kSkew);
        Badge(ui, a, count, id, chosen ? Kit::Palette::Accent : Kit::IsLight() ? Hex(0x6A6A76) : Hex(0x2E2E36));
        ui.PopTransform();
        ui.Tooltip(id, all ? Loc::Get("strip_all_tooltip") : Kit::HeroName(hero));
        if (it.clicked) state.filter = all || chosen ? L"" : MainState::FilterKey(hero);
    };

    item(L"", true, state.TotalMods());
    for (const auto& group : groups) item(group.hero, false, static_cast<int>(group.modIds.size()));
    scroll.SetContentSize(x - strip.x - kGap + MainLayout::SideMargin);
    return kHeight;
}

} // namespace RivalsForge::Main

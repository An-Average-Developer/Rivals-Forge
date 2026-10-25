#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Data/Heroes.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Overlays/HeroPickerOverlay.h"
#include "Windows/Main/Store/StoreView.h"

// The store's toolbar: the search box (searches once the typing stops), the order as slanted chips
// with a highlight sliding to the chosen one, and the hero filter (the hero picker; the hero's
// portrait on the button while one is chosen, a cross to drop it). How many mods match on the right.
namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;
using Services::Store::Sort;

constexpr float kHeight = 64.f, kControlH = 34.f;
const char* const kSortKeys[] = { "store_sort_popular", "store_sort_downloads", "store_sort_newest", "store_sort_updated" };

} // namespace

float StoreView::ChipWidth(UI::Ui& ui, const std::wstring& text) const {
    TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    return std::ceil(ui.g.Measure(Kit::Upper(text), t).w) + 34.f;
}

bool StoreView::Chip(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool on) {
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    if (!on) Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, fb.hover), 8.f);
    TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    ui.g.Text(Kit::Upper(text), r, on ? Colors::White : Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, fb.hover), t);
    return it.clicked;
}

void StoreView::HeroFilter(UI::Ui& ui, const Rect& r) {
    const UI::Id id = ui.MakeId("store-hero");
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Color role = hero.empty() ? Kit::Palette::TextDim : Kit::HeroColor(hero);
    Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, fb.hover), 8.f);
    Kit::StrokeSlant(ui, r, Lerp(Kit::Palette::Line, role.Fade(0.8f), hero.empty() ? fb.hover : 1.f), 1.f, 8.f);
    float x = r.x + 12.f;
    if (!hero.empty()) {
        Kit::SlantPortrait(ui, hero, { r.x + 6.f, r.y + 3.f, 34.f, r.h - 6.f }, 6.f, 1.15f);
        x = r.x + 46.f;
    } else {
        Kit::DrawGlyph(ui, Kit::Glyph::Person, x + 8.f, r.CY(), 15.f, Kit::Palette::TextDim);
        x += 22.f;
    }
    TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.valign = VAlign::Center;
    t.ellipsis = true;
    ui.g.Text(Kit::Upper(hero.empty() ? Loc::Get("store_hero_all") : Kit::HeroName(hero)), { x, r.y, r.R() - x - 30.f, r.h },
              hero.empty() ? Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, fb.hover) : Kit::Palette::Text, t);
    Kit::Chevron(ui, r.R() - 18.f, r.CY(), 9.f, 0.f, Kit::Palette::TextDim);
    ui.Tooltip(id, Loc::Get("store_hero_tooltip"));
    if (it.clicked)
        main.HeroPicker().Show(Loc::Get("store_hero_picker"), hero, [this](std::optional<std::wstring> picked) {
            if (!picked || *picked == hero) return;
            hero = *picked;   // "No character": every mod again
            Load(false);
        });
}

float StoreView::Toolbar(UI::Ui& ui, const Rect& r) {
    const Rect bar{ r.x, r.y, r.w, kHeight };
    ui.g.FillRect({ bar.x, bar.B() - 1.f, bar.w, 1.f }, Kit::Palette::Line);
    const float y = bar.CY() - kControlH * 0.5f;
    float x = bar.x + MainLayout::SideMargin;

    // Search
    {
        const float w = std::clamp(r.w * 0.3f, 200.f, 300.f);
        UI::TextBoxStyle box = Kit::InputBoxStyle(13.f);
        box.placeholder = Loc::Get("store_search");
        box.padding = Edges(34.f, 0.f, 10.f, 0.f);
        const std::wstring before = search;
        UI::TextBox(ui, ui.MakeId("store-search"), { x, y, w, kControlH }, search, box);
        Kit::DrawGlyph(ui, Kit::Glyph::Search, x + 17.f, y + kControlH * 0.5f, 16.f, Kit::Palette::TextDim);
        if (search != before) {
            searchChangedAt = ui.Time();
            searchPending = true;
        }
        x += w + 16.f;
    }

    // The order: chips with a highlight sliding under the chosen one
    {
        float widths[4];
        for (int i = 0; i < 4; ++i) widths[i] = ChipWidth(ui, Loc::Get(kSortKeys[i]));
        const int chosen = static_cast<int>(sort);
        float chosenX = x;
        for (int i = 0; i < chosen; ++i) chosenX += widths[i] + 4.f;
        const float hx = ui.Animate(ui.MakeId("store-sort-x"), chosenX, 18.f);
        const float hw = ui.Animate(ui.MakeId("store-sort-w"), widths[chosen], 18.f);
        if (std::fabs(hx - chosenX) > 0.3f || std::fabs(hw - widths[chosen]) > 0.3f) ui.KeepAnimating();
        if (!Kit::IsLight()) ui.g.GlowRound({ hx + 6.f, y, hw - 12.f, kControlH }, 2.f, Kit::Palette::Accent, 10.f, 0.35f);
        Kit::FillSlant(ui, { hx, y, hw, kControlH }, Kit::Palette::Accent, 8.f);
        float cx = x;
        for (int i = 0; i < 4; ++i) {
            if (Chip(ui, ui.MakeId("store-sort", i), { cx, y, widths[i], kControlH }, Loc::Get(kSortKeys[i]), i == chosen) && i != chosen) {
                sort = static_cast<Sort>(i);
                Load(false);
            }
            cx += widths[i] + 4.f;
        }
        x = cx + 12.f;
    }

    // The hero
    {
        const float w = 190.f;
        if (x + w <= bar.R() - MainLayout::SideMargin) HeroFilter(ui, { x, y, w, kControlH });
        if (!hero.empty()) {
            // A cross to drop the filter
            const Rect clear{ x + w + 4.f, y, kControlH, kControlH };
            if (Kit::IconButton(ui, ui.MakeId("store-hero-clear"), clear, Kit::Glyph::Plus, Kit::Palette::Danger, Loc::Get("store_hero_clear"))) {
                hero.clear();
                Load(false);
            }
        }
    }

    // How many
    if (loadedOnce && error.empty()) {
        TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        t.align = HAlign::Right;
        t.valign = VAlign::Center;
        const std::wstring count = Kit::Upper(Loc::Fmt(total == 1 ? "store_count_one" : "store_count_many", Number(total, false)));
        const float w = std::ceil(ui.g.Measure(count, t).w);
        if (bar.R() - MainLayout::SideMargin - w > x + 240.f)
            ui.g.Text(count, { bar.R() - MainLayout::SideMargin - w - 4.f, bar.y, w + 4.f, bar.h }, Kit::Palette::TextDim, t);
    }
    return kHeight;
}

} // namespace RivalsForge::Main

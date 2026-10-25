#include <cmath>
#include "Data/Heroes.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"

// A hero's card (was the SECTION CARD), in the game's style: the portrait in a slanted frame on the
// left, the name in big condensed italic capitals with the role in a slanted tag, "N mods · M
// enabled", the comic halftone and the name again as a huge faint watermark behind; a line of the
// role colour along the top that runs out on hover. The header (or the chevron) folds the card;
// under it its mods, unfolding.
namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kRadius = 4.f, kHeaderH = 92.f, kPortraitW = 132.f, kPortraitSkew = 22.f;
constexpr float kBodyPad = 10.f, kBodySide = 12.f, kModGap = 6.f;

TextStyle EmptyStyle() { return Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center); }

// The role in a slanted tag of its colour ("VANGUARD")
float RoleTag(UI::Ui& ui, float x, float cy, const std::wstring& text, Color color) {
    TextStyle t = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    t.align = HAlign::Center;
    const float w = std::ceil(ui.g.Measure(text, t).w) + 22.f;
    const Rect tag{ x, cy - 10.f, w, 20.f };
    Kit::FillSlant(ui, tag, color.Fade(0.22f), 6.f);
    Kit::EdgeStripe(ui, tag, 4.f, color, 6.f);
    ui.g.Text(text, tag.Offset(2.f, 0.f), Lerp(color, Kit::IsLight() ? Colors::Black : Colors::White, Kit::IsLight() ? 0.3f : 0.45f), t);
    return w;
}

} // namespace

float ModsView::HeroCardHeight(UI::Ui& ui, const HeroRow& row, float open, double now) {
    float content = 0.f;
    int live = 0;
    for (const auto& m : row.mods) {
        content += (ModCardHeight(ui) + kModGap) * Appear(m.shownAt, now) * (1.f - Leave(m.hiddenAt, now));
        if (m.hiddenAt < 0.0) ++live;
    }
    if (live == 0) content += 6.f + Kit::LineH(ui, EmptyStyle()) + 6.f;
    return kHeaderH + (kBodyPad + content + kBodyPad - kModGap) * open;
}

void ModsView::HeroCard(UI::Ui& ui, const Rect& r, const HeroRow& row, float open, bool interactive, double now) {
    const int64_t key = Key(row.hero);
    const Data::Hero* hero = Data::FindHero(row.hero);
    const Color role = Kit::HeroColor(row.hero);
    const bool muted = ui.InputMuted();
    if (!interactive) ui.SetInputMuted(true);

    // The header: hover zooms the portrait and lights the card; a click folds the card
    const Rect header{ r.x, r.y, r.w, kHeaderH };
    const UI::Id headerId = ui.MakeId("hero-header", key);
    const UI::Interaction headerIt = ui.Interact(headerId, header, true, false);
    if (headerIt.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, headerId, headerIt.hovered, headerIt.held);

    ui.g.PushRoundClip(r, kRadius);
    ui.g.FillRect(r, Kit::Palette::Panel);
    // The header's backdrop: the halftone fading in from the right, the role colour from the left
    // over it (drawn after the dots' fade, which would otherwise cut the colour off where it starts)
    {
        const Rect dots{ header.R() - 260.f, header.y, 260.f, header.h };
        ui.g.PushOpacity(0.5f + 0.5f * fb.hover);
        Kit::Halftone(ui, dots, 0.05f);
        ui.g.PopLayer();
        ui.g.FillLinear(dots, 0.f, { { 0.f, Kit::Palette::Panel }, { 1.f, Kit::Palette::Panel.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    }
    ui.g.FillLinear(header, 0.f, { { 0.f, role.Fade((Kit::IsLight() ? 0.22f : 0.14f) + 0.08f * fb.hover) }, { 0.6f, role.Fade(Kit::IsLight() ? 0.05f : 0.02f) }, { 1.f, role.Fade(0.f) } },
                    { 0.f, 0.f }, { 1.f, 0.f });
    // The name as a watermark, drifting a little on hover
    {
        TextStyle mark = Kit::Display(84.f, DWRITE_FONT_WEIGHT_BOLD);
        mark.align = HAlign::Right;
        mark.valign = VAlign::Center;
        ui.g.PushClip(header);
        ui.g.Text(Kit::Upper(Kit::HeroName(row.hero)), { header.x, header.y - 4.f, header.w - 70.f + 12.f * fb.hover, header.h },
                  Kit::IsLight() ? Lerp(Kit::Palette::Shade, role, 0.35f).Fade(0.14f) : Kit::Palette::Shade.Fade(0.025f), mark);
        ui.g.PopClip();
    }

    // The portrait in its slanted frame, its role colour behind it
    const Rect frame{ r.x - kPortraitSkew, r.y, kPortraitW + kPortraitSkew, kHeaderH };
    Kit::FillSlant(ui, frame.Offset(6.f, 0.f), role.Fade(0.55f + 0.3f * fb.hover), kPortraitSkew);
    Kit::SlantPortrait(ui, row.hero, frame, kPortraitSkew, 1.f + 0.08f * fb.hover);
    {
        Kit::PushSlantClip(ui, frame, kPortraitSkew);
        ui.g.FillLinear(frame, 0.f, { { 0.55f, Colors::Black.Fade(0.f) }, { 1.f, Colors::Black.Fade(0.55f) } }, { 0.f, 0.f }, { 0.f, 1.f });
        ui.g.PopLayer();
    }

    // The top line of the role colour, running out to the right on hover
    const float line = Anim::EaseOutCubic(fb.hover);
    ui.g.FillRect({ r.x, r.y, r.w * (0.18f + 0.82f * line), 2.f }, role);

    // The fold chevron on the right
    const Rect foldR{ header.R() - 16.f - 34.f, header.CY() - 15.f, 34.f, 30.f };
    bool fold = headerIt.clicked;
    {
        const UI::Id id = ui.MakeId("hero-fold", key);
        const UI::Interaction it = ui.Interact(id, foldR);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback ffb = M::Motion::Hover(ui, id, it.hovered, it.held);
        Kit::FillSlant(ui, foldR, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, std::max(ffb.hover, fb.hover * 0.5f)), 6.f);
        Kit::Chevron(ui, foldR.CX(), foldR.CY(), 10.f, open, Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, std::max(ffb.hover, fb.hover)));
        ui.Tooltip(id, Loc::Get("hero_fold_tooltip"));
        fold = fold || it.clicked;
    }
    if (fold) state.ToggleExpanded(row.hero);

    // Name, role and counts
    {
        const float x = frame.R() + 10.f + 4.f * fb.hover;
        const float w = std::max(0.f, foldR.x - 12.f - x);
        TextStyle name = Kit::Display(27.f, DWRITE_FONT_WEIGHT_BOLD);
        name.ellipsis = true;
        const float nameH = Kit::LineH(ui, name);
        const float y = header.CY() - (nameH + 4.f + 20.f) * 0.5f - 2.f;
        ui.g.Text(Kit::Upper(Kit::HeroName(row.hero)), { x, y, w, nameH }, Kit::Palette::Text, name);
        int live = 0, enabled = 0;
        for (const auto& m : row.mods)
            if (m.hiddenAt < 0.0) {
                ++live;
                enabled += m.mod.isEnabled ? 1 : 0;
            }
        float px = x;
        const float cy = y + nameH + 4.f + 10.f;
        if (hero) px += RoleTag(ui, px, cy, Kit::Upper(Loc::Get(Data::RoleKey(hero->role))), role) + 12.f;
        TextStyle count = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false);
        count.valign = VAlign::Center;
        count.ellipsis = true;
        const std::wstring mods = Loc::Fmt(live == 1 ? "hero_mod_count_one" : "hero_mod_count_many", live);
        const std::wstring on = Loc::Fmt("hero_enabled_count", enabled);
        const float mw = std::ceil(ui.g.Measure(mods, count).w);
        ui.g.Text(mods, { px, cy - 10.f, std::max(0.f, x + w - px), 20.f }, Kit::Palette::TextDim, count);
        px += mw + 12.f;
        if (px < x + w) {
            const float pop = Kit::Changed(ui, ui.MakeId("hero-on", key), enabled, 0.6f);
            ui.g.FillEllipse(px + 3.f, cy, 3.f + 3.f * pop, 3.f + 3.f * pop, Kit::Palette::Enabled.Fade(enabled > 0 ? 1.f : 0.3f));
            ui.g.Text(on, { px + 12.f, cy - 10.f, std::max(0.f, x + w - px - 12.f), 20.f },
                      enabled > 0 ? Kit::Palette::Enabled : Kit::Palette::TextFaint, count);
        }
    }

    // Body: the mods, unfolding with `open`
    if (open > 0.f) {
        const float bodyTop = r.y + kHeaderH;
        ui.PushClip({ r.x, bodyTop, r.w, r.B() - bodyTop });
        ui.g.FillRect({ r.x, bodyTop, r.w, 1.f }, Kit::Palette::Line.Fade(open));
        float y = bodyTop + kBodyPad;
        const float x = r.x + kBodySide, w = r.w - 2.f * kBodySide;
        int live = 0;
        int index = 0;
        for (const auto& m : row.mods) {
            const float appear = Appear(m.shownAt, now), leave = Leave(m.hiddenAt, now);
            if (appear < 1.f || leave > 0.f) ui.KeepAnimating();
            const float k = appear * (1.f - leave);
            const float h = ModCardHeight(ui);
            if (k > 0.01f && ui.Visible({ x, y, w, h * k }, 20.f)) {
                // A new mod slides in from the side, a leaving one fades and shrinks; unfolding
                // brings the rows in one after the other
                const float cascade = Anim::Clamp01(open * 1.6f - index * 0.12f);
                M::Motion::Enter enter(ui, k * cascade, 0.f, 28.f * (1.f - appear) + 18.f * (1.f - cascade));
                ui.PushScale(1.f - 0.04f * leave, x + w * 0.5f, y + h * 0.5f);
                ModCard(ui, { x, y, w, h }, m, interactive && m.hiddenAt < 0.0);
                ui.PopTransform();
            }
            y += (h + kModGap) * k;
            if (m.hiddenAt < 0.0) ++live;
            ++index;
        }
        if (live == 0) {
            const float h = Kit::LineH(ui, EmptyStyle());
            ui.g.Text(Loc::Get("hero_empty"), { x, y + 6.f, w, h }, Kit::Palette::TextFaint, EmptyStyle());
        }
        ui.PopClip();
    }
    ui.g.PopLayer();
    // A thin edge, taking the role colour on hover
    ui.g.StrokeRound(r.Inset(0.5f), kRadius, Lerp(Kit::Palette::Line, role.Fade(0.6f), fb.hover), 1.f);
    if (!interactive) ui.SetInputMuted(muted);
}

} // namespace RivalsForge::Main

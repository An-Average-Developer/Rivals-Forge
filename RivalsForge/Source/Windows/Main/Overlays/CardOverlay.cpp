#include "Windows/Main/Overlays/CardOverlay.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {
namespace M = UI::Material;
constexpr double kOpen = 0.45, kClose = 0.2;
} // namespace

void CardOverlay::Open() {
    if (open) return;
    open = true;
    changedAt = -1.0;
}

void CardOverlay::Close() {
    if (!open) return;
    open = false;
    changedAt = -2.0;
}

Color CardOverlay::Accent() const { return M::P().primaryMid.color; }

float CardOverlay::Reveal(UI::Ui& ui, int index) const { return open ? M::Motion::Stagger(ui, changedAt + 0.15, index, 0.07f, 0.4f) : 1.f; }

namespace {

// The card's outline: a rectangle with its top-left and bottom-right corners cut off
Microsoft::WRL::ComPtr<ID2D1PathGeometry> CutCorners(const Rect& r, float cut) {
    return Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(r.x + cut, r.y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(r.R(), r.y));
        sink->AddLine(D2D1::Point2F(r.R(), r.B() - cut));
        sink->AddLine(D2D1::Point2F(r.R() - cut, r.B()));
        sink->AddLine(D2D1::Point2F(r.x, r.B()));
        sink->AddLine(D2D1::Point2F(r.x, r.y + cut));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
}

} // namespace

void CardOverlay::PushCardClip(UI::Ui& ui, const Rect& card) const {
    const auto shape = CutCorners(card, kCut);
    ui.g.PushLayer(1.f, shape.Get());
}

void CardOverlay::Frame(UI::Ui& ui, const Rect& card) const {
    namespace Kit = ModsKit;
    const Color accent = Accent();
    const float t = static_cast<float>(ui.Time());
    M::Elevation(ui.g, card, 4.f, 18.f);
    // The glow breathes gently while the card is up
    ui.g.GlowRound(card, 4.f, accent, 24.f, 0.26f + 0.08f * std::sin(t * 1.6f));
    ui.KeepAnimatingThrottled();
    const auto shape = CutCorners(card, kCut);
    ui.g.FillGeometry(shape.Get(), Kit::Palette::Dialog);
    ui.g.PushLayer(1.f, shape.Get());
    // Lit from the top left by the accent, the halftone in the top right corner
    ui.g.FillRadial({ card.x - card.w * 0.4f, card.y - 160.f, card.w * 1.1f, 300.f }, { { 0.f, accent.Fade(0.12f) }, { 1.f, accent.Fade(0.f) } });
    const Rect dots{ card.R() - 200.f, card.y, 200.f, 120.f };
    Kit::Halftone(ui, dots, 0.05f);
    ui.g.FillLinear(dots, 0.f, { { 0.f, Kit::Palette::Dialog }, { 0.6f, Kit::Palette::Dialog.Fade(0.f) } }, { 0.f, 1.f }, { 1.f, 0.f });
    // The top edge: an accent line with a light sweeping along it every few seconds
    ui.g.FillLinear({ card.x, card.y, card.w, 3.f }, 0.f, { { 0.f, accent }, { 0.7f, accent.Fade(0.35f) }, { 1.f, accent.Fade(0.f) } }, { 0.f, 0.f },
                    { 1.f, 0.f });
    const float sweep = std::fmod(t * 0.35f, 1.6f) - 0.3f;
    if (sweep > -0.2f && sweep < 1.2f) {
        const float x = card.x + card.w * sweep;
        ui.g.FillLinear({ x - 60.f, card.y, 120.f, 3.f }, 0.f, { { 0.f, Kit::Palette::Strong.Fade(0.f) }, { 0.5f, Kit::Palette::Strong.Fade(Kit::IsLight() ? 0.35f : 0.8f) }, { 1.f, Kit::Palette::Strong.Fade(0.f) } },
                        { 0.f, 0.f }, { 1.f, 0.f });
    }
    ui.g.PopLayer();
    ui.g.StrokeGeometry(shape.Get(), Kit::Palette::DialogBorder, 1.f);
    // The accent slash on the cut corner
    ui.g.Line(card.x + 1.f, card.y + kCut + 6.f, card.x + kCut + 6.f, card.y + 1.f, accent, 2.f);
}

void CardOverlay::Draw(UI::Ui& ui, const Rect& screen) {
    const double now = ui.Time();
    if (changedAt == -1.0 || changedAt == -2.0) changedAt = now;
    const float raw = Anim::Clamp01(static_cast<float>((now - changedAt) / (open ? kOpen : kClose)));
    if (!open && raw >= 1.f) {
        changedAt = -10.0;
        if (onClosed) onClosed();
        return;
    }
    ui.KeepAnimating();
    const float fade = open ? Anim::EaseOutCubic(raw) : 1.f - Anim::EaseInCubic(raw);
    const float scale = open ? Anim::Lerp(0.86f, 1.f, Anim::EaseOutBack(raw, 1.2f)) : Anim::Lerp(1.f, 0.94f, raw);
    const float rise = open ? 30.f * (1.f - Anim::EaseOutCubic(raw)) : 16.f * raw;
    const float slide = open ? -40.f * (1.f - Anim::EaseOutCubic(raw)) : 24.f * raw;

    ui.Block(screen);
    ui.g.FillRect(screen, Colors::Black.Fade((ModsKit::IsLight() ? 0.32f : 0.55f) * fade));
    const float w = std::min(CardWidth(), screen.w - 80.f);
    const float h = std::min(CardHeight(ui, w), screen.h - 60.f);
    const Rect card{ screen.CX() - w * 0.5f, screen.CY() - h * 0.5f, w, h };
    const bool covered = cover && cover->Visible();
    if (open && !covered && Closable() && (ui.KeyPressed(VK_ESCAPE) || (ui.In().pressed[0] && !card.Contains(ui.MouseX(), ui.MouseY())))) Close();

    ui.g.PushOpacity(std::max(0.f, fade));
    ui.PushTranslate(slide, rise);
    ui.PushScale(scale, card.CX(), card.CY());
    ui.Block(card);
    DrawCard(ui, card);
    ui.PopTransform();
    ui.PopTransform();
    ui.g.PopLayer();
}

} // namespace RivalsForge::Main

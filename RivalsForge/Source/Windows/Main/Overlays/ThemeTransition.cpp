#include "Windows/Main/Overlays/ThemeTransition.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {
namespace Kit = ModsKit;
constexpr double kSweep = 0.75, kSpin = 0.6;
constexpr float kLean = 0.9f;   // how far the edge leans: x shifts left this much per DIP down
} // namespace

void ThemeTransition::Begin(ComPtr<ID2D1Bitmap1> bitmap, double now) {
    pending = false;
    active = bitmap != nullptr;
    snapshot = std::move(bitmap);
    startedAt = now;
    Kit::ApplyTheme(!Kit::IsLight());
    Kit::SaveTheme();
}

float ThemeTransition::IconAngle(double now) const {
    const double t = now - startedAt;
    if (t < 0.0 || t >= kSpin) return 0.f;
    return 360.f * Anim::EaseInOutCubic(static_cast<float>(t / kSpin));
}

void ThemeTransition::Draw(Graphics& g, const Rect& screen, double now) {
    if (!active) return;
    const double t = now - startedAt;
    if (t >= kSweep || !snapshot) {
        active = false;
        snapshot.Reset();
        return;
    }
    // The edge's top end goes from past the right side (all old) to past the left one (all new)
    const float p = Anim::EaseInOutCubic(static_cast<float>(t / kSweep));
    const float pad = 40.f;
    const float from = screen.R() + kLean * screen.h + pad, to = screen.x - pad;
    const float top = Anim::Lerp(from, to, p), bottom = top - kLean * screen.h;

    // The old theme, left of the edge
    const auto old = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(screen.x - pad - kLean * screen.h, screen.y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(top, screen.y));
        sink->AddLine(D2D1::Point2F(bottom, screen.B()));
        sink->AddLine(D2D1::Point2F(screen.x - pad - kLean * screen.h, screen.B()));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
    g.PushLayer(1.f, old.Get());
    g.DrawBitmap(snapshot.Get(), screen);
    g.PopLayer();

    // The red light along the edge: a soft wide band and a sharp line
    g.PushClip(screen);
    g.Line(top, screen.y, bottom, screen.B(), Kit::Palette::Accent.Fade(0.18f), 26.f);
    g.Line(top, screen.y, bottom, screen.B(), Kit::Palette::Accent.Fade(0.35f), 10.f);
    g.Line(top, screen.y, bottom, screen.B(), Kit::Palette::AccentHover, 2.5f);
    g.PopClip();
}

} // namespace RivalsForge::Main

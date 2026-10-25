#include "Windows/Main/Updates/PageKit.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "UI/Material/Motion.h"

namespace RivalsForge::Main::PageKit {

namespace Kit = ModsKit;

void TitleRow(UI::Ui& ui, const Rect& r, const std::wstring& title, const std::wstring& subtitle, M::Icon pillIcon,
              const std::wstring& pill, Color pillColor) {
    TextStyle head = Kit::Display(30.f, DWRITE_FONT_WEIGHT_BOLD);
    head.ellipsis = true;
    TextStyle sub = Kit::Style(12.5f);
    sub.ellipsis = true;
    // The pill on the right
    TextStyle p = Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    p.valign = VAlign::Center;
    const std::wstring pillText = Kit::Upper(pill);
    const float pw = pill.empty() ? 0.f : std::ceil(ui.g.Measure(pillText, p).w) + 50.f;
    if (pw > 0.f) {
        const Rect pr{ r.R() - pw, r.y + 6.f, pw, 28.f };
        Kit::FillSlant(ui, pr, pillColor.Fade(0.14f), 7.f);
        Kit::EdgeStripe(ui, pr, 4.f, pillColor, 7.f);
        M::DrawIcon(ui.g, pillIcon, pr.x + 20.f, pr.CY(), 15.f, pillColor);
        ui.g.Text(pillText, { pr.x + 32.f, pr.y, pr.w - 36.f, pr.h }, Kit::Palette::Text, p);
    }
    const float tw = r.w - pw - 16.f;
    const float th = Kit::LineH(ui, head);
    ui.g.Text(Kit::Upper(title), { r.x, r.y - 4.f, tw, th }, Kit::Palette::Text, head);
    // A red slash under the title, like the header's tabs
    const float slash = std::min(std::ceil(ui.g.Measure(Kit::Upper(title), head).w) * 0.45f, tw);
    Kit::FillSlant(ui, { r.x + 2.f, r.y - 4.f + th - 2.f, slash, 4.f }, Kit::Palette::Accent, 3.f);
    ui.g.Text(subtitle, { r.x, r.y - 4.f + th + 8.f, tw, 18.f }, Kit::Palette::TextDim, sub);
}

void Panel(UI::Ui& ui, const Rect& r, Color accent) {
    ui.g.FillRect(r, Kit::Palette::Panel);
    ui.g.StrokeRect(r.Inset(0.5f), Kit::Palette::Line, 1.f);
    ui.g.FillLinear({ r.x, r.y, r.w, 2.f }, 0.f, { { 0.f, accent }, { 0.45f, accent.Fade(0.25f) }, { 1.f, accent.Fade(0.f) } }, { 0.f, 0.f },
                    { 1.f, 0.f });
}

float Heading(UI::Ui& ui, float x, float y, float w, const std::wstring& text, Color color) {
    Kit::FillSlant(ui, { x, y + 4.f, 10.f, kHeadingH - 8.f }, color, 4.f);
    TextStyle t = Kit::Display(14.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    t.ellipsis = true;
    ui.g.Text(Kit::Upper(text), { x + 16.f, y, w - 16.f, kHeadingH }, color, t);
    return kHeadingH;
}

float Steps(UI::Ui& ui, float x, float y, float w, const std::wstring* steps, int count, bool draw, double shownAt) {
    const TextStyle text = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
    TextStyle num = Kit::Display(13.f, DWRITE_FONT_WEIGHT_BOLD, false);
    num.align = HAlign::Center;
    num.valign = VAlign::Center;
    const float top = y;
    for (int i = 0; i < count; ++i) {
        const float th = std::max(26.f, std::ceil(ui.g.Measure(steps[i], text, w - 40.f).h));
        if (draw) {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.3, i, 0.08f, 0.4f), 0.f, 16.f);
            const Rect tag{ x, y, 26.f, 24.f };
            Kit::FillSlant(ui, tag, Kit::Palette::Accent.Fade(0.16f), 5.f);
            ui.g.Text(std::to_wstring(i + 1), tag, Kit::IsLight() ? Kit::Palette::AccentPressed : Kit::Palette::AccentHover, num);
            ui.g.Text(steps[i], { x + 38.f, y + (th > 26.f ? 0.f : 4.f), w - 40.f, th }, Kit::Palette::Body, text);
            if (i < count - 1) ui.g.FillRect({ x + 12.f, y + 28.f, 2.f, th - 10.f }, Kit::Palette::Accent.Fade(0.25f));
        }
        y += th + 14.f;
    }
    return y - 14.f - top;
}

void InfoChip(UI::Ui& ui, const Rect& r, M::Icon icon, const std::wstring& label, const std::wstring& value, Color color) {
    Kit::FillSlant(ui, r, Kit::Palette::Ghost, 7.f);
    Kit::EdgeStripe(ui, r, 3.f, color, 7.f);
    M::DrawIcon(ui.g, icon, r.x + 24.f, r.CY(), 18.f, color);
    TextStyle tiny = Kit::Display(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    tiny.ellipsis = true;
    TextStyle big = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    big.ellipsis = true;
    ui.g.Text(Kit::Upper(label), { r.x + 42.f, r.y + 7.f, r.w - 50.f, 16.f }, Kit::Palette::TextDim, tiny);
    ui.g.Text(value, { r.x + 42.f, r.y + 23.f, r.w - 54.f, 20.f }, Kit::Palette::Text, big);
}

void Badge(UI::Ui& ui, float cx, float cy, float size, Color color, M::Icon icon, bool rings, float pop, float iconAngle, float iconBob) {
    const float t = static_cast<float>(ui.Time());
    const Rect badge{ cx - size * 0.5f, cy - size * 0.5f, size, size };
    ui.PushScale(0.6f + 0.4f * Anim::EaseOutBack(pop), cx, cy);
    if (rings) {
        for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(t * 0.6f + i * 0.5f, 1.f);
            Kit::StrokeSlant(ui, badge.Inset(-30.f * k), color.Fade(0.5f * (1.f - k)), 1.5f, 14.f + 6.f * k);
        }
        ui.KeepAnimatingThrottled();
    }
    ui.g.GlowRound(badge.Inset(8.f, 4.f), 4.f, color, 26.f, 0.5f);
    Kit::FillSlant(ui, badge, color, 14.f);
    Kit::PushSlantClip(ui, badge, 14.f);
    ui.g.FillLinear(badge, 0.f, { { 0.f, Colors::White.Fade(0.18f) }, { 1.f, Colors::Black.Fade(0.12f) } }, { 0.f, 0.f }, { 0.f, 1.f });
    Kit::Halftone(ui, badge, 0.12f);
    Kit::Sheen(ui, badge, std::fmod(t * 0.35f, 2.f), 1.f);
    ui.g.PopLayer();
    ui.KeepAnimatingThrottled();
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(iconAngle, D2D1::Point2F(cx, cy)) * D2D1::Matrix3x2F::Translation(0.f, iconBob));
    M::DrawIcon(ui.g, icon, cx, cy, size * 0.52f, Colors::White);
    ui.g.PopTransform();
    ui.PopTransform();
}

void Arc(UI::Ui& ui, float cx, float cy, float radius, float startDeg, float sweepDeg, Color color, float width) {
    if (sweepDeg <= 0.f) return;
    sweepDeg = std::min(sweepDeg, 359.9f);
    constexpr float kRad = 3.14159265f / 180.f;
    const float a0 = startDeg * kRad, a1 = (startDeg + sweepDeg) * kRad;
    const auto arc = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(cx + radius * std::sin(a0), cy - radius * std::cos(a0)), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx + radius * std::sin(a1), cy - radius * std::cos(a1)), D2D1::SizeF(radius, radius), 0.f,
                                      D2D1_SWEEP_DIRECTION_CLOCKWISE, sweepDeg > 180.f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    });
    ui.g.StrokeGeometry(arc.Get(), color, width, true);
}

void Progress(UI::Ui& ui, const Rect& r, float value, Color color) {
    Kit::FillSlant(ui, r, Kit::Palette::Ghost, 4.f);
    Rect fill = r;
    fill.w = std::max(8.f, r.w * std::clamp(value, 0.f, 1.f));
    Kit::FillSlant(ui, fill, color, 4.f);
    Kit::PushSlantClip(ui, fill, 4.f);
    const float offset = std::fmod(static_cast<float>(ui.Time()) * 30.f, 12.f);
    for (float x = fill.x - 12.f + offset; x < fill.R(); x += 12.f) ui.g.Line(x, fill.B(), x + fill.h, fill.y, Colors::White.Fade(0.2f), 4.f);
    ui.g.PopLayer();
    ui.KeepAnimating();
}

Kit::ButtonStyle Go() {
    Kit::ButtonStyle s = Kit::Primary();
    s.background = Hex(0x2FA862);
    s.hoverBackground = Hex(0x3CC777);
    s.pressedBackground = Hex(0x23864D);
    return s;
}

bool Link(UI::Ui& ui, UI::Id id, const Rect& r, M::Icon icon, const std::wstring& text, HAlign align) {
    TextStyle t = Kit::Style(12.f);
    t.valign = VAlign::Center;
    const float tw = std::ceil(ui.g.Measure(text, t).w) + 2.f;
    const float w = tw + 22.f;
    const float x = align == HAlign::Center ? r.CX() - w * 0.5f : align == HAlign::Right ? r.R() - w : r.x;
    const Rect hit{ x, r.y, w, r.h };
    const UI::Interaction it = ui.Interact(id, hit);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
    const Color c = Lerp(Kit::Palette::TextDim, Kit::Palette::AccentHover, hover);
    M::DrawIcon(ui.g, icon, x + 7.f, hit.CY(), 14.f, c);
    t.underline = hover > 0.5f;
    ui.g.Text(text, { x + 20.f, hit.y, tw, hit.h }, c, t);
    return it.clicked;
}

Columns Split(const Rect& area, float leftMinH) {
    Columns c;
    c.stacked = area.w < 680.f;
    if (c.stacked) {
        c.left = { area.x, area.y, area.w, leftMinH };
        c.right = { area.x, c.left.B() + kGap, area.w, 0.f };
    } else {
        const float h = std::max(area.h, leftMinH);
        c.left = { area.x, area.y, area.w - kRightW - kGap, h };
        c.right = { c.left.R() + kGap, area.y, kRightW, h };
    }
    return c;
}

} // namespace RivalsForge::Main::PageKit

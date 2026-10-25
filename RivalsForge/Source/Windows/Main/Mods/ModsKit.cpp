#include "Windows/Main/Mods/ModsKit.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "App/Paths.h"
#include "Core/Text.h"
#include "Data/Heroes.h"
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "Localization/Localization.h"
#include "Settings/ConfigFile.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"

namespace RivalsForge::Main::ModsKit {

namespace M = UI::Material;
using Microsoft::WRL::ComPtr;

// ── Theme ───────────────────────────────────────────────────────────────────

namespace {
bool light = false;
constexpr const wchar_t* kThemeFile = L"theme.cfg";
} // namespace

bool IsLight() { return light; }

float GlowStrength() { return light ? 0.3f : 1.f; }

void ApplyTheme(bool toLight) {
    light = toLight;
    namespace P = Palette;
    if (toLight) {
        P::Window = Hex(0xEDEDF2);
        P::WindowBorder = Hex(0xC4C4CE);
        P::Panel = Hex(0xFFFFFF);
        P::PanelRaised = Hex(0xF7F7FA);
        P::PanelHover = Hex(0xEFEFF4);
        P::Sunken = Hex(0xEDEDF1);
        P::Line = Hex(0xD3D3DC);
        P::Text = Hex(0x15151A);
        P::TextDim = Hex(0x5C5C68);
        P::TextFaint = Hex(0x9E9EAA);
        P::Body = Hex(0x3A3A44);
        P::Strong = Hex(0x000000);
        P::Accent = Hex(0xC41E3A);
        P::AccentHover = Hex(0xD62845);
        P::AccentPressed = Hex(0xA01830);
        P::Gold = Hex(0xC98A00);
        P::Info = Hex(0x1C7ED6);
        P::Enabled = Hex(0x1E9E55);
        P::Disabled = Hex(0x9A9AA4);
        P::Danger = Hex(0xE0283C);
        P::Ghost = Hex(0xEAEAEF);
        P::GhostHover = Hex(0xDEDEE5);
        P::StatusBar = Hex(0xE4E4EA);
        P::Dialog = Hex(0xFFFFFF);
        P::DialogBorder = Hex(0xD6D6DE);
        P::InputBg = Hex(0xF3F3F6);
        P::InputBorder = Hex(0xCDCDD5);
        P::Shade = Hex(0x000000);
    } else {
        P::Window = Hex(0x0B0B0E);
        P::WindowBorder = Hex(0x26262D);
        P::Panel = Hex(0x121216);
        P::PanelRaised = Hex(0x18181D);
        P::PanelHover = Hex(0x202027);
        P::Sunken = Hex(0x0E0E12);
        P::Line = Hex(0x23232A);
        P::Text = Hex(0xFFFFFF);
        P::TextDim = Hex(0x9A9AA6);
        P::TextFaint = Hex(0x5E5E68);
        P::Body = Hex(0xC4C4CC);
        P::Strong = Hex(0xFFFFFF);
        P::Accent = Hex(0xC41E3A);
        P::AccentHover = Hex(0xE8304C);
        P::AccentPressed = Hex(0xA01830);
        P::Gold = Hex(0xF5C542);
        P::Info = Hex(0x5AA9F0);
        P::Enabled = Hex(0x43D17A);
        P::Disabled = Hex(0x6E6E78);
        P::Danger = Hex(0xFF4D5E);
        P::Ghost = Hex(0x1D1D23);
        P::GhostHover = Hex(0x2A2A32);
        P::StatusBar = Hex(0x08080A);
        P::Dialog = Hex(0x141418);
        P::DialogBorder = Hex(0x2A2A32);
        P::InputBg = Hex(0x1C1C22);
        P::InputBorder = Hex(0x34343D);
        P::Shade = Hex(0xFFFFFF);
    }
    M::SetLightTheme(toLight);
}

void LoadTheme() { ApplyTheme(Config::ReadText(kThemeFile).value_or(L"dark") == L"light"); }

void SaveTheme() { Config::WriteText(kThemeFile, light ? L"light" : L"dark"); }

// ── Shapes ──────────────────────────────────────────────────────────────────

ComPtr<ID2D1Geometry> SlantGeometry(const Rect& r, float skew) {
    skew = std::min(skew, r.w * 0.3f);
    ComPtr<ID2D1Geometry> geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(r.x + skew, r.y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(r.R(), r.y));
        sink->AddLine(D2D1::Point2F(r.R() - skew, r.B()));
        sink->AddLine(D2D1::Point2F(r.x, r.B()));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
    return geometry;
}

void FillSlant(UI::Ui& ui, const Rect& r, Color color, float skew) {
    if (r.Empty() || color.Transparent()) return;
    ui.g.FillGeometry(SlantGeometry(r, skew).Get(), color);
}

void StrokeSlant(UI::Ui& ui, const Rect& r, Color color, float width, float skew) {
    if (r.Empty() || color.Transparent()) return;
    ui.g.StrokeGeometry(SlantGeometry(r.Inset(width * 0.5f), skew).Get(), color, width);
}

void EdgeStripe(UI::Ui& ui, const Rect& r, float width, Color color, float skew) {
    if (r.Empty() || width <= 0.f || color.Transparent()) return;
    skew = std::min(skew, r.w * 0.3f);   // the shape's own skew (SlantGeometry clamps it the same way)
    const ComPtr<ID2D1Geometry> geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(r.x + skew, r.y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(r.x + skew + width, r.y));
        sink->AddLine(D2D1::Point2F(r.x + width, r.B()));
        sink->AddLine(D2D1::Point2F(r.x, r.B()));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
    ui.g.FillGeometry(geometry.Get(), color);
}

void PushSlantClip(UI::Ui& ui, const Rect& r, float skew) { ui.g.PushLayer(1.f, SlantGeometry(r, skew).Get()); }

void Sheen(UI::Ui& ui, const Rect& r, float t, float strength) {
    if (t <= 0.f || t >= 1.f || strength <= 0.f) return;
    const float c = -0.2f + 1.4f * t;
    ui.g.FillLinear(r, 0.f,
                    { { std::clamp(c - 0.12f, 0.f, 1.f), Colors::White.Fade(0.f) },
                      { std::clamp(c, 0.f, 1.f), Colors::White.Fade(0.35f * strength) },
                      { std::clamp(c + 0.12f, 0.f, 1.f), Colors::White.Fade(0.f) } },
                    { 0.f, 0.f }, { 1.f, 0.45f });
}

namespace {

// A 48 x 48 tile of soft white dots (8 px apart), made once
ImagePtr HalftoneTile() {
    static ImagePtr tile = [] {
        constexpr unsigned kSize = 48;
        std::vector<uint32_t> pixels(kSize * kSize, 0);
        for (unsigned y = 0; y < kSize; ++y)
            for (unsigned x = 0; x < kSize; ++x) {
                // Every other row shifted by half a cell, like printed dots
                const float ox = static_cast<float>((y / 8) % 2 ? (x + 4) % 8 : x % 8) - 3.5f;
                const float oy = static_cast<float>(y % 8) - 3.5f;
                const float d = std::sqrt(ox * ox + oy * oy);
                const float a = std::clamp(1.6f - d, 0.f, 1.f);
                const auto v = static_cast<uint32_t>(a * 255.f);
                pixels[y * kSize + x] = v << 24 | v << 16 | v << 8 | v;   // premultiplied white
            }
        return Images::FromPixels(kSize, kSize, pixels.data());
    }();
    return tile;
}

} // namespace

void Halftone(UI::Ui& ui, const Rect& r, float opacity) {
    const ImagePtr tile = HalftoneTile();
    if (!tile || r.Empty() || opacity <= 0.f) return;
    ui.g.PushClip(r);
    constexpr float kTile = 48.f;
    for (float y = r.y; y < r.B(); y += kTile)
        for (float x = r.x; x < r.R(); x += kTile) ui.g.DrawBitmap(tile->bitmap.Get(), { x, y, kTile, kTile }, opacity);
    ui.g.PopClip();
}

// ── Text ────────────────────────────────────────────────────────────────────

TextStyle Display(float size, DWRITE_FONT_WEIGHT weight, bool italic) {
    TextStyle t;
    t.size = size;
    t.font = Font::Display;
    t.weight = weight;
    t.italic = italic;
    t.valign = VAlign::Top;
    return t;
}

TextStyle Style(float size, DWRITE_FONT_WEIGHT weight, bool wrap, HAlign align) {
    TextStyle t = M::Text(size, weight);
    t.valign = VAlign::Top;
    t.wrap = wrap;
    t.align = align;
    return t;
}

float LineH(UI::Ui& ui, const TextStyle& style) { return ui.g.Measure(L"Ag", style).h; }

std::wstring Upper(const std::wstring& text) { return Text::Upper(text); }

// ── Buttons ─────────────────────────────────────────────────────────────────

ButtonStyle Primary() {
    ButtonStyle s;
    s.background = Palette::Accent;
    s.hoverBackground = Palette::AccentHover;
    s.pressedBackground = Palette::AccentPressed;
    s.foreground = s.hoverForeground = Colors::White;
    s.padding = Edges(20.f, 9.f);
    s.fontSize = 15.f;
    s.glow = true;
    s.sheen = true;
    return s;
}

ButtonStyle Ghost() {
    ButtonStyle s;
    s.background = Palette::Ghost;
    s.hoverBackground = s.pressedBackground = Palette::GhostHover;
    s.foreground = Palette::TextDim;
    s.hoverForeground = Palette::Text;
    s.border = Palette::Line;
    return s;
}

ButtonStyle Danger() {
    ButtonStyle s = Ghost();
    s.hoverBackground = Lerp(Palette::Ghost, Palette::Danger, 0.16f);
    s.pressedBackground = Lerp(Palette::Ghost, Palette::Danger, 0.26f);
    s.foreground = s.hoverForeground = Palette::Danger;
    return s;
}

ButtonStyle DialogButton(bool primary) {
    ButtonStyle s = primary ? Primary() : Ghost();
    s.padding = Edges(14.f, 8.f);
    s.fontSize = 14.f;
    s.sheen = false;
    return s;
}

TextStyle ButtonText(const ButtonStyle& style) {
    TextStyle t = Display(style.fontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    return t;
}

float ButtonWidth(UI::Ui& ui, const std::wstring& text, const ButtonStyle& style) {
    return std::ceil(ui.g.Measure(Upper(text), ButtonText(style)).w) + style.padding.H() + style.skew;
}

float ButtonHeight(UI::Ui& ui, const ButtonStyle& style) { return std::ceil(LineH(ui, ButtonText(style))) + style.padding.V(); }

bool Button(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, const ButtonStyle& style, bool enabled) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held && enabled);
    // A light crosses it once when the mouse comes in; the main action's every few seconds too
    const float hoverSweep = ui.Tween(UI::HashId(id, 0x5EE), it.hovered && enabled ? 1.f : 0.f, 0.5f);
    float sweep = hoverSweep < 1.f ? hoverSweep : 0.f;
    if (style.sheen && enabled) {
        const float cycle = std::fmod(static_cast<float>(ui.Time()) * 0.4f, 2.4f);
        if (cycle < 1.f) sweep = std::max(sweep, cycle);
        ui.KeepAnimatingThrottled();
    }

    ui.PushScale(1.f - 0.04f * fb.press, r.CX(), r.CY());
    ui.g.PushOpacity(enabled ? 1.f : 0.38f);
    const Color bg = Lerp(Lerp(style.background, style.hoverBackground, fb.hover), style.pressedBackground, fb.press);
    if (style.glow && fb.hover > 0.f) ui.g.GlowRound(r.Inset(style.skew * 0.5f, 0.f), 2.f, bg, 14.f, 0.55f * GlowStrength() * fb.hover);
    const ComPtr<ID2D1Geometry> shape = SlantGeometry(r, style.skew);
    ui.g.FillGeometry(shape.Get(), bg);
    if (sweep > 0.f && sweep < 1.f) {
        ui.g.PushLayer(1.f, shape.Get());
        Sheen(ui, r, sweep, 1.f);
        ui.g.PopLayer();
    }
    if (!style.border.Transparent()) StrokeSlant(ui, r, Lerp(style.border, style.hoverForeground.Fade(0.35f), fb.hover), 1.f, style.skew);
    ui.g.Text(Upper(text), r.Offset(fb.hover * 1.f, 0.f), Lerp(style.foreground, style.hoverForeground, fb.hover), ButtonText(style));
    ui.g.PopLayer();
    ui.PopTransform();
    return it.clicked && enabled;
}

// ── Icons ───────────────────────────────────────────────────────────────────

void DrawGlyph(UI::Ui& ui, Glyph glyph, float cx, float cy, float size, Color color) {
    const float s = size;
    const float w = std::max(1.4f, s * 0.11f);
    const auto P = [&](float x, float y) { return D2D1::Point2F(cx + x * s, cy + y * s); };
    const auto line = [&](float x1, float y1, float x2, float y2) { ui.g.Line(cx + x1 * s, cy + y1 * s, cx + x2 * s, cy + y2 * s, color, w, true); };
    const auto poly = [&](std::initializer_list<D2D1_POINT_2F> points, bool closed) {
        auto geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
            auto p = points.begin();
            sink->BeginFigure(*p, D2D1_FIGURE_BEGIN_HOLLOW);
            for (++p; p != points.end(); ++p) sink->AddLine(*p);
            sink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
        });
        ui.g.StrokeGeometry(geometry.Get(), color, w, true);
    };
    switch (glyph) {
        case Glyph::Plus:
            line(-0.38f, 0.f, 0.38f, 0.f);
            line(0.f, -0.38f, 0.f, 0.38f);
            break;
        case Glyph::Pencil:
            poly({ P(-0.36f, 0.36f), P(-0.36f, 0.16f), P(0.2f, -0.4f), P(0.4f, -0.2f), P(-0.16f, 0.36f) }, true);
            line(0.08f, -0.28f, 0.28f, -0.08f);
            break;
        case Glyph::Trash:
            line(-0.4f, -0.26f, 0.4f, -0.26f);
            poly({ P(-0.13f, -0.26f), P(-0.13f, -0.4f), P(0.13f, -0.4f), P(0.13f, -0.26f) }, false);
            poly({ P(-0.3f, -0.18f), P(-0.24f, 0.42f), P(0.24f, 0.42f), P(0.3f, -0.18f) }, false);
            line(-0.08f, -0.06f, -0.08f, 0.28f);
            line(0.08f, -0.06f, 0.08f, 0.28f);
            break;
        case Glyph::Person:
            ui.g.StrokeEllipse(cx, cy - 0.17f * s, 0.17f * s, 0.17f * s, color, w);
            poly({ P(-0.36f, 0.42f), P(-0.3f, 0.2f), P(-0.14f, 0.1f), P(0.14f, 0.1f), P(0.3f, 0.2f), P(0.36f, 0.42f) }, false);
            break;
        case Glyph::Folder:
            poly({ P(-0.44f, -0.32f), P(-0.1f, -0.32f), P(0.f, -0.2f), P(0.44f, -0.2f), P(0.44f, 0.34f), P(-0.44f, 0.34f) }, true);
            line(-0.44f, -0.08f, 0.44f, -0.08f);
            break;
        case Glyph::Warning:
            poly({ P(0.f, -0.42f), P(0.46f, 0.38f), P(-0.46f, 0.38f) }, true);
            line(0.f, -0.12f, 0.f, 0.12f);
            ui.g.FillEllipse(cx, cy + 0.25f * s, w * 0.7f, w * 0.7f, color);
            break;
        case Glyph::Check:
            poly({ P(-0.36f, 0.02f), P(-0.1f, 0.28f), P(0.38f, -0.26f) }, false);
            break;
        case Glyph::Box:
            poly({ P(-0.4f, -0.2f), P(0.f, -0.4f), P(0.4f, -0.2f), P(0.4f, 0.24f), P(0.f, 0.44f), P(-0.4f, 0.24f) }, true);
            poly({ P(-0.4f, -0.2f), P(0.f, 0.f), P(0.4f, -0.2f) }, false);
            line(0.f, 0.f, 0.f, 0.44f);
            break;
        case Glyph::Heart: {
            auto geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
                sink->BeginFigure(P(0.f, 0.4f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddBezier(D2D1::BezierSegment(P(-0.6f, 0.f), P(-0.45f, -0.55f), P(0.f, -0.2f)));
                sink->AddBezier(D2D1::BezierSegment(P(0.45f, -0.55f), P(0.6f, 0.f), P(0.f, 0.4f)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            });
            ui.g.FillGeometry(geometry.Get(), color);
            break;
        }
        case Glyph::Download:
            line(0.f, -0.4f, 0.f, 0.14f);
            poly({ P(-0.24f, -0.08f), P(0.f, 0.16f), P(0.24f, -0.08f) }, false);
            line(-0.38f, 0.38f, 0.38f, 0.38f);
            break;
        case Glyph::Eye: {
            // An almond (two arcs meeting at the corners) and its pupil
            auto geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
                sink->BeginFigure(P(-0.42f, 0.f), D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddBezier(D2D1::BezierSegment(P(-0.2f, -0.34f), P(0.2f, -0.34f), P(0.42f, 0.f)));
                sink->AddBezier(D2D1::BezierSegment(P(0.2f, 0.34f), P(-0.2f, 0.34f), P(-0.42f, 0.f)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            });
            ui.g.StrokeGeometry(geometry.Get(), color, w, true);
            ui.g.FillEllipse(cx, cy, 0.11f * s, 0.11f * s, color);
            break;
        }
        case Glyph::Search:
            ui.g.StrokeEllipse(cx - 0.08f * s, cy - 0.08f * s, 0.26f * s, 0.26f * s, color, w);
            line(0.12f, 0.12f, 0.4f, 0.4f);
            break;
        case Glyph::Play: {
            auto geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
                sink->BeginFigure(P(-0.28f, -0.38f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(P(0.38f, 0.f));
                sink->AddLine(P(-0.28f, 0.38f));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            });
            ui.g.FillGeometry(geometry.Get(), color);
            break;
        }
        case Glyph::External:
            poly({ P(0.05f, -0.36f), P(-0.36f, -0.36f), P(-0.36f, 0.36f), P(0.36f, 0.36f), P(0.36f, -0.05f) }, false);
            line(-0.05f, 0.05f, 0.4f, -0.4f);
            poly({ P(0.12f, -0.4f), P(0.4f, -0.4f), P(0.4f, -0.12f) }, false);
            break;
    }
}

bool IconButton(UI::Ui& ui, UI::Id id, const Rect& r, Glyph glyph, Color hover, const std::wstring& tooltip, bool enabled) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held && enabled);
    ui.PushScale(1.f - 0.06f * fb.press, r.CX(), r.CY());
    ui.g.PushOpacity(enabled ? 1.f : 0.35f);
    FillSlant(ui, r, Lerp(Palette::Ghost, Palette::GhostHover, fb.hover), 5.f);
    if (fb.hover > 0.f) StrokeSlant(ui, r, hover.Fade(0.6f * fb.hover), 1.f, 5.f);
    ui.PushScale(1.f + 0.1f * fb.hover, r.CX(), r.CY());
    DrawGlyph(ui, glyph, r.CX(), r.CY(), std::min(r.w, r.h) * 0.52f, Lerp(Palette::TextDim, hover, fb.hover));
    ui.PopTransform();
    ui.g.PopLayer();
    ui.PopTransform();
    if (!tooltip.empty()) ui.Tooltip(id, tooltip);
    return it.clicked && enabled;
}

void Chevron(UI::Ui& ui, float cx, float cy, float size, float turn, Color color) {
    // ▼ pointing down at 0, turned half a circle at 1
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(180.f * turn, D2D1::Point2F(cx, cy)));
    const float h = size * 0.3f;
    ui.g.Line(cx - size * 0.5f, cy - h, cx, cy + h, color, 1.8f, true);
    ui.g.Line(cx, cy + h, cx + size * 0.5f, cy - h, color, 1.8f, true);
    ui.g.PopTransform();
}

// ── Switch ──────────────────────────────────────────────────────────────────

bool Switch(UI::Ui& ui, UI::Id id, const Rect& r, bool on, bool enabled) {
    const UI::Interaction it = ui.Interact(id, r.Inset(-4.f), enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held && enabled);
    const float k = Anim::EaseInOutCubic(ui.Tween(UI::HashId(id, 0x0A), on ? 1.f : 0.f, 0.22f));
    if (k > 0.f && k < 1.f) ui.KeepAnimating();
    ui.g.PushOpacity(enabled ? 1.f : 0.4f);
    const float radius = r.h * 0.5f;
    if (k > 0.f) ui.g.GlowRound(r, radius, Palette::Enabled, 10.f, 0.4f * k);
    ui.g.FillRound(r, radius, Lerp(Lerp(light ? Hex(0xD4D4DC) : Hex(0x2A2A32), light ? Hex(0xC4C4CE) : Hex(0x34343D), fb.hover), Palette::Enabled, k));
    // The knob: a little wider while pressed
    const float knob = r.h - 6.f;
    const float stretch = 4.f * fb.press;
    const float x = r.x + 3.f + (r.w - knob - 6.f - stretch) * k;
    ui.g.FillRound({ x, r.y + 3.f, knob + stretch, knob }, knob * 0.5f, Lerp(light ? Colors::White : Hex(0xBDBDC6), Colors::White, k));
    ui.g.PopLayer();
    return it.clicked && enabled;
}

// ── Heroes ──────────────────────────────────────────────────────────────────

ImagePtr HeroPortrait(const std::wstring& heroId) {
    if (heroId.empty() || !Data::FindHero(heroId)) return nullptr;
    return Images::Load(Paths::Asset(L"Heroes\\" + heroId + L".png"));
}

std::wstring HeroName(const std::wstring& heroId) {
    const Data::Hero* hero = Data::FindHero(heroId);
    return hero ? hero->name : Loc::Get("hero_none");
}

Color HeroColor(const std::wstring& heroId) {
    const Data::Hero* hero = Data::FindHero(heroId);
    return hero ? Data::RoleColor(hero->role) : Hex(0x6A6A74);
}

void Portrait(UI::Ui& ui, const std::wstring& heroId, const Rect& r, float radius, float zoom, float opacity) {
    if (r.Empty()) return;
    const ImagePtr image = HeroPortrait(heroId);
    ui.g.PushRoundClip(r, radius);
    if (opacity < 1.f) ui.g.PushOpacity(opacity);
    if (image && image->bitmap && image->w > 0.f) {
        // Cover: as wide as the rect at least, the top (the face) kept; the zoom grows it from the
        // top centre, so the face stays in view
        const float scale = std::max(r.w / image->w, r.h / image->h) * zoom;
        const float w = image->w * scale, h = image->h * scale;
        ui.g.FillRect(r, light ? Hex(0xDADAE0) : Hex(0x15151A));
        ui.g.DrawBitmap(image->bitmap.Get(), { r.CX() - w * 0.5f, r.y - (zoom - 1.f) * r.h * 0.25f, w, h });
    } else {
        // No hero: a big "?" on a dark gradient with the halftone
        ui.g.FillLinear(r, 0.f, { { 0.f, light ? Hex(0xE6E6EC) : Hex(0x26262D) }, { 1.f, light ? Hex(0xD2D2DA) : Hex(0x131317) } }, { 0.f, 0.f },
                        { 0.f, 1.f });
        Halftone(ui, r, 0.06f);
        TextStyle mark = Display(std::min(r.w, r.h) * 0.6f);
        mark.align = HAlign::Center;
        mark.valign = VAlign::Center;
        ui.PushScale(zoom, r.CX(), r.CY());
        ui.g.Text(L"?", r, light ? Hex(0xA8A8B4) : Hex(0x4A4A54), mark);
        ui.PopTransform();
    }
    if (opacity < 1.f) ui.g.PopLayer();
    ui.g.PopLayer();
}

void SlantPortrait(UI::Ui& ui, const std::wstring& heroId, const Rect& r, float skew, float zoom, float opacity) {
    PushSlantClip(ui, r, skew);
    Portrait(ui, heroId, r, 0.f, zoom, opacity);
    ui.g.PopLayer();
}

// ── Motion ──────────────────────────────────────────────────────────────────

float Changed(UI::Ui& ui, UI::Id id, int64_t value, float duration) {
    struct State {
        bool seen = false;
        int64_t value = 0;
        double at = -100.0;
    };
    State& s = ui.Data<State>(UI::HashId(id, 0xC4A6));
    if (s.seen && s.value != value) s.at = ui.Time();
    s.seen = true;
    s.value = value;
    const float t = static_cast<float>((ui.Time() - s.at) / duration);
    if (t >= 1.f) return 0.f;
    ui.KeepAnimating();
    return 1.f - Anim::EaseOutCubic(t);
}

float Count(UI::Ui& ui, UI::Id id, float value) {
    const float shown = ui.Animate(id, value, 9.f, 0.f);
    if (std::fabs(shown - value) > 0.02f) ui.KeepAnimating();
    return shown;
}

// ── Tooltip ─────────────────────────────────────────────────────────────────

void DrawTooltip(Graphics& g, const std::wstring& text, float mouseX, float mouseY, float age, float left) {
    constexpr float kTipSkew = 6.f, kArrow = 6.f, kMaxW = 320.f;
    const float in = Anim::EaseOutCubic(Anim::Clamp01(age / 0.18f));
    const float out = Anim::Clamp01(left / 0.2f);
    const float alpha = in * out;
    if (alpha <= 0.f) return;

    TextStyle style = M::Text(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    style.wrap = true;
    style.valign = VAlign::Top;
    const TextSize size = g.Measure(text, style, kMaxW);
    const float w = std::ceil(size.w) + 30.f + kTipSkew, h = std::ceil(size.h) + 14.f;
    // Under the cursor (above it near the bottom), kept inside the window
    bool below = true;
    float x = mouseX - w * 0.5f, y = mouseY + 22.f;
    x = std::clamp(x, 6.f, std::max(6.f, g.Width() - w - 6.f));
    if (y + h > g.Height() - 6.f) {
        y = mouseY - h - 14.f;
        below = false;
    }
    y += (below ? 6.f : -6.f) * (1.f - in);   // rises (or drops) into place
    const Rect box{ std::round(x), std::round(y), w, h };
    const float ax = std::clamp(mouseX, box.x + kTipSkew + 10.f, box.R() - kTipSkew - 10.f);

    g.PushOpacity(alpha);
    g.GlowRound(box.Inset(kTipSkew * 0.5f, 0.f), 2.f, Colors::Black, 14.f, light ? 0.18f : 0.5f);
    const auto shape = SlantGeometry(box, kTipSkew);
    g.FillGeometry(shape.Get(), Palette::Dialog);
    // The arrow pointing at the cursor, in the card's colour
    const auto arrow = Geometry::Build([&](ID2D1GeometrySink* sink) {
        const float edge = below ? box.y + 0.5f : box.B() - 0.5f, tip = below ? box.y - kArrow : box.B() + kArrow;
        sink->BeginFigure(D2D1::Point2F(ax - kArrow, edge), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(ax, tip));
        sink->AddLine(D2D1::Point2F(ax + kArrow, edge));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
    g.FillGeometry(arrow.Get(), Palette::Dialog);
    g.StrokeGeometry(SlantGeometry(box.Inset(0.5f), kTipSkew).Get(), Palette::DialogBorder, 1.f);
    // The red stripe along the left edge, growing in
    {
        const float skew = std::min(kTipSkew, box.w * 0.3f);
        const float sh = box.h * in;
        const auto stripe = Geometry::Build([&](ID2D1GeometrySink* sink) {
            const float top = box.B() - sh, k = skew * sh / box.h;
            sink->BeginFigure(D2D1::Point2F(box.x + k, top), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(box.x + k + 3.f, top));
            sink->AddLine(D2D1::Point2F(box.x + 3.f, box.B()));
            sink->AddLine(D2D1::Point2F(box.x, box.B()));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        });
        g.FillGeometry(stripe.Get(), Palette::Accent);
    }
    g.Text(text, { box.x + kTipSkew + 12.f, box.y + 7.f, std::ceil(size.w) + 2.f, std::ceil(size.h) + 2.f }, Palette::Text, style);
    g.PopLayer();
}

UI::TextBoxStyle InputBoxStyle(float fontSize) {
    UI::TextBoxStyle s;
    s.text = M::Text(fontSize);
    s.foreground = Palette::Text;
    s.background = Palette::InputBg;
    s.border = Palette::InputBorder;
    s.focusBorder = Palette::Accent;
    s.caret = Palette::Text;
    s.selection = Palette::Accent.Fade(0.6f);
    s.padding = Edges(10.f, 8.f);
    s.radius = 4.f;
    s.placeholderColor = Palette::TextFaint;
    return s;
}

} // namespace RivalsForge::Main::ModsKit

#include <cmath>
#include "Graphics/Animation.h"
#include "App/Paths.h"
#include "Graphics/Images.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"

// The title bar: the app's logo, "RIVALS FORGE  MOD MANAGER" in condensed italic
// capitals, and the window buttons (minimize, maximize / restore, close) in the app's own look.
namespace RivalsForge::Main::TitleBar {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

// The window buttons, drawn in the app's look: a slanted tile that lights up on hover (the close
// one in the Marvel red, a light crossing it) and an icon drawn with lines, each with its own little
// move: the minimize bar drops, the maximize frame grows, the close cross turns.
enum class WindowGlyph { Minimize, Maximize, Restore, Close };

void DrawWindowGlyph(UI::Ui& ui, WindowGlyph glyph, float cx, float cy, float hover, Color color) {
    constexpr float kW = 1.6f, kLean = 2.f;   // stroke and how far the shapes lean, like the tiles
    switch (glyph) {
        case WindowGlyph::Minimize: {
            const float y = cy + 1.f + 2.f * hover;
            ui.g.Line(cx - 5.f, y, cx + 5.f, y, color, kW, true);
            break;
        }
        case WindowGlyph::Maximize: {
            const float h = 4.5f + 0.8f * hover;
            const Rect box{ cx - h - 1.5f, cy - h, h * 2.f + 3.f, h * 2.f };
            Kit::StrokeSlant(ui, box, color, kW, kLean);
            break;
        }
        case WindowGlyph::Restore: {
            const float d = 1.f + 0.8f * hover;
            Kit::StrokeSlant(ui, { cx - 4.f + d, cy - 6.f - d * 0.5f, 11.f, 8.f }, color.Fade(0.6f), kW, kLean);
            Kit::StrokeSlant(ui, { cx - 7.f, cy - 2.f, 11.f, 8.f }, color, kW, kLean);
            break;
        }
        case WindowGlyph::Close: {
            const float turn = 90.f * Anim::EaseOutBack(hover, 0.6f);
            ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(turn, D2D1::Point2F(cx, cy)));
            ui.g.Line(cx - 5.f, cy - 5.f, cx + 5.f, cy + 5.f, color, kW, true);
            ui.g.Line(cx + 5.f, cy - 5.f, cx - 5.f, cy + 5.f, color, kW, true);
            ui.g.PopTransform();
            break;
        }
    }
}

// True when clicked
bool WindowButton(UI::Ui& ui, const char* name, const Rect& r, WindowGlyph glyph) {
    const bool close = glyph == WindowGlyph::Close;
    const UI::Id id = ui.MakeId(name);
    const UI::Interaction it = ui.Interact(id, r);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Rect tile = r.Inset(Edges(3.f, 6.f));
    if (fb.hover > 0.f) {
        ui.PushScale(0.9f + 0.1f * Anim::EaseOutBack(fb.hover, 0.8f) - 0.05f * fb.press, tile.CX(), tile.CY());
        if (close) {
            Kit::FillSlant(ui, tile, Lerp(Kit::Palette::Accent, Kit::Palette::AccentPressed, fb.press).Fade(fb.hover), 7.f);
            Kit::PushSlantClip(ui, tile, 7.f);
            Kit::Sheen(ui, tile, std::fmod(static_cast<float>(ui.Time()) * 0.9f, 1.6f), fb.hover);
            ui.g.PopLayer();
            ui.KeepAnimatingThrottled();
        } else {
            Kit::FillSlant(ui, tile, Lerp(Kit::Palette::GhostHover, Kit::Palette::Ghost, fb.press).Fade(fb.hover), 7.f);
            Kit::EdgeStripe(ui, tile, 2.5f, Kit::Palette::Accent.Fade(fb.hover), 7.f);
        }
        ui.PopTransform();
    }
    const Color color = Lerp(Kit::Palette::TextDim, close ? Colors::White : Kit::Palette::Strong, fb.hover);
    ui.PushScale(1.f - 0.1f * fb.press, r.CX(), r.CY());
    DrawWindowGlyph(ui, glyph, r.CX(), r.CY(), fb.hover, color);
    ui.PopTransform();
    return it.clicked;
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar) {
    // The app's logo, a soft red glow breathing behind it
    const Rect logo{ bar.x + 14.f, bar.CY() - 12.f, 24.f, 24.f };
    {
        const float breathe = 0.5f + 0.5f * std::sin(static_cast<float>(ui.Time()) * 1.4f);
        ui.KeepAnimatingThrottled();
        ui.g.GlowRound(logo.Inset(3.f), 4.f, Kit::Palette::Accent, 10.f, 0.25f + 0.2f * breathe);
        if (const ImagePtr image = Images::Load(Paths::Asset(L"Logo\\logo.png"), 64); image && image->bitmap)
            ui.g.DrawBitmap(image->bitmap.Get(), logo);
    }

    // "RIVALS FORGE" + "  MOD MANAGER" (brand: not translated)
    TextStyle brand = Kit::Display(14.f, DWRITE_FONT_WEIGHT_BOLD);
    brand.valign = VAlign::Center;
    TextStyle rest = Kit::Display(14.f, DWRITE_FONT_WEIGHT_SEMI_LIGHT);
    rest.valign = VAlign::Center;
    const std::wstring first = L"RIVALS FORGE", second = L"  MOD MANAGER";
    const float x = logo.R() + 10.f;
    const float w = std::ceil(ui.g.Measure(first, brand).w);
    ui.g.Text(first, { x, bar.y, w + 4.f, bar.h }, Kit::Palette::Text, brand);
    ui.g.Text(second, { x + w, bar.y, bar.w, bar.h }, Kit::Palette::TextDim, rest);

    const float bw = MainLayout::WindowButtonWidth;
    Rect r{ bar.R() - bw * 3.f, bar.y, bw, bar.h };
    if (WindowButton(ui, "win-min", r, WindowGlyph::Minimize)) main.Minimize();
    r.x += bw;
    // The frame, or two frames while maximized (Window_StateChanged)
    if (WindowButton(ui, "win-max", r, main.IsMaximized() ? WindowGlyph::Restore : WindowGlyph::Maximize)) main.ToggleMaximize();
    r.x += bw;
    if (WindowButton(ui, "win-close", r, WindowGlyph::Close)) main.Quit();
}

} // namespace RivalsForge::Main::TitleBar

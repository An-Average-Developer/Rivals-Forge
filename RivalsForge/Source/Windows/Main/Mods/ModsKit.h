#pragma once
#include <d2d1.h>
#include <string>
#include <wrl/client.h>
#include "Graphics/Graphics.h"
#include "Graphics/Images.h"
#include "UI/Ui.h"
#include "UI/Widgets/TextBox.h"

// The look of the whole window, in one place: the palette and the pieces every screen is built
// from. The redesign borrows from the game's own menus: slanted panels and buttons, condensed
// italic capitals (Bahnschrift), a comic halftone, the Marvel red, light sweeping across things.
namespace RivalsForge::Main::ModsKit {

// The colours of the current theme (dark: the game's near-black menus; light: white panels on a
// pale grey window, as in the reference shot). ApplyTheme swaps them all at once; everything reads
// them every frame, so the next frame is drawn in the new theme.
namespace Palette {
// Window and panels
inline Color Window = Hex(0x0B0B0E);
inline Color WindowBorder = Hex(0x26262D);
inline Color Panel = Hex(0x121216);          // hero cards, the status bar's tag
inline Color PanelRaised = Hex(0x18181D);    // mod rows
inline Color PanelHover = Hex(0x202027);
inline Color Sunken = Hex(0x0E0E12);         // wells inside the cards (file lists, the hero grid)
inline Color Line = Hex(0x23232A);           // separators
// Text
inline Color Text = Hex(0xFFFFFF);
inline Color TextDim = Hex(0x9A9AA6);
inline Color TextFaint = Hex(0x5E5E68);
inline Color Body = Hex(0xC4C4CC);           // paragraphs in the cards
inline Color Strong = Hex(0xFFFFFF);         // what text lights up to on hover
// The Marvel red (#C41E3A, MainWindow.xaml's accent) and its states; the game's gold for highlights
inline Color Accent = Hex(0xC41E3A);
inline Color AccentHover = Hex(0xE8304C);
inline Color AccentPressed = Hex(0xA01830);
inline Color Gold = Hex(0xF5C542);
inline Color Info = Hex(0x5AA9F0);           // links, the Nexus blue
// Status (ModViewModel.StatusBrush, brighter)
inline Color Enabled = Hex(0x43D17A);
inline Color Disabled = Hex(0x6E6E78);
inline Color Danger = Hex(0xFF4D5E);
// Buttons without a colour of their own
inline Color Ghost = Hex(0x1D1D23);
inline Color GhostHover = Hex(0x2A2A32);
// Bars, dialogs, text boxes
inline Color StatusBar = Hex(0x08080A);
inline Color Dialog = Hex(0x141418);
inline Color DialogBorder = Hex(0x2A2A32);
inline Color InputBg = Hex(0x1C1C22);
inline Color InputBorder = Hex(0x34343D);
// Light drawn over the shapes (sheens, slanted bands, stripes): white on dark, black on light
inline Color Shade = Hex(0xFFFFFF);
} // namespace Palette

// ── Theme ──
// Dark (the default) or light; ApplyTheme also switches the MaterialDesign palette (scroll bars,
// tooltips). The choice is kept in theme.cfg.
bool IsLight();
// How strong coloured glows are drawn: a soft haze on the dark window, barely there on the light one
float GlowStrength();
void ApplyTheme(bool light);
// Reads theme.cfg (at startup) / writes it
void LoadTheme();
void SaveTheme();

// How far the slanted shapes lean: the top edge is shifted right by this much (DIPs)
inline constexpr float kSkew = 8.f;

// ── Shapes ──
// A parallelogram leaning right: top edge from x + skew, bottom edge to R() - skew
Microsoft::WRL::ComPtr<ID2D1Geometry> SlantGeometry(const Rect& r, float skew = kSkew);
void FillSlant(UI::Ui& ui, const Rect& r, Color color, float skew = kSkew);
void StrokeSlant(UI::Ui& ui, const Rect& r, Color color, float width, float skew = kSkew);
// A stripe `width` wide along the left edge of a slanted shape (`r`, `skew`): it leans with the edge
void EdgeStripe(UI::Ui& ui, const Rect& r, float width, Color color, float skew = kSkew);
// What follows is clipped to the parallelogram (close with ui.g.PopLayer())
void PushSlantClip(UI::Ui& ui, const Rect& r, float skew = kSkew);
// A band of light crossing the rect diagonally at `t` (0..1 = left to right)
void Sheen(UI::Ui& ui, const Rect& r, float t, float strength);
// The comic halftone: a grid of white dots at `opacity` over the rect
void Halftone(UI::Ui& ui, const Rect& r, float opacity);

// ── Text ──
// Condensed italic capitals for headings, numbers and buttons (Bahnschrift)
TextStyle Display(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_BOLD, bool italic = true);
// Text style anchored to the top (wrapping optional) and the height of one line of it
TextStyle Style(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, bool wrap = false, HAlign align = HAlign::Left);
float LineH(UI::Ui& ui, const TextStyle& style);
std::wstring Upper(const std::wstring& text);

// ── Buttons ──
struct ButtonStyle {
    Color background, hoverBackground, pressedBackground;
    Color foreground, hoverForeground;
    Color border = Colors::Transparent;
    float skew = kSkew;
    Edges padding{ 16.f, 7.f };
    float fontSize = 13.f;
    bool glow = false;    // glows in its colour on hover
    bool sheen = false;   // a light crosses it every few seconds (the main action)
};
// The red one: the main action ("＋ Add Mod", OK, Yes)
ButtonStyle Primary();
// Dark, the text lighting up on hover
ButtonStyle Ghost();
// Red text, a dark red fill on hover
ButtonStyle Danger();
// The dialogs' buttons (primary: red; otherwise ghost), 88 x 34
ButtonStyle DialogButton(bool primary);

TextStyle ButtonText(const ButtonStyle& style);
float ButtonWidth(UI::Ui& ui, const std::wstring& text, const ButtonStyle& style);
float ButtonHeight(UI::Ui& ui, const ButtonStyle& style);
// A slanted button with its label in capitals; true when clicked. Disabled: dimmed, no mouse.
bool Button(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, const ButtonStyle& style, bool enabled = true);

// ── Icons (drawn with lines, in `color`, `size` DIPs, centred) ──
enum class Glyph { Plus, Pencil, Trash, Person, Folder, Warning, Check, Box, Heart, Download, Search, External, Play };
void DrawGlyph(UI::Ui& ui, Glyph glyph, float cx, float cy, float size, Color color);
// A small slanted button with an icon (and a tooltip); `hover` is the icon's colour when hovered
bool IconButton(UI::Ui& ui, UI::Id id, const Rect& r, Glyph glyph, Color hover, const std::wstring& tooltip, bool enabled = true);
// A small chevron (▼) turned by `turn` (0 = down, 1 = up), centred on (cx, cy)
void Chevron(UI::Ui& ui, float cx, float cy, float size, float turn, Color color);

// ── Switch ──
// An on / off switch (the mod's Enable / Disable): the knob slides, the track lights up green.
// True when clicked.
bool Switch(UI::Ui& ui, UI::Id id, const Rect& r, bool on, bool enabled = true);

// ── Heroes ──
// The hero's portrait (Assets\Heroes\<id>.png), null for "" / an unknown id
ImagePtr HeroPortrait(const std::wstring& heroId);
// The hero's name, or "No character" for ""
std::wstring HeroName(const std::wstring& heroId);
// The hero's role colour (grey for "")
Color HeroColor(const std::wstring& heroId);
// The portrait filling `r` (cropped, the top kept: the face), `zoom` >= 1 grows it from its top
// centre; "" draws the "no character" tile (a big "?" on a dark gradient). Clipped to a rounded
// rect (`radius`), or to a slanted one with SlantPortrait
void Portrait(UI::Ui& ui, const std::wstring& heroId, const Rect& r, float radius, float zoom = 1.f, float opacity = 1.f);
void SlantPortrait(UI::Ui& ui, const std::wstring& heroId, const Rect& r, float skew, float zoom = 1.f, float opacity = 1.f);

// ── Motion ──
// 1 the moment `value` changes, fading to 0 over `duration` (0 the first time it's seen: nothing
// flashes at startup)
float Changed(UI::Ui& ui, UI::Id id, int64_t value, float duration);
// A number counting towards `value` (the header's stats)
float Count(UI::Ui& ui, UI::Id id, float value);

// ── Tooltip ──
// The tooltips of the whole window: a slanted card in the dialogs' colours with a red stripe and a
// little arrow pointing at the cursor; it rises and fades in (`age`, seconds since it appeared) and
// fades out before it hides (`left`, seconds left)
void DrawTooltip(Graphics& g, const std::wstring& text, float mouseX, float mouseY, float age, float left);

// The dialogs' text box (dark, red focus and selection)
UI::TextBoxStyle InputBoxStyle(float fontSize = 14.f);

} // namespace RivalsForge::Main::ModsKit

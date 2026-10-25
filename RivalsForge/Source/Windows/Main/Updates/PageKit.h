#pragma once
#include <string>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"
#include "Windows/Main/Mods/ModsKit.h"

// The pieces the Updates and Changelog pages are built from, in the window's look (slanted cards,
// condensed italic capitals, the Marvel red): the title row, the cards, their headings, numbered
// steps, info chips, the big status badge, arcs and progress bars, and the two-column layout.
namespace RivalsForge::Main::PageKit {

namespace M = UI::Material;

inline constexpr float kTitleH = 58.f, kGap = 14.f, kRightW = 330.f, kPad = 18.f;

// The page's title in big condensed capitals with the subtitle under it, and a pill on the right
// (an icon and a short text, e.g. "Installed v1.0.0")
void TitleRow(UI::Ui& ui, const Rect& r, const std::wstring& title, const std::wstring& subtitle, M::Icon pillIcon,
              const std::wstring& pill, Color pillColor);

// A card: the panel colour inside a 1 px line, a short accent line along its top
void Panel(UI::Ui& ui, const Rect& r, Color accent);
// A card's heading: a slanted tick and condensed capitals in `color`; returns its height
float Heading(UI::Ui& ui, float x, float y, float w, const std::wstring& text, Color color);
inline constexpr float kHeadingH = 22.f;

// Numbered steps joined by a line, coming in one by one from `shownAt`; returns their height
// (draw = false only measures)
float Steps(UI::Ui& ui, float x, float y, float w, const std::wstring* steps, int count, bool draw, double shownAt);

// "Label" over "value" in a slanted chip with an icon
void InfoChip(UI::Ui& ui, const Rect& r, M::Icon icon, const std::wstring& label, const std::wstring& value, Color color);

// The big status badge: a slanted tile in `color` with the halftone, a light crossing it and rings
// pulsing out of it (`rings`); the icon in the middle (turned / bobbing); `pop` 0..1 grows it in
void Badge(UI::Ui& ui, float cx, float cy, float size, Color color, M::Icon icon, bool rings, float pop, float iconAngle = 0.f,
           float iconBob = 0.f);

// An arc around (cx, cy): from `startDeg` (0 = up, clockwise) over `sweepDeg`
void Arc(UI::Ui& ui, float cx, float cy, float radius, float startDeg, float sweepDeg, Color color, float width);

// A slanted progress bar (0..1) with stripes running along the filled part
void Progress(UI::Ui& ui, const Rect& r, float value, Color color);

// The green button (the Play button's colours): Update now, Download & Install
ModsKit::ButtonStyle Go();

// A clickable line of text with an icon (the manual update link); true when clicked
bool Link(UI::Ui& ui, UI::Id id, const Rect& r, M::Icon icon, const std::wstring& text, HAlign align = HAlign::Center);

// The two columns under the title: side by side (the right one kRightW wide), or one under the
// other when the page is narrow
struct Columns {
    Rect left, right;
    bool stacked = false;
};
Columns Split(const Rect& area, float leftMinH);

} // namespace RivalsForge::Main::PageKit

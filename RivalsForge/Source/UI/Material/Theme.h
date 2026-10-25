#pragma once
#include <string>
#include "Graphics/Color.h"
#include "Graphics/Graphics.h"

// The near-black Marvel look of MainWindow.xaml (#0D0D0D window, #111 section cards, #161616 mod
// cards, red #C41E3A accent), laid out as the MaterialDesign brushes the shared widgets read, and
// its light counterpart (white cards on a pale grey window, the same red). No seasons.
namespace RivalsForge::UI::Material {

// A MaterialDesign ColorPair: the color and the contrasting text color on it.
struct ColorPair {
    Color color;
    Color foreground;
};

// The MaterialDesign brushes (MaterialDesign.Brush.*), named as in the toolkit.
struct Palette {
    bool baseDark = true;
    Color background;               // MaterialDesignPaper (RootBorder Background="#0D0D0D")
    Color foreground;               // MaterialDesignBody
    Color foregroundLight;          // MaterialDesignBodyLight / TextBox.Border
    Color divider;                  // MaterialDesignDivider
    Color cardBackground;           // section cards (#111)
    Color toolkitCard;              // popups, menus (the ContextMenu's #1E1E1E)
    Color cardBorder;
    ColorPair primaryLight, primaryMid, primaryDark;
    ColorPair secondaryLight, secondaryMid, secondaryDark;
    Color validationError;
    // Base-theme brushes
    Color textBoxOutlineBorder, textBoxOutlineInactiveBorder, textBoxHoverBorder, textBoxFilledBackground;
    Color comboOutlineBorder, comboOutlineInactiveBorder, comboHoverBorder;
    Color comboPopupBackground;
    Color checkBoxUncheckedBorder, checkBoxDisabled;
    Color switchTrackOff;
    Color flatClick, buttonRipple;
    Color separator;
    Color scrollBarForeground;      // the slim ScrollBar thumb (#333)
    Color snackbarBackground, snackbarMouseOver;
    Color toolTipBackground;
    Color listBoxItemSelected, listViewHover;
    Color tabDivider;
    Color sectionHeader;
};

// The palette (built on first use).
const Palette& P();
// Increments whenever the palette changes (screens can cache derived values).
unsigned Revision();
// Switches between the dark palette (the default) and the light one
void SetLightTheme(bool light);
bool IsLightTheme();

// MaterialDesign ColorAssist.ContrastingForegroundColor
Color ContrastingForeground(Color c);

// ── Typography (MaterialDesign TextBlock styles; Segoe UI, see Graphics/Fonts.h) ──
TextStyle Headline4();   // 34 Regular
TextStyle Headline5();   // 24 Regular
TextStyle Headline6();   // 20 Medium
TextStyle Subtitle1();   // 16 Regular
TextStyle Subtitle2();   // 14 Medium
TextStyle Body1();       // 16 Regular
TextStyle Body2();       // 14 Regular
TextStyle Caption();     // 12 Regular
TextStyle Text(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);
inline TextStyle Medium(float size) { return Text(size, DWRITE_FONT_WEIGHT_MEDIUM); }
inline TextStyle SemiBold(float size) { return Text(size, DWRITE_FONT_WEIGHT_SEMI_BOLD); }
inline TextStyle Bold(float size) { return Text(size, DWRITE_FONT_WEIGHT_BOLD); }

} // namespace RivalsForge::UI::Material

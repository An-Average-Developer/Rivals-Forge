#include "UI/Material/Theme.h"
#include <cmath>

namespace RivalsForge::UI::Material {

namespace {

Palette palette;
unsigned revision = 0;
bool built = false;
bool lightTheme = false;

ColorPair Pair(Color c) { return { c, ContrastingForeground(c) }; }

// MaterialDesignTheme.Light's brushes in the same spirit: white cards, grey lines, the same red
void BuildLight() {
    Palette p;
    p.baseDark = false;
    p.background = Hex(0xEDEDF2);
    p.foreground = Hex(0x15151A);
    p.foregroundLight = Hex(0x6A6A76);
    p.divider = Hex(0xE2E2E8);
    p.cardBackground = Hex(0xFFFFFF);
    p.toolkitCard = Hex(0xFFFFFF);
    p.cardBorder = Hex(0xD4D4DC);
    p.primaryLight = Pair(Hex(0xD62845));
    p.primaryMid = Pair(Hex(0xC41E3A));
    p.primaryDark = Pair(Hex(0xA01830));
    p.secondaryLight = Pair(Hex(0xEDEDF2));
    p.secondaryMid = Pair(Hex(0xE2E2E8));
    p.secondaryDark = Pair(Hex(0xD4D4DC));
    p.validationError = Hex(0xD32F2F);
    p.textBoxOutlineBorder = Hex(0xCDCDD5);
    p.textBoxOutlineInactiveBorder = Hex(0xDDDDE3);
    p.textBoxHoverBorder = Hex(0xA8A8B2);
    p.textBoxFilledBackground = Hex(0xF2F2F5);
    p.comboOutlineBorder = Hex(0xCDCDD5);
    p.comboOutlineInactiveBorder = Hex(0xDDDDE3);
    p.comboHoverBorder = Hex(0xA8A8B2);
    p.comboPopupBackground = Hex(0xFFFFFF);
    p.checkBoxUncheckedBorder = Hex(0xA8A8B2);
    p.checkBoxDisabled = Hex(0xD4D4DC);
    p.switchTrackOff = Hex(0xD4D4DC);
    p.flatClick = Argb(0x19000000);
    p.buttonRipple = Hex(0x6A6A76);
    p.separator = Hex(0xE2E2E8);
    p.scrollBarForeground = Hex(0xC4C4CC);
    p.snackbarBackground = Hex(0x2A2A32);
    p.snackbarMouseOver = Hex(0x3A3A44);
    p.toolTipBackground = Hex(0x2A2A32);
    p.listBoxItemSelected = Argb(0x19000000);
    p.listViewHover = Hex(0xEEEEF3);
    p.tabDivider = Hex(0xE2E2E8);
    p.sectionHeader = p.primaryMid.color;
    palette = p;
    built = true;
    ++revision;
}

// MaterialDesignTheme.Dark's brushes over the colours of MainWindow.xaml / App.xaml
void Build() {
    if (lightTheme) {
        BuildLight();
        return;
    }
    Palette p;
    p.baseDark = true;
    p.background = Hex(0x0D0D0D);
    p.foreground = Hex(0xFFFFFF);
    p.foregroundLight = Hex(0x888888);
    p.divider = Hex(0x1A1A1A);            // <Rectangle Fill="#1A1A1A"/> separators
    p.cardBackground = Hex(0x111111);     // section card
    p.toolkitCard = Hex(0x1E1E1E);        // ContextMenu Background
    p.cardBorder = Hex(0x333333);         // ContextMenu BorderBrush
    // Marvel red: hover #E02040, pressed #A01830 (PrimaryBtn)
    p.primaryLight = Pair(Hex(0xE02040));
    p.primaryMid = Pair(Hex(0xC41E3A));
    p.primaryDark = Pair(Hex(0xA01830));
    p.secondaryLight = Pair(Hex(0x303030));
    p.secondaryMid = Pair(Hex(0x222222));
    p.secondaryDark = Pair(Hex(0x1A1A1A));
    p.validationError = Hex(0xFF5555);    // DangerBtn Foreground
    p.textBoxOutlineBorder = Hex(0x3A3A3A);   // InputDialog's TextBox BorderBrush
    p.textBoxOutlineInactiveBorder = Hex(0x2A2A2A);
    p.textBoxHoverBorder = Hex(0x555555);
    p.textBoxFilledBackground = Hex(0x252525);   // TextBox Background
    p.comboOutlineBorder = Hex(0x3A3A3A);
    p.comboOutlineInactiveBorder = Hex(0x2A2A2A);
    p.comboHoverBorder = Hex(0x555555);
    p.comboPopupBackground = Hex(0x1E1E1E);
    p.checkBoxUncheckedBorder = Hex(0x444444);
    p.checkBoxDisabled = Hex(0x333333);
    p.switchTrackOff = Hex(0x2A2A2A);
    p.flatClick = Argb(0x19FFFFFF);
    p.buttonRipple = Hex(0xB9B9B9);
    p.separator = Hex(0x1A1A1A);
    p.scrollBarForeground = Hex(0x333333);   // Thumb Background="#333"
    p.snackbarBackground = Hex(0x1E1E1E);
    p.snackbarMouseOver = Hex(0x2E2E2E);
    p.toolTipBackground = Hex(0x1E1E1E);
    p.listBoxItemSelected = Argb(0x19FFFFFF);
    p.listViewHover = Hex(0x2E2E2E);         // MenuItem IsMouseOver
    p.tabDivider = Hex(0x1A1A1A);
    p.sectionHeader = p.primaryMid.color;
    palette = p;
    built = true;
    ++revision;
}

} // namespace

const Palette& P() {
    if (!built) Build();
    return palette;
}

unsigned Revision() {
    if (!built) Build();
    return revision;
}

void SetLightTheme(bool light) {
    if (built && light == lightTheme) return;
    lightTheme = light;
    Build();
}

bool IsLightTheme() { return lightTheme; }

Color ContrastingForeground(Color c) {
    const auto lin = [](float v) { return v > 0.03928f ? std::pow((v + 0.055f) / 1.055f, 2.4f) : v / 12.92f; };
    const double luminance = 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
    return luminance > 0.179 ? Colors::Black : Colors::White;
}

TextStyle Text(float size, DWRITE_FONT_WEIGHT weight) {
    TextStyle s;
    s.size = size;
    s.font = Font::Ui;
    s.weight = weight;
    return s;
}

TextStyle Headline4() { return Text(34.f); }
TextStyle Headline5() { return Text(24.f); }
TextStyle Headline6() { return Text(20.f, DWRITE_FONT_WEIGHT_MEDIUM); }
TextStyle Subtitle1() { return Text(16.f); }
TextStyle Subtitle2() { return Text(14.f, DWRITE_FONT_WEIGHT_MEDIUM); }
TextStyle Body1() { return Text(16.f); }
TextStyle Body2() { return Text(14.f); }
TextStyle Caption() { return Text(12.f); }

} // namespace RivalsForge::UI::Material

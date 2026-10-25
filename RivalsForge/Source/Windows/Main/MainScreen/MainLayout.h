#pragma once
#include "Graphics/Rect.h"

namespace RivalsForge::Main {

// MainWindow.xaml's grid (MinHeight 520, MinWidth 720), the rows
// 36 (title bar) / Auto (header) / 1 (separator) / * (content) / Auto (status bar).
struct MainLayout {
    // Opens wide (the C#'s was 960 x 720), shrunk to the screen's work area when it's smaller
    static constexpr float WindowWidth = 1320.f, WindowHeight = 860.f;
    static constexpr float MinWidth = 720.f, MinHeight = 520.f;
    static constexpr float Radius = 10.f;            // RootBorder CornerRadius (0 maximized)
    static constexpr float TitleBarHeight = 36.f;
    static constexpr float HeaderHeight = 104.f;     // the title and the counters under it
    static constexpr float StatusBarHeight = 34.f;   // Padding 28,7 around the folder buttons
    static constexpr float SideMargin = 28.f;
    static constexpr float WindowButtonWidth = 46.f; // WinBtn Width / Height 46 x 36

    Rect window, titleBar, header, separator, content, statusBar;

    static MainLayout Compute(const Rect& window) {
        MainLayout l;
        l.window = window;
        l.titleBar = { window.x, window.y, window.w, TitleBarHeight };
        l.header = { window.x, l.titleBar.B(), window.w, HeaderHeight };
        l.separator = { window.x, l.header.B(), window.w, 1.f };
        l.statusBar = { window.x, window.B() - StatusBarHeight, window.w, StatusBarHeight };
        l.content = { window.x, l.separator.B(), window.w, l.statusBar.y - l.separator.B() };
        return l;
    }
    // The title bar the window is dragged by (all of it but the three window buttons)
    Rect Caption() const { return { titleBar.x, titleBar.y, titleBar.w - WindowButtonWidth * 3.f, titleBar.h }; }
};

} // namespace RivalsForge::Main

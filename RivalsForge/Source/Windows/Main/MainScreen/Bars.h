#pragma once
#include "Graphics/Rect.h"
#include "UI/Ui.h"

namespace RivalsForge::Main {

class MainWindow;

// ═══ TITLE BAR ═══: the logo, "RIVALS FORGE  MOD MANAGER" and the — / □ (❐) / ✕ window buttons. Dragging
// and double-click to maximize come from the window's caption hit test (MainLayout::Caption).
namespace TitleBar {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar);
}

// ═══ HEADER ═══: "INSTALLED MODS" with its counters (mods, enabled, heroes), the language flag and
// "＋ Add Mod" (enabled once the game is configured; each mod goes to the hero found in its ZIP).
namespace Header {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& header);
}

// ═══ STATUS BAR ═══: the status message (sliding in when it changes), the progress of a running
// installation, and "📁 Mods Folder" / "📁 Paks Folder".
namespace StatusBar {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar);
}

} // namespace RivalsForge::Main

#pragma once
#include <string>
#include "Graphics/Graphics.h"

namespace RivalsForge::UI::Material {

// MaterialDesignToolTip: ToolTip.Background box, Brush.Background text, Padding 8, rounded corners,
// shown under the element the mouse rests on (Ui::tooltipRenderer).
void DrawToolTip(Graphics& g, const std::wstring& text, float mouseX, float mouseY);

} // namespace RivalsForge::UI::Material

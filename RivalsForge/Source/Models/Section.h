#pragma once
#include <string>

namespace RivalsForge::Models {

// Models\Section.cs
struct Section {
    std::wstring id;                     // Guid.NewGuid().ToString()
    std::wstring name;
    std::wstring icon = L"\U0001F4E6";   // "📦"
    bool isExpanded = true;
};

} // namespace RivalsForge::Models

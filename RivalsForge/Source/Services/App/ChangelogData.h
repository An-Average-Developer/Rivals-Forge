#pragma once
#include <string>
#include <vector>

// The in-app changelog (newest first), as in the Inazuma Eleven VR Save Editor.
namespace RivalsForge::Services::Changelog {

struct Entry {
    std::wstring version;
    std::wstring label;                // shown after the version ("[C++ UPDATE!]"), usually empty
    std::vector<std::wstring> lines;   // "## Additions" section headers and "- change" lines
};

const std::vector<Entry>& Entries();

// The whole changelog as changelog.txt text ("1.0.1\n## Additions\n- ...\n\n1.0.0\n...", trimmed):
// what the published changelog.txt holds, and what's shown when GitHub can't be reached.
std::wstring EmbeddedText();

} // namespace RivalsForge::Services::Changelog

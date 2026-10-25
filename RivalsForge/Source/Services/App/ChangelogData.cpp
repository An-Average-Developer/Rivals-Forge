// Rivals Forge changelog. Add new versions at the TOP (newest first).
// Each entry: version, label shown after it (e.g. "[C++ UPDATE!]", usually empty) and its lines:
// "## " section headers (Additions, Redesigns, Improvements, Performance, Bug Fixes, Other) and
// "- " changes. The published changelog.txt uses the same text (EmbeddedText), so copy it there
// when releasing a version.
#include "Services/App/ChangelogData.h"
#include "Core/Text.h"

namespace RivalsForge::Services::Changelog {

const std::vector<Entry>& Entries() {
    static const std::vector<Entry> entries = {
        { L"1.0.1", L"", {
            L"## Additions",
            L"- GameBanana: browse, search and install Marvel Rivals mods from GameBanana too, no account needed",
            L"- Old mods: mods packed for an older version of the game get a red OLD tag, so you can tell which ones the game's updates stopped loading",
            L"- Game open: installing, enabling or deleting a mod while Marvel Rivals is running offers to close the game first, then carries on by itself",
            L"## Redesigns",
            L"- Mod Store: the Nexus Store is now the Mod Store, and its header lets you choose between Nexus Mods and GameBanana (your choice is remembered)",
            L"## Improvements",
            L"- Files in use: the error now says which file is busy and reminds you to close Marvel Rivals if it's open",
        } },
        { L"1.0.0", L"[RIVALS FORGE!]", {
            L"## Additions",
            L"- Nexus Store: browse, search and install Marvel Rivals mods from Nexus Mods without leaving the manager",
            L"- Heroes: mods are grouped by hero, with a strip on top to show one hero's mods",
            L"- Steam and Epic Games: every installation is found and you can move your mods between them",
            L"- Play: launch the game from the header",
            L"- UTOC Signature Bypass Patch: the header shows whether it's installed and helps you install it",
            L"- Themes: switch between the dark and the light theme with the sun / moon button in the header",
            L"- Updates: the manager checks GitHub at startup and installs new versions by itself",
            L"- Changelog: every version and its changes, inside the manager",
            L"- Languages: English, Spanish, French and Italian",
        } },
    };
    return entries;
}

std::wstring EmbeddedText() {
    std::wstring text;
    for (const auto& entry : Entries()) {
        if (!text.empty()) text += L"\n\n";
        text += entry.label.empty() ? entry.version : entry.version + L" " + entry.label;
        for (size_t i = 0; i < entry.lines.size(); ++i) {
            // A blank line between sections, like the published changelog.txt
            if (i > 0 && entry.lines[i].rfind(L"## ", 0) == 0) text += L"\n";
            text += L"\n" + entry.lines[i];
        }
    }
    return Text::Trim(text);
}

} // namespace RivalsForge::Services::Changelog

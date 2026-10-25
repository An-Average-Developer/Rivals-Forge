#pragma once
#include <optional>
#include <string>

// The user's Nexus Mods side: their personal API key (kept in DataDir\nexus.key, encrypted for the
// Windows user with DPAPI) and the nxm:// links the site's "Mod Manager Download" button opens,
// which this app can be set to receive (HKCU\Software\Classes\nxm; the program that had them before,
// Vortex say, is remembered and given them back when it's turned off).
namespace RivalsForge::Services::Store::NexusAccount {

// "" when there's none
std::wstring LoadKey();
// "" forgets it
void SaveKey(const std::wstring& key);

// nxm://marvelrivals/mods/1233/files/4567?key=...&expires=...&user_id=...
struct NxmLink {
    std::wstring game;
    long long modId = 0, fileId = 0;
    std::wstring key, expires;
};
std::optional<NxmLink> ParseNxm(const std::wstring& url);

// Whether nxm:// links open this exe
bool HandlerIsOurs();
// Points nxm:// links at this exe; false when the registry can't be written
bool RegisterHandler();
// Gives them back to the program that had them (or removes the association)
void UnregisterHandler();
// The links pointed at this app under its old exe name (MarvelRivalsModManager.exe): pointed at
// this exe again
void RepairHandler();

} // namespace RivalsForge::Services::Store::NexusAccount

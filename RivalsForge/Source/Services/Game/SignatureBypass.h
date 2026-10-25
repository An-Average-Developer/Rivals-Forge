#pragma once
#include <string>
#include "Services/Game/GamePathService.h"

// "UTOC Signature Bypass Patch for Marvel Rivals" by DeathChaos (Nexus Mods, mod 2940): the game
// checks the signatures of its .utoc files, so mods don't load without it. It's an ASI plugin and
// Ultimate ASI Loader's dsound.dll, unpacked into the game's MarvelGame\Marvel\Binaries\Win64 from
// the copy built into the app (Assets\Tools\UTOC Signature Bypass Patch.zip).
namespace RivalsForge::Services::Game::SignatureBypass {

inline constexpr const wchar_t* kAuthor = L"DeathChaos";
inline constexpr const wchar_t* kName = L"UTOC Signature Bypass Patch for Marvel Rivals";
inline constexpr const wchar_t* kPage = L"https://www.nexusmods.com/marvelrivals/mods/2940?tab=description";

// The installation's MarvelGame\Marvel\Binaries\Win64
std::wstring BinariesDir(const GamePath::Installation& installation);
// Both files are there
bool IsInstalled(const GamePath::Installation& installation);
// Unpacks the patch there (overwriting); throws Mods::ModError (the game running holds the files)
void Install(const GamePath::Installation& installation);

} // namespace RivalsForge::Services::Game::SignatureBypass

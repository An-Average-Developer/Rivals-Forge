#pragma once
#include <string>

// Well-known folders. The data lived in %LOCALAPPDATA%\MarvelRivalsModManager (the C# manager's
// folder) until the app became Rivals Forge: MigrateDataFolder moves it once.
namespace RivalsForge::Paths {

// %LOCALAPPDATA%\RivalsForge — data.json, MODS\, language.cfg, logs\ (ModService.DataDir).
// Debug builds honour RF_DATA_DIR (App\Config.h).
const std::wstring& DataDir();
// Before anything reads DataDir: %LOCALAPPDATA%\MarvelRivalsModManager becomes DataDir when DataDir
// isn't there yet (data.json, MODS, the Nexus key... all of it), and the old %TEMP% assets go
void MigrateDataFolder();
std::wstring DataFile(const std::wstring& name);
// DataDir\MODS (ModService.ModsStorageDir)
std::wstring ModsDir();
// %APPDATA%\MarvelRivalsModManager — the C# app's old location, only read by the one-time migration
// (empty while RF_DATA_DIR redirects the data)
std::wstring OldRoamingDir();

// %TEMP%\RivalsForge — embedded assets are unpacked here on startup
const std::wstring& TempDir();
std::wstring Asset(const std::wstring& relative);          // TempDir\Assets\<relative>
std::wstring Translation(const std::wstring& fileName);    // TempDir\Translations\<file>

} // namespace RivalsForge::Paths

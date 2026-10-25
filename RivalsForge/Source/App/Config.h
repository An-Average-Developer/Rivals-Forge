#pragma once

// Every fixed name and path the manager uses, in one place (the C# kept them as constants in
// ModService.cs and GamePathService.cs). The services reference these instead of keeping their own
// copies.
namespace RivalsForge::Config {

// ── Presentation mode (screenshots, recordings, streams) ────────────────────
// true: nothing personal shows on screen. The Windows user name in every path (C:\Users\<name>\...)
// and the Nexus Mods account name read "an-average-developer" (PresentationName), and the Nexus API
// key box shows dots. Only what's drawn changes: the real paths and names are used as always.
inline constexpr bool PresentationMode = false;
inline constexpr const wchar_t* PresentationName = L"an-average-developer";

// ── App data (ModService.DataDir / ModsStorageDir) ──────────────────────────
// %LOCALAPPDATA%\<DataFolder>: data.json, the MODS folder and the C++ port's own files (language.cfg,
// logs\); %TEMP%\<DataFolder> holds the unpacked assets. Before the app was Rivals Forge it was the
// Marvel Rivals Mod Manager: %LOCALAPPDATA%\<LegacyDataFolder> is moved to the new folder once
// (Paths::MigrateDataFolder), and %APPDATA%\<LegacyDataFolder> is the C# app's old Roaming location,
// migrated once too (MigrateFromRoaming)
inline constexpr const wchar_t* DataFolder = L"RivalsForge";
inline constexpr const wchar_t* LegacyDataFolder = L"MarvelRivalsModManager";
inline constexpr const wchar_t* DataFileName = L"data.json";
inline constexpr const wchar_t* LegacyFileName = L"mods.json";   // the list of mods before data.json
inline constexpr const wchar_t* ModsFolder = L"MODS";            // where the mod ZIPs are kept
inline constexpr const wchar_t* ZipExtension = L".zip";          // an enabled mod's archive
inline constexpr const wchar_t* ZipOffExtension = L".zip.off";   // a disabled mod's archive
// Where a test version kept disabled mods' files (next to Paks, a folder per mod id): removed
// when such a mod is enabled or deleted
inline constexpr const wchar_t* DisabledFolder = L"~MRMM Disabled";
inline constexpr const wchar_t* LanguageFileName = L"language.cfg";
inline constexpr const wchar_t* CollapsedFileName = L"collapsed.cfg";   // the hero groups folded shut
#ifdef _DEBUG
// Development aid: RF_DATA_DIR=<folder> keeps data.json and MODS there instead of
// %LOCALAPPDATA% (tests with a copy of a user's data); the Roaming migration is skipped then
inline constexpr const wchar_t* DataDirOverrideVariable = L"RF_DATA_DIR";
#endif

// ── Game (GamePathService.cs) ───────────────────────────────────────────────
// From any Steam library root to the game folder
inline constexpr const wchar_t* GameSubPath = L"steamapps\\common\\MarvelRivals\\MarvelGame";
// From MarvelGame to the Paks directory
inline constexpr const wchar_t* PaksSubPath = L"Marvel\\Content\\Paks";
// The folder names ResolvePaksFromSelection steps through
inline constexpr const wchar_t* MarvelGameFolder = L"MarvelGame";
inline constexpr const wchar_t* PaksFolder = L"Paks";
inline constexpr const wchar_t* ContentFolder = L"Content";
inline constexpr const wchar_t* MarvelFolder = L"Marvel";
inline constexpr const wchar_t* ContentPaksSubPath = L"Content\\Paks";
// From Content to the game's intro videos (what the "skip intro" mods replace)
inline constexpr const wchar_t* IntroVideosSubPath = L"Marvel\\MoviesBink\\Movies\\MarvelLogo";

// ── Epic Games: its install manifests (one .item JSON per game) ──────────────
inline constexpr const wchar_t* EpicManifestsFolder = L"Epic\\EpicGamesLauncher\\Data\\Manifests";   // under %ProgramData%
inline constexpr const wchar_t* EpicGameName = L"Marvel Rivals";                                     // the manifest's DisplayName
// From an installation's root folder (Steam's ...\common\MarvelRivals, Epic's InstallLocation)
inline constexpr const wchar_t* GameFolder = L"MarvelGame";
inline constexpr const wchar_t* LauncherExe = L"MarvelRivals_Launcher.exe";

// ── Steam (GetSteamInstallPath / GetSteamLibraries) ─────────────────────────
inline constexpr const wchar_t* SteamKeyWow64 = L"SOFTWARE\\WOW6432Node\\Valve\\Steam";   // HKLM, InstallPath
inline constexpr const wchar_t* SteamKey = L"SOFTWARE\\Valve\\Steam";                      // HKLM InstallPath, HKCU SteamPath
inline constexpr const wchar_t* SteamInstallPathValue = L"InstallPath";
inline constexpr const wchar_t* SteamPathValue = L"SteamPath";
inline constexpr const wchar_t* SteamAppsFolder = L"steamapps";
inline constexpr const wchar_t* LibraryFoldersFile = L"libraryfolders.vdf";

// ── Updates (UpdateService) ─────────────────────────────────────────────────
// The repository the manager updates from: version.txt and changelog.txt at the root of `Branch`
// (raw.githubusercontent.com). Each release carries the zip RivalsForgev<x-y-z>.zip (ZipPrefix +
// the version with dashes, the exe inside: what the automatic update downloads) and the bare exe
// (LatestExeUrl: what the manual update downloads)
namespace GitHub {
inline constexpr const wchar_t* Owner = L"An-Average-Developer";
inline constexpr const wchar_t* Repo = L"Rivals-Forge";
inline constexpr const wchar_t* Branch = L"main";
inline constexpr const wchar_t* ExeAsset = L"RivalsForge.exe";
inline constexpr const wchar_t* ZipPrefix = L"RivalsForgev";
inline constexpr const wchar_t* LatestExeUrl = L"https://github.com/An-Average-Developer/Rivals-Forge/releases/latest/download/RivalsForge.exe";
} // namespace GitHub
// Where the update is staged (under %TEMP%) before the helper batch swaps it in
inline constexpr const wchar_t* UpdateFolder = L"RivalsForge_Update";

} // namespace RivalsForge::Config

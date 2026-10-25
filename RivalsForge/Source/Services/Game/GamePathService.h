#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Services\GamePathService.cs: finds Marvel Rivals' Paks directory in the Steam libraries.
namespace RivalsForge::Services::GamePath {

// Tries to auto-detect the Paks directory by scanning all Steam library folders: the first library
// with steamapps\common\MarvelRivals\MarvelGame wins, and its Marvel\Content\Paks is created when
// missing; without one, Epic's installation (new). nullopt when not found.
std::optional<std::wstring> TryDetect();

// An installation of the game: Steam's or Epic's (or a folder the user chose)
enum class Store { Steam, Epic, Other };
struct Installation {
    Store store = Store::Other;
    std::wstring root;       // ...\MarvelRivals (Steam) / Epic's InstallLocation
    std::wstring paks;       // root\MarvelGame\Marvel\Content\Paks
    std::wstring launcher;   // root\MarvelRivals_Launcher.exe
};
// Every installation on this PC: Steam's libraries, then Epic's manifests
std::vector<Installation> FindInstallations();
// The installation a Paks folder belongs to (one of FindInstallations, else worked out from the path)
Installation Describe(const std::wstring& paks, const std::vector<Installation>& known);
// The store's name ("Steam", "Epic Games", or the folder's)
std::wstring StoreName(const Installation& installation);
// Starts the game from the installation's launcher; false (and `error`) when it can't
bool Launch(const Installation& installation, std::wstring& error);
// The game's processes running now (the client, its launcher, anything started from an
// installation's folder): while there are any, its files in Paks are locked
std::vector<uint32_t> RunningProcesses(const std::vector<Installation>& installations);

// Given a folder the user browsed to, resolves the Paks directory. Accepts the Paks folder itself,
// MarvelGame, MarvelRivals, Content or Marvel (checked in that order).
std::optional<std::wstring> ResolvePaksFromSelection(std::wstring selectedFolder);

} // namespace RivalsForge::Services::GamePath

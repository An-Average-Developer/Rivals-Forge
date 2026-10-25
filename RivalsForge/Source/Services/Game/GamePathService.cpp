#include "Services/Game/GamePathService.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <vector>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Services/App/Log.h"

namespace RivalsForge::Services::GamePath {

namespace {

// ReadRegistry: the value as a string, nullopt when the key or a string value isn't there
std::optional<std::wstring> ReadRegistry(System::Hive hive, const wchar_t* key, const wchar_t* value) {
    return System::RegistryString(hive, key, value);
}

// HKLM WOW6432Node InstallPath ?? HKLM InstallPath ?? HKCU SteamPath
std::optional<std::wstring> GetSteamInstallPath() {
    if (auto path = ReadRegistry(System::Hive::LocalMachine, Config::SteamKeyWow64, Config::SteamInstallPathValue)) return path;
    if (auto path = ReadRegistry(System::Hive::LocalMachine, Config::SteamKey, Config::SteamInstallPathValue)) return path;
    return ReadRegistry(System::Hive::CurrentUser, Config::SteamKey, Config::SteamPathValue);
}

bool IsNullOrWhiteSpace(const std::wstring& s) {
    for (wchar_t c : s)
        if (!std::iswspace(c)) return false;
    return true;
}

// Regex.Matches(vdf, "\"(?:path|\\d+)\"\\s+\"(.+?)\""): the values of "path" keys (new format) and
// of numbered keys (old format), each taken up to the next quote on the same line
std::vector<std::wstring> LibraryPaths(const std::wstring& vdf) {
    std::vector<std::wstring> found;
    const size_t n = vdf.size();
    size_t i = 0;
    while (i < n) {
        if (vdf[i] != L'"') {
            ++i;
            continue;
        }
        // "path" or "<digits>"
        size_t p = i + 1;
        if (vdf.compare(p, 4, L"path") == 0 && p + 4 < n && vdf[p + 4] == L'"') {
            p += 4;
        } else {
            const size_t digits = p;
            while (p < n && std::iswdigit(vdf[p])) ++p;
            if (p == digits || p >= n || vdf[p] != L'"') {
                ++i;
                continue;
            }
        }
        ++p;   // closing quote of the key
        // \s+ then the opening quote of the value
        const size_t space = p;
        while (p < n && std::iswspace(vdf[p])) ++p;
        if (p == space || p >= n || vdf[p] != L'"') {
            ++i;
            continue;
        }
        // (.+?)" : at least one character, '.' doesn't cross a line break
        const size_t start = p + 1;
        if (start >= n || vdf[start] == L'\n') {
            ++i;
            continue;
        }
        size_t q = start + 1;
        while (q < n && vdf[q] != L'"' && vdf[q] != L'\n') ++q;
        if (q >= n || vdf[q] != L'"') {
            ++i;
            continue;
        }
        found.push_back(vdf.substr(start, q - start));
        i = q + 1;
    }
    return found;
}

// The Steam root first, then the libraries listed in libraryfolders.vdf
std::vector<std::wstring> GetSteamLibraries() {
    std::vector<std::wstring> libraries;
    const auto steamPath = GetSteamInstallPath();
    if (!steamPath) return libraries;
    libraries.push_back(*steamPath);

    const std::wstring vdfPath = Files::DotNetCombine(Files::DotNetCombine(*steamPath, Config::SteamAppsFolder), Config::LibraryFoldersFile);
    if (!Files::Exists(vdfPath)) return libraries;
    const auto vdf = Files::ReadText(vdfPath);
    if (!vdf) return libraries;
    for (const auto& match : LibraryPaths(*vdf)) {
        const std::wstring path = Text::Replace(match, L"\\\\", L"\\");
        if (!IsNullOrWhiteSpace(path) && path != *steamPath) libraries.push_back(path);
    }
    return libraries;
}

// Directory.Exists(Path.GetDirectoryName(path))
bool DirectoryParentExists(const std::wstring& path) { return Files::DirectoryExists(Files::DirectoryName(path)); }

} // namespace

namespace {

std::wstring TrimSlashes(std::wstring path) {
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    return path;
}

Installation FromRoot(Store store, const std::wstring& root) {
    Installation installation;
    installation.store = store;
    installation.root = TrimSlashes(root);
    installation.paks = Files::DotNetCombine(Files::DotNetCombine(installation.root, Config::GameFolder), Config::PaksSubPath);
    installation.launcher = Files::DotNetCombine(installation.root, Config::LauncherExe);
    return installation;
}

// Epic's manifests: the one whose DisplayName is the game's, at its InstallLocation
std::vector<Installation> EpicInstallations() {
    std::vector<Installation> found;
    wchar_t programData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH)) return found;
    const std::wstring folder = Files::Combine(programData, Config::EpicManifestsFolder);
    for (const auto& file : Files::ListFiles(folder, L"*.item")) {
        const auto text = Files::ReadText(file);
        if (!text) continue;
        const Json::Value manifest = Json::Parse(*text);
        if (!Text::Equals(manifest[std::string_view("DisplayName")].WStr(), Config::EpicGameName, true)) continue;
        const std::wstring location = manifest[std::string_view("InstallLocation")].WStr();
        if (location.empty() || !Files::DirectoryExists(Files::DotNetCombine(location, Config::GameFolder))) continue;
        found.push_back(FromRoot(Store::Epic, location));
    }
    return found;
}

} // namespace

std::optional<std::wstring> TryDetect() {
#ifdef _DEBUG
    // Development aid: RF_NO_DETECT=1 acts as if the game weren't installed (the setup screen)
    if (GetEnvironmentVariableW(L"RF_NO_DETECT", nullptr, 0) > 0) return std::nullopt;
#endif
    for (const auto& library : GetSteamLibraries()) {
        const std::wstring marvelGame = Files::DotNetCombine(library, Config::GameSubPath);
        if (!Files::DirectoryExists(marvelGame)) continue;
        const std::wstring paks = Files::DotNetCombine(marvelGame, Config::PaksSubPath);
        // Creates it only if missing (the C# threw when it couldn't; here it's logged)
        if (!Files::CreateDirectories(paks)) Log::Warning(L"Could not create the Paks folder: " + paks);
        return paks;
    }
    // No Steam installation: Epic's
    const auto epic = EpicInstallations();
    if (epic.empty()) return std::nullopt;
    if (!Files::CreateDirectories(epic.front().paks)) Log::Warning(L"Could not create the Paks folder: " + epic.front().paks);
    return epic.front().paks;
}


std::vector<Installation> FindInstallations() {
    std::vector<Installation> found;
#ifdef _DEBUG
    if (GetEnvironmentVariableW(L"RF_NO_DETECT", nullptr, 0) > 0) return found;
#endif
    for (const auto& library : GetSteamLibraries()) {
        const std::wstring marvelGame = Files::DotNetCombine(library, Config::GameSubPath);
        if (Files::DirectoryExists(marvelGame)) found.push_back(FromRoot(Store::Steam, Files::DirectoryName(marvelGame)));
    }
    for (auto& epic : EpicInstallations()) found.push_back(std::move(epic));
    // The same folder twice (a library listed twice): once
    std::vector<Installation> unique;
    for (auto& i : found)
        if (std::none_of(unique.begin(), unique.end(), [&](const Installation& u) { return Text::Equals(u.paks, i.paks, true); }))
            unique.push_back(std::move(i));
    return unique;
}

Installation Describe(const std::wstring& paks, const std::vector<Installation>& known) {
    for (const auto& i : known)
        if (Text::Equals(TrimSlashes(i.paks), TrimSlashes(paks), true)) return i;
    // ...\<root>\MarvelGame\Marvel\Content\Paks: four folders up
    std::wstring root = TrimSlashes(paks);
    for (int i = 0; i < 4 && !root.empty(); ++i) root = Files::DirectoryName(root);
    Installation installation = FromRoot(Store::Other, root);
    installation.paks = paks;
    return installation;
}

std::wstring StoreName(const Installation& installation) {
    switch (installation.store) {
        case Store::Steam: return L"Steam";
        case Store::Epic: return L"Epic Games";
        default: return Files::FileName(installation.root);
    }
}

bool Launch(const Installation& installation, std::wstring& error) {
    if (!Files::Exists(installation.launcher)) {
        error = installation.launcher;
        return false;
    }
    if (!Process::Open(installation.launcher, L"", installation.root)) {
        error = installation.launcher;
        return false;
    }
    Log::Info(L"Game started: " + installation.launcher);
    return true;
}

std::optional<std::wstring> ResolvePaksFromSelection(std::wstring selectedFolder) {
    if (IsNullOrWhiteSpace(selectedFolder)) return std::nullopt;
    while (!selectedFolder.empty() && (selectedFolder.back() == L'\\' || selectedFolder.back() == L'/')) selectedFolder.pop_back();

    // They selected Paks directly: a folder named exactly Paks inside Content (the C#'s
    // EndsWith("Paks") also took "MyPaks" or "SomethingPaks")
    if (Text::Equals(Files::FileName(selectedFolder), Config::PaksFolder, true) &&
        Text::Equals(Files::FileName(Files::DirectoryName(selectedFolder)), Config::ContentFolder, true))
        return selectedFolder;

    // They selected MarvelGame
    const std::wstring fromMarvelGame = Files::DotNetCombine(selectedFolder, Config::PaksSubPath);
    if (DirectoryParentExists(fromMarvelGame)) return fromMarvelGame;

    // They selected MarvelRivals
    const std::wstring fromMarvelRivals = Files::DotNetCombine(Files::DotNetCombine(selectedFolder, Config::MarvelGameFolder), Config::PaksSubPath);
    if (DirectoryParentExists(fromMarvelRivals)) return fromMarvelRivals;

    // The last two rules only checked that the parent of the result exists, which for Content is
    // the selected folder itself: any folder was accepted (and got a Paks inside). Now the folder
    // has to be named Content / Marvel.
    const std::wstring selectedName = Files::FileName(selectedFolder);

    // They selected Content
    const std::wstring fromContent = Files::DotNetCombine(selectedFolder, Config::PaksFolder);
    if (Text::Equals(selectedName, Config::ContentFolder, true) && DirectoryParentExists(fromContent)) return fromContent;

    // They selected Marvel (inside MarvelGame)
    const std::wstring fromMarvel = Files::DotNetCombine(selectedFolder, Config::ContentPaksSubPath);
    if (Text::Equals(selectedName, Config::MarvelFolder, true) && DirectoryParentExists(fromMarvel)) return fromMarvel;

    return std::nullopt;
}

} // namespace RivalsForge::Services::GamePath

#pragma once
#include <optional>
#include <string>
#include "Core/Zip.h"

// Mods that aren't a .pak / .ucas / .utoc but loose game files replacing the game's own (the
// "skip intro" mods: .bk2 videos for Content\Marvel\MoviesBink\Movies\MarvelLogo). Where they go is
// found by looking for the same files in the game's Content folder; the game's originals are kept
// in %LOCALAPPDATA%\RivalsForge\Originals before they're overwritten, and put back when the mod is
// disabled or deleted.
namespace RivalsForge::Services::Mods::LooseFiles {

// An archive with game files and none of them a .pak / .ucas / .utoc
bool IsLoose(const Zip::Reader& archive);
// What comes with a mod but isn't for the game (readme, screenshots, links): never copied there
bool IsExtra(const std::wstring& name);

// Where the archive's files belong in the game whose Paks folder is `paks`: the folder under
// Content ("Marvel\MoviesBink\Movies\MarvelLogo", "." for Content itself) and the folder inside the
// archive they're under ("Skip Intro/", "" when they're at its root). nullopt when the game has
// none of those files.
struct Placement {
    std::wstring target, strip;
};
std::optional<Placement> Locate(const Zip::Reader& archive, const std::wstring& paks);

// MarvelGame\Marvel\Content (the folder Paks is in)
std::wstring ContentDir(const std::wstring& paks);
// Where a target folder is on disk (`target` as Placement::target)
std::wstring TargetDir(const std::wstring& paks, const std::wstring& target);

// The game's original of `path` (a file under Content), copied aside unless it already is (the
// first mod to replace a file keeps the real original); nothing to do when there's no such file
// yet. False when the copy couldn't be made (the file mustn't be overwritten then).
bool KeepOriginal(const std::wstring& paks, const std::wstring& path);
// The original put back over `path` (and its copy removed); false when none was kept (the file
// was new: the caller deletes it)
bool RestoreOriginal(const std::wstring& paks, const std::wstring& path);

} // namespace RivalsForge::Services::Mods::LooseFiles

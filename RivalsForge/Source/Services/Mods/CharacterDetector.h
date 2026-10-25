#pragma once
#include <optional>
#include <string>

// Which hero a mod is for, read from its ZIP without extracting anything: the game's paths name the
// hero by id (Marvel/Content/Marvel/Characters/1020/..., T_1020001_Body_D), and the small .utoc of
// an IoStore mod lists them. The id used most wins; ids that aren't a known hero don't count.
namespace RivalsForge::Services::Mods::CharacterDetector {

// The hero id ("1020"), or nullopt when the archive names none (or can't be read). Reads the .utoc
// entries; an old-style mod with only a .pak has that scanned instead. Any thread.
std::optional<std::wstring> Detect(const std::wstring& zipPath);

} // namespace RivalsForge::Services::Mods::CharacterDetector

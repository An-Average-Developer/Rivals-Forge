#pragma once
#include <string>

// What kind of mod it is (mesh, texture, audio...), read from its ZIP without extracting anything,
// like CharacterDetector: the .utoc of an IoStore mod lists its folders and files by name, and the
// game's naming says what each one is (SK_ skeletal meshes, T_ textures, MI_ materials, NS_ Niagara
// effects, .bnk / .wem Wwise audio, the UI folder...).
namespace RivalsForge::Services::Mods::ModTypeDetector {

// The kinds, as bits (a mod is often more than one: a skin is a mesh and its textures)
enum Type : unsigned {
    Mesh = 1u << 0,
    Texture = 1u << 1,
    Material = 1u << 2,
    Animation = 1u << 3,
    VFX = 1u << 4,
    Audio = 1u << 5,
    UI = 1u << 6,
    Video = 1u << 7,   // Bink movies (.bk2): the intro videos and the like
};
inline constexpr unsigned kAllTypes[] = { Mesh, Texture, Material, Animation, VFX, Audio, UI, Video };

// The kinds found (0 when none could be told, or the archive can't be read). Reads the .utoc
// entries; an old-style mod with only a .pak has that scanned instead. Any thread.
unsigned Detect(const std::wstring& zipPath);

} // namespace RivalsForge::Services::Mods::ModTypeDetector

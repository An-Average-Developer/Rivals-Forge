#pragma once
#include <optional>
#include <string>
#include <vector>

namespace RivalsForge::Models {

// Models\Mod.cs
struct Mod {
    std::wstring id;                        // Guid.NewGuid().ToString()
    std::wstring name;
    std::vector<std::wstring> files;        // the archive's entries (FullName) extracted to Paks, in order
    bool isEnabled = true;
    // AddedAt, kept exactly as it was read (System.Text.Json's ISO 8601 text with the local offset),
    // so saving doesn't rewrite it; Time::Parse turns it into a date for display
    std::wstring addedAt;
    std::optional<std::wstring> sectionId;  // nullopt = Uncategorized
    std::wstring zipFileName;               // base name of the ZIP in MODS (no extension); "" = use the Id
    // New in the C++ port ("Character" in data.json, after ZipFileName): the hero the mod is for
    // ("1020", Data\Heroes). nullopt = not looked at yet (mods the C# added); "" = none found or
    // chosen. The C# app ignores the field, and drops it if it saves the file.
    std::optional<std::wstring> character;
    // New too: the Nexus Mods mod it was installed from by the store ("NexusModId" / "NexusVersion",
    // written only for those), 0 = not from the store
    long long nexusId = 0;
    std::wstring nexusVersion;
    // The same for GameBanana ("GameBananaModId" / "GameBananaFileId"): its files have no version,
    // so the file installed says which one is here
    long long gameBananaId = 0;
    long long gameBananaFile = 0;
    // New too: a mod of loose game files instead of a .pak ("Target" / "TargetStrip", written only
    // for those): the folder under Content its files replace ("Marvel\MoviesBink\Movies\MarvelLogo";
    // "" = an ordinary mod, its files in Paks), and the folder inside its ZIP they're under
    // ("Skip Intro/"; left out when extracting). Mod::files are then relative to that folder.
    std::wstring target;
    std::wstring targetStrip;

    // A new mod: fresh Id, AddedAt = DateTime.Now
    static Mod New();
};

} // namespace RivalsForge::Models

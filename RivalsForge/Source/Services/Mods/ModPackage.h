#pragma once
#include <string>
#include <vector>
#include "Services/Mods/ModService.h"

// ZIPs that don't come the usual way (the .pak / .ucas / .utoc of one mod at the root): a mod
// inside a folder, several mods in one ZIP (in folders, side by side, or as ZIPs inside the ZIP).
// Their mod files are pulled out to a temporary folder, grouped by mod (the files sharing a name),
// and each mod is written as a ZIP of its own, its files at the root, which the installation
// writes into MODS; once they're installed the original goes to the Recycle Bin. Analyze and Build run on
// a worker thread and throw ModError.
namespace RivalsForge::Services::Mods::ModPackage {

struct Part {
    std::wstring name;                // suggested: its folder, its inner ZIP or its file name
    std::wstring stem;                // the files' shared name ("z_Y2KMantis_9999999_P")
    std::vector<std::wstring> files;  // extracted, in the temporary folder
};

struct Plan {
    std::wstring source;        // the ZIP looked at
    bool split = false;         // false: a normal mod ZIP, installed as it is
    std::wstring tempDir;       // where the parts' files are ("" when not split)
    std::vector<Part> parts;
};

// A .7z / .rar (anything that isn't a ZIP) extracted with Windows' tar.exe and written again as a
// ZIP (stored, its folders kept) in a temporary folder of its own, so it goes the way a ZIP goes
struct Converted {
    std::wstring zip, folder;
};
Converted ToZip(const std::wstring& archive, const Progress& progress = {});
bool IsZip(const std::wstring& path);

// Looks at the ZIP and, when it has to be rebuilt, extracts its mod files
Plan Analyze(const std::wstring& zipPath, const Progress& progress = {});
// The new ZIPs: one per part, its files at the root, named `names[i]`
struct Built {
    std::wstring folder;              // temporary: removed once they're installed
    std::vector<std::wstring> paths;
};
// Builds them (and removes the plan's extracted files)
Built Build(const Plan& plan, const std::vector<std::wstring>& names, const Progress& progress = {});
// The file to the Recycle Bin (the original ZIP, once its mods are installed)
void Recycle(const std::wstring& path);
// The temporary folder removed (a plan that won't be built)
void Discard(const Plan& plan);
// The temporary folders an earlier run left behind (closed while asking the names), at startup
void RemoveLeftovers();

} // namespace RivalsForge::Services::Mods::ModPackage

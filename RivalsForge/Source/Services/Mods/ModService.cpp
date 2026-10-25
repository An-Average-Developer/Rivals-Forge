#include "Services/Mods/ModService.h"
#include <algorithm>
#include "App/Paths.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Game/GamePathService.h"
#include "Services/Mods/CharacterDetector.h"
#include "Services/Mods/ModTypeDetector.h"

namespace RivalsForge::Services::Mods {

ModService::ModService() {
    // The C# let these throw out of the constructor (the app wouldn't start); here they're logged
    try {
        FileOps::MakeDirectory(Paths::DataDir());
        FileOps::MakeDirectory(ModsStorageDir());   // EnsureModsDir
        MigrateFromRoaming();
        Load();

        // Auto-detect if no path has been saved yet
        if (!IsConfigured()) {
            if (auto detected = GamePath::TryDetect()) {
                paksDir = *detected;
                Save();
            }
        }
    } catch (const ModError& e) {
        Log::Error(L"Starting the mod service: " + e.Message());
    }
}

// ── Path management ─────────────────────────────────────────────────────────

void ModService::SetGamePath(const std::wstring& paks) {
    paksDir = paks;
    FileOps::MakeDirectory(paks);
    Save();
}

std::wstring ModService::ModsStorageDir() { return Paths::ModsDir(); }

void ModService::EnsureConfigured() const {
    if (!IsConfigured()) throw ModError(Loc::Get("error_game_not_configured"));
}

// ── Lookups ─────────────────────────────────────────────────────────────────

const Models::Mod* ModService::FindMod(const std::wstring& id) const {
    auto it = std::find_if(mods.begin(), mods.end(), [&](const Models::Mod& m) { return m.id == id; });
    return it == mods.end() ? nullptr : &*it;
}

Models::Mod* ModService::EditMod(const std::wstring& id) { return const_cast<Models::Mod*>(FindMod(id)); }

// ── Heroes ──────────────────────────────────────────────────────────────────

void ModService::SetModCharacter(const std::wstring& modId, const std::wstring& character) {
    if (Models::Mod* mod = EditMod(modId)) mod->character = character;
    Save();
}

std::wstring ModService::DetectCharacter(const Models::Mod& mod) {
    const std::wstring zip = FileOps::FileExists(ZipPath(mod)) ? ZipPath(mod) : ZipOffPath(mod);
    return CharacterDetector::Detect(zip).value_or(L"");
}

unsigned ModService::DetectTypes(const Models::Mod& mod) {
    const std::wstring zip = FileOps::FileExists(ZipPath(mod)) ? ZipPath(mod) : ZipOffPath(mod);
    return ModTypeDetector::Detect(zip);
}

void ModService::ApplyCharacters(const std::vector<std::pair<std::wstring, std::wstring>>& found) {
    bool changed = false;
    for (const auto& [modId, character] : found) {
        Models::Mod* mod = EditMod(modId);
        if (!mod || mod->character) continue;   // set meanwhile (a choice in the window wins)
        mod->character = character;
        changed = true;
    }
    if (changed) Save();
}

} // namespace RivalsForge::Services::Mods

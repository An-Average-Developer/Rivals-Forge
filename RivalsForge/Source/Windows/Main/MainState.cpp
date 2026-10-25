#include "Windows/Main/MainState.h"
#include <algorithm>
#include <utility>
#include "App/Config.h"
#include "Core/Async.h"
#include "Core/Text.h"
#include "Data/Heroes.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/App/UpdateService.h"
#include "Services/App/VersionCompare.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Settings/ConfigFile.h"

namespace RivalsForge::Main {

MainState::MainState(HWND window) : owner(window) {
    statusMessage = Loc::Get("status_ready");
    if (auto saved = Config::ReadText(Config::CollapsedFileName))
        for (const auto& key : Text::Split(*saved, L',', true)) collapsed.insert(key);
    DetectMissingHeroes();
    FindInstallations();
}

std::wstring MainState::HeroOf(const Models::Mod& mod) const {
    return mod.character && Data::FindHero(*mod.character) ? *mod.character : L"";
}

unsigned MainState::TypesOf(const Models::Mod& mod) {
    if (const auto it = modTypes.find(mod.id); it != modTypes.end()) return it->second;
    if (typesPending.insert(mod.id).second)
        Async::Run([mod] { return Services::Mods::ModService::DetectTypes(mod); }, [this, id = mod.id](unsigned types) {
            typesPending.erase(id);
            modTypes[id] = types;
        });
    return 0;
}

std::vector<MainState::Group> MainState::Groups() const {
    // The heroes A to Z (the roster is sorted by name), then the mods with no hero
    std::vector<Group> groups;
    const auto& mods = service.Mods();
    for (const auto& hero : Data::Heroes()) {
        Group group{ hero.id, {} };
        for (const auto& mod : mods)
            if (HeroOf(mod) == hero.id) group.modIds.push_back(mod.id);
        if (!group.modIds.empty()) groups.push_back(std::move(group));
    }
    Group none{ L"", {} };
    for (const auto& mod : mods)
        if (HeroOf(mod).empty()) none.modIds.push_back(mod.id);
    if (!none.modIds.empty()) groups.push_back(std::move(none));
    return groups;
}

std::vector<MainState::Group> MainState::ShownGroups() const {
    std::vector<Group> groups = Groups();
    const bool filterExists = std::any_of(groups.begin(), groups.end(), [&](const Group& g) { return FilterKey(g.hero) == filter; });
    if (filter.empty() || !filterExists) return groups;
    std::erase_if(groups, [&](const Group& g) { return FilterKey(g.hero) != filter; });
    return groups;
}

void MainState::ToggleExpanded(const std::wstring& hero) {
    const std::wstring key = Key(hero);
    if (!collapsed.erase(key)) collapsed.insert(key);
    std::wstring text;
    for (const auto& k : collapsed) text += (text.empty() ? L"" : L",") + k;
    Config::WriteText(Config::CollapsedFileName, text);
}

// "Game not found" / "No mods installed" / "{n} mod(s) · {m} hero(es)"
std::wstring MainState::Summary() const {
    if (!IsConfigured()) return Loc::Get("summary_game_not_found");
    const int total = TotalMods();
    if (total == 0) return Loc::Get("summary_no_mods");
    int heroes = 0;
    for (const auto& group : Groups()) heroes += group.hero.empty() ? 0 : 1;
    return Loc::Fmt("summary_format", Loc::Fmt(total == 1 ? "summary_mods_one" : "summary_mods_many", total),
                    Loc::Fmt(heroes == 1 ? "summary_heroes_one" : "summary_heroes_many", heroes));
}

void MainState::DetectMissingHeroes() {
    std::vector<Models::Mod> pending;
    for (const auto& mod : service.Mods())
        if (!mod.character) pending.push_back(mod);
    if (pending.empty()) return;
    SetStatus(Loc::Get("status_detecting_heroes"));
    using Found = std::vector<std::pair<std::wstring, std::wstring>>;
    Async::Run(
        [pending]() {
            Found found;
            for (const auto& mod : pending) found.emplace_back(mod.id, Services::Mods::ModService::DetectCharacter(mod));
            return found;
        },
        [this](Found found) {
            try {
                service.ApplyCharacters(found);
            } catch (const Services::Mods::ModError& e) {
                Log::Warning(L"Saving the detected heroes: " + e.Message());
            }
            int known = 0;
            for (const auto& [id, hero] : found) known += hero.empty() ? 0 : 1;
            SetStatus(Loc::Fmt("status_heroes_detected", known, static_cast<int>(found.size())));
        });
}

void MainState::FindInstallations() { installations = Services::GamePath::FindInstallations(); }

Services::GamePath::Installation MainState::Current() const { return Services::GamePath::Describe(service.PaksDir(), installations); }

// ── Updates ──────────────────────────────────────────────────────────────────

void MainState::CheckForUpdates(bool silent) {
    if (updates.checking || updates.downloading) return;
    updates.checking = true;
    if (!silent) SetStatus(Loc::Get("status_checking_updates"));
    struct Result {
        Services::Updates::UpdateInfo info;
        bool reachable = false;
    };
    Async::Run(
        [] {
            Result r;
            r.info = Services::Updates::CheckForUpdates(&r.reachable);
            return r;
        },
        [this, silent](Result r) {
            updates.checking = false;
            updates.checked = true;
            updates.reachable = r.reachable;
            const bool found = r.info.isUpdateAvailable && !updates.available;
            updateInfo = r.info;
            updates.available = r.info.isUpdateAvailable;
            updates.latest = r.reachable ? r.info.latestVersion : L"";
            updates.fileName = r.info.fileName;
            updates.fileSize = r.info.fileSize > 0 ? Text::FormatBytes(static_cast<unsigned long long>(r.info.fileSize)) : Loc::Get("updates_unknown_size");
            updates.changelog = r.info.changelog;
            if (!updates.available) updates.installView = false;
            if (!r.reachable) Log::Warning(L"Update check: GitHub could not be reached");
            else Log::Info(L"Update check: latest " + r.info.latestVersion + (updates.available ? L" (newer)" : L""));
            if (updates.available) SetStatus(Loc::Fmt("status_update_available", Versions::WithV(updates.latest)));
            else if (!silent) SetStatus(Loc::Get(r.reachable ? "status_up_to_date" : "status_update_check_failed"));
            if (found && onUpdateFound) onUpdateFound();
        });
}

void MainState::ShowUpdates(bool install) {
    view = View::Updates;
    if (install && updates.available) updates.installView = true;
}

void MainState::InstallUpdate() {
    if (!updates.available || updates.downloading) return;
    if (busy) {
        Dialogs::Message(Loc::Get("updates_wait_install"), Loc::Get("updates_title_plain"), Dialogs::MessageButtons::Ok,
                         Dialogs::MessageIcon::Information);
        return;
    }
    updates.downloading = true;
    updates.progress = 0;
    SetStatus(Loc::Get("status_downloading_update"));
    const Services::Updates::UpdateInfo info = updateInfo;
    Async::Run(
        [this, info] {
            // Only returns when the download failed (otherwise the process exits to be replaced)
            return Services::Updates::DownloadAndInstall(info, [this](int percent) { Async::Post([this, percent] { updates.progress = percent; }); });
        },
        [this](bool) {
            updates.downloading = false;
            updates.progress = 0;
            SetStatus(Loc::Get("status_update_failed"));
            Dialogs::Message(Loc::Get("updates_failed_message"), Loc::Get("updates_failed_title"), Dialogs::MessageButtons::Ok,
                             Dialogs::MessageIcon::Error);
        });
}

} // namespace RivalsForge::Main

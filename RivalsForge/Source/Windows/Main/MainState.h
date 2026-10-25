#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Services/App/UpdateService.h"
#include "Services/Game/GamePathService.h"
#include "Services/Mods/ModService.h"

namespace RivalsForge::Main {

// MainWindow.xaml.cs's state outside the controls: the ModService, the mods grouped by hero (the
// C#'s sections gave way to the heroes: one group per hero that has mods, A to Z, then the mods
// with no hero), the hero filter of the strip on top, and the status bar.
class MainState {
public:
    explicit MainState(HWND owner);

    // One hero's mods ("" = no hero), in the order they were added
    struct Group {
        std::wstring hero;
        std::vector<std::wstring> modIds;
    };

    Services::Mods::ModService& Service() { return service; }

    // The game's installations on this PC (Steam, Epic), found at startup, and the one in use
    std::vector<Services::GamePath::Installation> installations;
    void FindInstallations();
    Services::GamePath::Installation Current() const;

    // Every group (the strip shows them all), and the ones the list shows (the filter applied)
    std::vector<Group> Groups() const;
    std::vector<Group> ShownGroups() const;
    // The mod's hero ("" for none or an unknown id)
    std::wstring HeroOf(const Models::Mod& mod) const;
    // What kinds of mod it is (ModTypeDetector::Type bits): read from its ZIP on a worker the first
    // time it's asked for (0 until then, or when it can't be told)
    unsigned TypesOf(const Models::Mod& mod);

    // What the content shows: the installed mods or the store (the header's tabs), the updates or
    // the changelog (the status bar's version and changelog buttons)
    enum class View { Mods, Store, Updates, Changelog };
    View view = View::Mods;

    // The strip's filter: one hero's group alone ("" = every group; FilterKey("") = the mods with no hero)
    std::wstring filter;
    static std::wstring FilterKey(const std::wstring& hero) { return hero.empty() ? L"-" : hero; }
    // A group folded shut (kept in collapsed.cfg)
    bool Expanded(const std::wstring& hero) const { return !collapsed.count(Key(hero)); }
    void ToggleExpanded(const std::wstring& hero);

    // Bumped when the list should play its entrance again
    unsigned revision = 0;
    void Reload() { ++revision; }

    // ── RefreshUI ──
    bool IsConfigured() const { return service.IsConfigured(); }
    int TotalMods() const { return static_cast<int>(service.Mods().size()); }
    // SummaryLabel.Text: "Game not found" / "No mods installed" / "{n} mod(s) · {heroes} hero(es)"
    std::wstring Summary() const;

    // ── Status bar ──
    std::wstring statusMessage;   // StatusLabel.Text ("Ready" at startup)
    void SetStatus(const std::wstring& message) { statusMessage = message; }
    // A background operation (installing ZIPs) shows its progress (0..1); the actions that change
    // mods wait for it
    bool busy = false;
    float progress = 0.f;

    // ── Updates (the Inazuma Eleven VR Save Editor's MainViewModel) ──
    struct Updates {
        bool checking = false;
        bool checked = false;           // a check finished (GitHub answered or not)
        bool reachable = false;         // GitHub answered the last check
        bool available = false;         // a newer version is published
        std::wstring latest;            // the published version
        std::wstring fileName, fileSize;
        std::wstring changelog;         // the published changelog.txt (the embedded one when offline)
        bool installView = false;       // the Updates page shows the installation
        bool downloading = false;
        int progress = 0;               // 0..100 while downloading
    } updates;
    // Asks GitHub for the latest version; `silent` (at startup) leaves the status bar alone.
    // onUpdateFound runs when a check finds a newer version.
    void CheckForUpdates(bool silent = false);
    std::function<void()> onUpdateFound;
    // Downloads the update and swaps the exe (the manager closes and opens again); not while mods
    // are being installed
    void InstallUpdate();
    void ShowUpdates(bool install = false);

    HWND Owner() const { return owner; }

private:
    // The mods the C# added have no hero yet: found in their ZIPs on a worker thread
    void DetectMissingHeroes();
    static std::wstring Key(const std::wstring& hero) { return hero.empty() ? L"none" : hero; }

    HWND owner;
    Services::Updates::UpdateInfo updateInfo;
    std::map<std::wstring, unsigned> modTypes;   // by mod id, once read
    std::set<std::wstring> typesPending;         // being read
    Services::Mods::ModService service;
    std::set<std::wstring> collapsed;
};

} // namespace RivalsForge::Main

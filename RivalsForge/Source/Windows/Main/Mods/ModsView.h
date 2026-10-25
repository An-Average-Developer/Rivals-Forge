#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Models/Mod.h"
#include "Services/Mods/ModService.h"
#include "UI/Ui.h"
#include "Windows/Main/MainState.h"

namespace RivalsForge::Main {

class MainWindow;
struct AddJob;

// Where the archives being added come from: the user's own files (the originals go to the Recycle
// Bin once installed), or a download from the store (a temporary file, deleted afterwards)
struct AddOptions {
    bool downloaded = false;
    std::wstring folder;        // downloaded: the temporary folder it's in (removed at the end)
    std::wstring name;          // the suggested name ("" = the file's)
    long long nexusId = 0;      // the Nexus Mods mod it is
    std::wstring nexusVersion;
    long long gameBananaId = 0;     // or the GameBanana mod, and its file
    long long gameBananaFile = 0;
};

// MainWindow.xaml's content row and the handlers of MainWindow.xaml.cs behind it: the setup state
// (game not found), the empty state and the list, now by hero: the strip of hero avatars on top
// (a filter), then one card per hero with its portrait and its mods (the C#'s section and mod
// DataTemplates, redesigned). Split:
//   ModsView.cpp       which state shows, cross-fading between them
//   SetupPanel.cpp     "Marvel Rivals Not Found": Browse / Retry and the hint
//   EmptyState.cpp     "No mods installed"
//   HeroStrip.cpp      the avatars of the heroes with mods ("All" first): a click shows that hero alone
//   HeroList.cpp       the scroll area and the cards coming and going (appear / leave animations)
//   HeroCard.cpp       a hero: portrait, name, role, counts, fold; its mods
//   ModCard.cpp        a mod: status, name, files · date · status, Enable / Rename / Hero / Delete
//   ModsAdd.cpp        adding ZIPs: looked at, named, split when one holds several mods, installed
//                      (in the background), the originals to the Recycle Bin
//   ModsActions.cpp    toggle, rename, hero, delete, the folders
//   GameActions.cpp    start the game, switch between its installations (Steam / Epic), copying the mods or not
class ModsView {
public:
    explicit ModsView(MainWindow& window);
    void Draw(UI::Ui& ui, const Rect& content);

    // ── ModsActions.cpp ──
    // AddMod_Click: the ZIPs chosen are added (ModsAdd.cpp); each mod goes to the hero found in it
    void AddMod();
    // The rest of DoAddMod once the ZIPs are chosen: the name of every mod is asked, then they're installed
    void AddFiles(std::vector<std::wstring> files, const AddOptions& options = {});
    void ToggleMod(const std::wstring& modId);
    void RenameMod(const std::wstring& modId);
    // The hero picker for the mod (the C#'s "Move ▾")
    void ChangeHero(const std::wstring& modId);
    void DeleteMod(const std::wstring& modId);
    // GameActions.cpp: start the game (the installation in use), move to another installation
    void PlayGame();
    void SwitchInstallation(const Services::GamePath::Installation& target);
    // Runs `action` once the game is closed: straight away when it isn't running; otherwise a card
    // says mods can't change while it's open and offers to close it (`cancelled` if not)
    void WhenGameClosed(std::function<void()> action, std::function<void()> cancelled = {});
    void OpenModsFolder();
    void OpenPaksFolder();
    // The game's intro videos (Content\Marvel\MoviesBink\Movies\MarvelLogo)
    void OpenIntrosFolder();
    // A background installation is running: the actions that change mods wait
    bool Busy() const;

private:
    // A card on screen: alive while the state has it, then leaving (shrinking away) for a moment
    struct ModRow {
        std::wstring id;
        Models::Mod mod;           // what's drawn (kept while it leaves)
        double shownAt = 0.0;
        double hiddenAt = -1.0;
    };
    struct HeroRow {
        std::wstring hero;         // "" = no hero
        std::vector<ModRow> mods;
        double shownAt = 0.0;
        double hiddenAt = -1.0;
    };

    // SetupPanel.cpp
    void SetupPanel(UI::Ui& ui, const Rect& r);
    void BrowseGamePath();
    void RetryDetection();
    // EmptyState.cpp
    void EmptyState(UI::Ui& ui, const Rect& r);
    // HeroStrip.cpp: draws the strip, returns its height
    float HeroStrip(UI::Ui& ui, const Rect& r);
    // HeroList.cpp
    void HeroList(UI::Ui& ui, const Rect& r);
    void SyncRows(double now);
    // HeroCard.cpp. `open` is the expanded state's animation (0 folded .. 1 open); a leaving card
    // isn't `interactive`.
    float HeroCardHeight(UI::Ui& ui, const HeroRow& row, float open, double now);
    void HeroCard(UI::Ui& ui, const Rect& r, const HeroRow& row, float open, bool interactive, double now);
    // ModCard.cpp
    static float ModCardHeight(UI::Ui& ui);
    void ModCard(UI::Ui& ui, const Rect& r, const ModRow& row, bool interactive);
    // A hero / mod id as a widget id key
    static int64_t Key(const std::wstring& id);
    // How far a card has come in (0..1) and gone (0..1) at `now`
    static float Appear(double shownAt, double now);
    static float Leave(double hiddenAt, double now);
    // ModsActions.cpp: ToggleMod once the game is closed
    void DoToggle(const std::wstring& modId);
    // GameActions.cpp: ends the game's processes (on a worker), then `then`
    void CloseGame(std::function<void()> then);
    // ModsAdd.cpp: the steps of adding ZIPs (see there), once the game is closed
    void StartAdd(const std::vector<std::wstring>& files, const AddOptions& options);
    void AnalyzeNext(std::shared_ptr<AddJob> job);
    void AskName(std::shared_ptr<AddJob> job);
    void BuildNext(std::shared_ptr<AddJob> job);
    void InstallNext(std::shared_ptr<AddJob> job);
    void FinishAdd(std::shared_ptr<AddJob> job);
    // ModsActions.cpp: an error card ("<key text>\n<message>"); a worker's progress handed to the
    // status bar as `from + span * k`
    static void ShowError(const char* key, const std::wstring& message);
    Services::Mods::Progress ProgressTo(float from, float span);
    // After a change of the mods: the status bar gets `status`
    void Done(const std::wstring& status);

    MainWindow& main;
    MainState& state;
    std::wstring setupHint;          // SetupHintLabel.Text
    std::vector<HeroRow> rows;
    unsigned seenRevision = ~0u;
};

} // namespace RivalsForge::Main

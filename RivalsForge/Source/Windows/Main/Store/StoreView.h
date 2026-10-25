#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Graphics/Images.h"
#include "Models/Mod.h"
#include "Services/Store/Nexus.h"
#include "UI/Ui.h"

namespace RivalsForge::Main {

class MainWindow;
class MainState;

// The store (new): Marvel Rivals' mods on Nexus Mods. A toolbar (search, the order, a hero filter),
// the mods as cards in a grid (picture, name, author, endorsements / downloads, "Installed" /
// "Update" for the ones already here) loading more as asked, and a mod's card with its description
// and files. Installing a file downloads it and hands it to the Mod Manager's own way of adding
// mods (ModsView::AddFiles): the same name card, the same splitting, the same installation.
// Downloading needs the user's Nexus API key (StoreAccount): a Premium account downloads straight
// away; a free one through the site's "Mod Manager Download" button and the nxm:// link it opens
// (HandleNxm). Split:
//   StoreView.cpp     state, requests, pictures, downloads, nxm:// links
//   StoreToolbar.cpp  the search, the order, the hero filter
//   StoreGrid.cpp     the grid, its cards, "Load more", the empty / loading / error states
class StoreView {
public:
    using StoreMod = Services::Store::StoreMod;
    using ModFile = Services::Store::ModFile;

    explicit StoreView(MainWindow& window);
    void Draw(UI::Ui& ui, const Rect& content);

    // Install a file of a mod (the mod's card): straight away with Premium, through the site otherwise
    void Install(const StoreMod& mod, const ModFile& file);
    // An nxm:// link from the site (a second launch of the app hands it here)
    void HandleNxm(const std::wstring& url);
    // The picture at `url`, decoded on a worker; nullptr until it's here
    ImagePtr Picture(const std::wstring& url, unsigned maxSide);
    // The installed mod from Nexus mod `id`, or nullptr
    const Models::Mod* Installed(long long id) const;

    // ── The account (StoreAccount) ──
    struct Account {
        std::wstring key;
        std::optional<Services::Store::Account> info;   // once the key was checked
        bool checking = false;
        std::wstring error;
    };
    const Account& GetAccount() const { return account; }
    // Checks a key and keeps it when it's good ("" signs out)
    void SetKey(const std::wstring& key);
    bool Busy() const;
    // 9830 -> "9,830" (the language's separator); short: 1234 -> "1.2K", 2500000 -> "2.5M"
    static std::wstring Number(long long n, bool shorten);

private:
    // Requests
    void Load(bool more);
    void CheckKey();
    void Download(const StoreMod& mod, const ModFile& file, const std::wstring& nxmKey, const std::wstring& expires);

    // Drawing
    float Toolbar(UI::Ui& ui, const Rect& r);
    bool Chip(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool on);
    float ChipWidth(UI::Ui& ui, const std::wstring& text) const;
    void HeroFilter(UI::Ui& ui, const Rect& r);
    void Grid(UI::Ui& ui, const Rect& r);
    void Card(UI::Ui& ui, const Rect& r, size_t index);
    void States(UI::Ui& ui, const Rect& r);

    MainWindow& main;
    MainState& state;

    // The list
    std::vector<StoreMod> mods;
    long long total = 0;
    int pagesLoaded = 0;
    Services::Store::Sort sort = Services::Store::Sort::Popular;
    std::wstring search, appliedSearch, hero;   // hero: an id of Data\Heroes ("" = all)
    double searchChangedAt = -10.0;
    bool searchPending = false;
    bool loading = false, loadedOnce = false;
    std::wstring error;
    unsigned generation = 0;   // a response for an older request is dropped
    double listShownAt = -10.0;

    // The download (one at a time)
    long long downloadingId = 0;

    // Pictures
    struct PictureSlot {
        ImagePtr image;
        bool loading = false, failed = false;
    };
    std::map<std::wstring, PictureSlot> pictures;

    Account account;
};

} // namespace RivalsForge::Main

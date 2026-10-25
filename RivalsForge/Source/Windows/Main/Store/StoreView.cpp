#include "Windows/Main/Store/StoreView.h"
#include "Services/App/Privacy.h"
#include <algorithm>
#include <memory>
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Data/Heroes.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Store/NexusAccount.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsView.h"
#include "Windows/Main/Store/StoreAccountOverlay.h"

namespace RivalsForge::Main {

namespace {

namespace Store = Services::Store;
namespace NexusAccount = Services::Store::NexusAccount;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;

constexpr int kPerPage = 24;
constexpr double kSearchDelay = 0.5;   // seconds of quiet typing before the search runs

// The file name at the end of a download address ("…/Y2K%20Mantis-1233.zip?md5=…" -> "Y2K Mantis-1233.zip")
std::wstring FileNameOf(const std::wstring& url) {
    std::wstring path = url.substr(0, url.find(L'?'));
    std::wstring name = path.substr(path.find_last_of(L'/') + 1);
    std::string bytes;
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] == L'%' && i + 2 < name.size()) {
            bytes += static_cast<char>(std::wcstol(name.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            bytes += Text::Narrow(std::wstring(1, name[i]));
        }
    }
    std::wstring decoded = Text::Widen(bytes);
    for (auto& c : decoded)
        if (c < 32 || wcschr(L"\"<>|:*?\\/", c)) c = L'_';
    return decoded;
}

} // namespace

StoreView::StoreView(MainWindow& window) : main(window), state(window.State()) {
    account.key = NexusAccount::LoadKey();
    if (!account.key.empty()) CheckKey();
}

std::wstring StoreView::Number(long long n, bool shorten) {
    const std::wstring separator = Loc::Get("number_group");
    if (shorten && n >= 1000) {
        const double value = n >= 1000000 ? n / 1000000.0 : n / 1000.0;
        std::wstring text = std::to_wstring(static_cast<long long>(value * 10.0 + 0.5));
        if (value < 100.0) text.insert(text.size() - 1, Loc::Get("number_decimal"));
        else text.pop_back();
        if (text.size() > 2 && text[text.size() - 1] == L'0' && text.find(Loc::Get("number_decimal")) == text.size() - 2) text.resize(text.size() - 2);
        return text + (n >= 1000000 ? L"M" : L"K");
    }
    std::wstring digits = std::to_wstring(n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += separator;
        out += digits[i];
    }
    return out;
}

bool StoreView::Busy() const { return state.busy || downloadingId != 0; }

const Models::Mod* StoreView::Installed(long long id) const {
    for (const auto& mod : state.Service().Mods())
        if (mod.nexusId == id) return &mod;
    return nullptr;
}

// ── The list ────────────────────────────────────────────────────────────────

void StoreView::Load(bool more) {
    const unsigned request = ++generation;
    loading = true;
    error.clear();
    const int page = more ? pagesLoaded : 0;
    const Data::Hero* filterHero = Data::FindHero(hero);
    struct Result {
        Store::Page page;
        std::optional<std::wstring> error;
    };
    Async::Run(
        [page, sort = sort, search = appliedSearch, heroName = filterHero ? filterHero->name : std::wstring()]() -> Result {
            try {
                return { Store::FetchPage(page, kPerPage, sort, search, heroName), std::nullopt };
            } catch (const Store::Error& e) {
                return { {}, e.Message() };
            }
        },
        [this, request, more, page](Result result) {
            if (request != generation) return;   // the filters changed meanwhile
            loading = false;
            loadedOnce = true;
            if (result.error) {
                error = *result.error;
                Log::Warning(L"Store: " + error);
                return;
            }
            if (!more) {
                mods.clear();
                listShownAt = -1.0;   // the cards come in again
            }
            for (auto& mod : result.page.mods) mods.push_back(std::move(mod));
            total = result.page.total;
            pagesLoaded = page + 1;
        });
}

ImagePtr StoreView::Picture(const std::wstring& url, unsigned maxSide) {
    if (url.empty()) return nullptr;
    PictureSlot& slot = pictures[url];
    if (slot.image || slot.loading || slot.failed) return slot.image;
    slot.loading = true;
    struct Decoded {
        unsigned w = 0, h = 0;
        std::vector<uint32_t> pixels;
    };
    Async::Run(
        [url, maxSide]() -> Decoded {
            Decoded d;
            try {
                const std::string bytes = Store::FetchImage(url);
                if (!Images::DecodeMemory(bytes, maxSide, d.w, d.h, d.pixels)) d.pixels.clear();
            } catch (const Store::Error&) {
            }
            return d;
        },
        [this, url](Decoded d) {
            PictureSlot& slot = pictures[url];
            slot.loading = false;
            if (d.pixels.empty()) slot.failed = true;
            else slot.image = Images::FromPixels(d.w, d.h, d.pixels.data());
        });
    return nullptr;
}

// ── The account ─────────────────────────────────────────────────────────────

void StoreView::CheckKey() {
    account.checking = true;
    account.error.clear();
    struct Result {
        std::optional<Store::Account> info;
        std::wstring error;
    };
    Async::Run(
        [key = account.key]() -> Result {
            try {
                return { Store::Validate(key), L"" };
            } catch (const Store::Error& e) {
                return { std::nullopt, e.Message() };
            }
        },
        [this, key = account.key](Result result) {
            if (key != account.key) return;   // another key was given meanwhile
            account.checking = false;
            account.info = result.info;
            account.error = result.error;
            if (result.info) {
                Services::Privacy::Remember(result.info->name);
                NexusAccount::SaveKey(key);
                Log::Info(L"Nexus: signed in as " + result.info->name + (result.info->premium ? L" (Premium)" : L""));
            }
        });
}

void StoreView::SetKey(const std::wstring& key) {
    const std::wstring trimmed = Text::TrimDotNet(key);
    if (trimmed.empty()) {
        NexusAccount::SaveKey(L"");
        account = {};
        state.SetStatus(Loc::Get("store_status_signed_out"));
        return;
    }
    account.key = trimmed;
    account.info.reset();
    CheckKey();
}

// ── Installing ──────────────────────────────────────────────────────────────

void StoreView::Install(const StoreMod& mod, const ModFile& file) {
    if (Busy()) return;
    if (account.key.empty() || (!account.info && !account.checking)) {
        // Downloading needs the account: its card says why and how
        main.StoreAccount().Show(true);
        return;
    }
    if (account.checking) {
        state.SetStatus(Loc::Get("store_status_checking"));
        return;
    }
    if (account.info->premium) {
        Download(mod, file, L"", L"");
        return;
    }
    // A free account: Nexus hands out the download on its own site ("Mod Manager Download"), as an
    // nxm:// link this app has to be the one opening
    const std::wstring page = mod.PageUrl() + L"?tab=files&file_id=" + std::to_wstring(file.id);
    if (NexusAccount::HandlerIsOurs()) {
        Process::OpenUrl(page);
        state.SetStatus(Loc::Fmt("store_status_waiting_site", mod.name));
        return;
    }
    Dialogs::Message(Loc::Get("store_free_message"), Loc::Get("store_free_title"), MessageButtons::YesNo, MessageIcon::Information,
                     [this, page, name = mod.name](MessageResult answer) {
                         if (answer == MessageResult::Yes && !NexusAccount::RegisterHandler()) {
                             Dialogs::Message(Loc::Get("store_nxm_failed"), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
                             return;
                         }
                         Process::OpenUrl(page);
                         state.SetStatus(Loc::Fmt(answer == MessageResult::Yes ? "store_status_waiting_site" : "store_status_manual", name));
                     });
}

void StoreView::HandleNxm(const std::wstring& url) {
    const auto link = NexusAccount::ParseNxm(url);
    if (!link) {
        Log::Warning(L"Store: not an nxm link: " + url);
        return;
    }
    Log::Info(L"Store: nxm link for mod " + std::to_wstring(link->modId) + L", file " + std::to_wstring(link->fileId));
    if (!Text::Equals(link->game, Store::kGameDomain, true)) {
        Dialogs::Message(Loc::Fmt("store_nxm_other_game", link->game), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Warning);
        return;
    }
    if (account.key.empty()) {
        main.StoreAccount().Show(true);
        return;
    }
    if (Busy()) {
        Dialogs::Message(Loc::Get("store_busy"), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Warning);
        return;
    }
    main.ShowStore();
    // The mod's name and the file's version come from its details
    downloadingId = link->modId;
    state.SetStatus(Loc::Get("store_status_preparing"));
    struct Result {
        Store::ModDetails details;
        std::optional<std::wstring> error;
    };
    Async::Run(
        [modId = link->modId]() -> Result {
            try {
                return { Store::FetchDetails(modId), std::nullopt };
            } catch (const Store::Error& e) {
                return { {}, e.Message() };
            }
        },
        [this, link = *link](Result result) {
            downloadingId = 0;
            StoreMod mod = result.details.mod;
            if (result.error || mod.id == 0) {
                mod.id = link.modId;
                mod.name = L"Nexus " + std::to_wstring(link.modId);
            }
            ModFile file;
            file.id = link.fileId;
            for (const auto& f : result.details.files)
                if (f.id == link.fileId) file = f;
            Download(mod, file, link.key, link.expires);
        });
}

void StoreView::Download(const StoreMod& mod, const ModFile& file, const std::wstring& nxmKey, const std::wstring& expires) {
    if (Busy()) return;
    downloadingId = mod.id;
    state.busy = true;
    state.progress = 0.f;
    state.SetStatus(Loc::Fmt("store_status_downloading", mod.name));
    struct Result {
        std::wstring path, folder;
        std::optional<std::wstring> error;
    };
    Async::Run(
        [key = account.key, mod, file, nxmKey, expires, &stateRef = state]() -> Result {
            Result result;
            result.folder = Files::Combine(Files::TempDirectory(), L"RF-" + System::NewGuid());
            try {
                const std::wstring link = Store::DownloadLink(key, mod.id, file.id, nxmKey, expires);
                std::wstring name = FileNameOf(link);
                if (name.empty()) name = file.fileName.empty() ? L"mod.zip" : file.fileName;
                result.path = Files::Combine(result.folder, name);
                Files::CreateDirectories(result.folder);
                auto last = std::make_shared<float>(-1.f);
                Store::Download(link, result.path, [&stateRef, last](uint64_t received, uint64_t total) {
                    if (total == 0) return;
                    const float k = static_cast<float>(static_cast<double>(received) / static_cast<double>(total));
                    if (k - *last < 0.005f && k < 1.f) return;
                    *last = k;
                    Async::Post([&stateRef, k] { stateRef.progress = k; });
                });
            } catch (const Store::Error& e) {
                Files::DeleteDirectory(result.folder);
                result.error = e.Message();
            }
            return result;
        },
        [this, mod, file](Result result) {
            downloadingId = 0;
            state.busy = false;
            state.progress = 0.f;
            if (result.error) {
                Log::Warning(L"Store: downloading " + mod.name + L": " + *result.error);
                state.SetStatus(Loc::Get("status_ready"));
                Dialogs::Message(Loc::Fmt("store_download_failed", *result.error), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
                return;
            }
            Log::Info(L"Store: downloaded " + Files::FileName(result.path));
            // From here, exactly as a mod the user adds: the name card, the splitting, the installation
            AddOptions options;
            options.downloaded = true;
            options.folder = result.folder;
            options.name = mod.name;
            options.nexusId = mod.id;
            options.nexusVersion = file.version.empty() ? mod.version : file.version;
            main.Mods().AddFiles({ result.path }, options);
        });
}

// ── Drawing ─────────────────────────────────────────────────────────────────

void StoreView::Draw(UI::Ui& ui, const Rect& content) {
    if (!loadedOnce && !loading) Load(false);
    if (searchPending && ui.Time() - searchChangedAt > kSearchDelay) {
        searchPending = false;
        if (Text::TrimDotNet(search) != appliedSearch) {
            appliedSearch = Text::TrimDotNet(search);
            Load(false);
        }
    }
    if (searchPending || loading || downloadingId) ui.KeepAnimating();
    if (listShownAt < 0.0) listShownAt = ui.Time();

    const float toolbarH = Toolbar(ui, content);
    Grid(ui, { content.x, content.y + toolbarH, content.w, content.h - toolbarH });
}

} // namespace RivalsForge::Main

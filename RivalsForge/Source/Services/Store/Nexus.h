#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Marvel Rivals' mods on Nexus Mods (game "marvelrivals", id 7106). The list, the search and a mod's
// details come from the public GraphQL API (api.nexusmods.com/v2/graphql), no account needed; a
// download link needs the user's personal API key (v1 REST API): a Premium account gets one straight
// away, a free one only with the key / expiry of an nxm:// link from the site's "Mod Manager
// Download" button. Blocking: call from worker threads. Failed requests throw Store::Error.
namespace RivalsForge::Services::Store {

class Error : public std::exception {
public:
    explicit Error(std::wstring text) : message(std::move(text)) {}
    const std::wstring& Message() const { return message; }
    const char* what() const noexcept override { return "Store::Error"; }

private:
    std::wstring message;
};

inline constexpr const wchar_t* kGameDomain = L"marvelrivals";
inline constexpr long long kGameId = 7106;

struct StoreMod {
    long long id = 0;
    std::wstring name, summary, author, version, category;
    std::wstring pictureUrl, thumbnailUrl;
    long long endorsements = 0, downloads = 0;
    std::wstring createdAt, updatedAt;   // ISO 8601 ("2025-01-14T20:36:10Z")
    bool adult = false;
    std::wstring PageUrl() const;        // https://www.nexusmods.com/marvelrivals/mods/<id>
};

enum class Sort { Popular, Downloads, Newest, Updated };

struct Page {
    std::vector<StoreMod> mods;
    long long total = 0;
};

// One page (0-based, `perPage` mods), adult mods left out. `search` and `hero` (a hero's name)
// narrow it by the mod's name.
Page FetchPage(int page, int perPage, Sort sort, const std::wstring& search, const std::wstring& hero);

struct ModFile {
    long long id = 0;
    std::wstring name, version, category;   // category: MAIN, OPTIONAL, MISCELLANEOUS, UPDATE...
    std::wstring fileName;                  // "Y2K Mantis-1233-1-2-1746475442.zip"
    std::wstring description;
    long long size = 0;
    long long date = 0;                     // Unix seconds
};

struct ModDetails {
    StoreMod mod;
    std::wstring description;               // from its BBCode to plain text
    std::vector<ModFile> files;             // the current ones (old versions and archived left out), main first
};
ModDetails FetchDetails(long long modId);

// The picture's bytes, from %LOCALAPPDATA%\RivalsForge\StoreCache when it's there
std::string FetchImage(const std::wstring& url);

// ── With the user's API key (v1) ──
struct Account {
    std::wstring name;
    long long userId = 0;
    bool premium = false;
};
// The account the key belongs to; throws when the key is wrong
Account Validate(const std::wstring& apiKey);

// The CDN address of a file. A free account needs `nxmKey` / `expires` from an nxm:// link.
std::wstring DownloadLink(const std::wstring& apiKey, long long modId, long long fileId, const std::wstring& nxmKey = L"",
                          const std::wstring& expires = L"");

// Downloads `url` into `path`; `progress(received, total)` (total 0 when unknown)
void Download(const std::wstring& url, const std::wstring& path, const std::function<void(uint64_t, uint64_t)>& progress);

} // namespace RivalsForge::Services::Store

#pragma once
#include <optional>
#include <string>
#include "Services/Store/Store.h"

// Marvel Rivals' mods on Nexus Mods (game "marvelrivals", id 7106). The list, the search and a mod's
// details come from the public GraphQL API (api.nexusmods.com/v2/graphql), no account needed; a
// download link needs the user's personal API key (v1 REST API): a Premium account gets one straight
// away, a free one only with the key / expiry of an nxm:// link from the site's "Mod Manager
// Download" button. Blocking: call from worker threads. Failed requests throw Store::Error.
namespace RivalsForge::Services::Store {

inline constexpr const wchar_t* kGameDomain = L"marvelrivals";
inline constexpr long long kGameId = 7106;

// One page (0-based, `perPage` mods), adult mods left out. `search` and `hero` (a hero's name)
// narrow it by the mod's name.
Page FetchPage(int page, int perPage, Sort sort, const std::wstring& search, const std::wstring& hero);

ModDetails FetchDetails(long long modId);

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

} // namespace RivalsForge::Services::Store

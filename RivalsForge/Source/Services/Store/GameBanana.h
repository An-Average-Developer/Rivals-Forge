#pragma once
#include <string>
#include "Services/Store/Store.h"

// Marvel Rivals' mods on GameBanana (game 21533), from its public API (gamebanana.com/apiv11): the
// list, the search, a mod's details and its files, whose addresses download straight away, with
// no account. Blocking: call from worker threads. Failed requests throw Store::Error.
namespace RivalsForge::Services::Store::GameBanana {

inline constexpr long long kGameId = 21533;

// One page (0-based), adult mods left out (Page::skipped). With no search or hero, `perPage` mods
// in `sort`'s order; a search (and a hero, by its name) gives GameBanana's own 15 a page.
Page FetchPage(int page, int perPage, Sort sort, const std::wstring& search, const std::wstring& hero);

ModDetails FetchDetails(long long modId);

} // namespace RivalsForge::Services::Store::GameBanana

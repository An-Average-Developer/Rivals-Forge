#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// What every store of the Mod Store shares: the mods as the grid and the mod's card show them,
// their files, the order, and the picture / download helpers. Each store (Nexus.h, GameBanana.h)
// fills these from its own API. Blocking: call from worker threads. Failed requests throw Error.
namespace RivalsForge::Services::Store {

class Error : public std::exception {
public:
    explicit Error(std::wstring text) : message(std::move(text)) {}
    const std::wstring& Message() const { return message; }
    const char* what() const noexcept override { return "Store::Error"; }

private:
    std::wstring message;
};

// The stores the Mod Store browses
enum class Source { Nexus, GameBanana };
// "Nexus Mods" / "GameBanana"
std::wstring SourceName(Source source);

struct StoreMod {
    Source source = Source::Nexus;
    long long id = 0;
    std::wstring name, summary, author, version, category;
    std::wstring pictureUrl, thumbnailUrl;
    long long endorsements = 0, downloads = 0;   // GameBanana: its likes; its downloads only in the details
    long long views = 0;                         // GameBanana only
    std::wstring createdAt, updatedAt;           // ISO 8601 ("2025-01-14T20:36:10Z")
    bool adult = false;
    std::wstring PageUrl() const;                // the mod's page on its store
};

enum class Sort { Popular, Downloads, Newest, Updated };

struct Page {
    std::vector<StoreMod> mods;
    long long total = 0;
    long long skipped = 0;   // left out of `mods` though `total` counts them (adult mods)
};

struct ModFile {
    long long id = 0;
    std::wstring name, version, category;   // category: MAIN, OPTIONAL, MISCELLANEOUS, UPDATE...
    std::wstring fileName;                  // "Y2K Mantis-1233-1-2-1746475442.zip"
    std::wstring description;
    long long size = 0;
    long long date = 0;                     // Unix seconds
    std::wstring downloadUrl;               // GameBanana: straight to the file, no account needed
};

struct ModDetails {
    StoreMod mod;
    std::wstring description;               // from its BBCode / HTML to plain text
    std::vector<ModFile> files;             // the current ones (old versions and archived left out), main first
};

// BBCode and HTML to plain text: tags out, links kept as their text, list items as "•", at most
// one empty line in a row
std::wstring PlainText(std::wstring text);

// The picture's bytes, from %LOCALAPPDATA%\RivalsForge\StoreCache when it's there
std::string FetchImage(const std::wstring& url);

// Downloads `url` into `path`; `progress(received, total)` (total 0 when unknown)
void Download(const std::wstring& url, const std::wstring& path, const std::function<void(uint64_t, uint64_t)>& progress);

} // namespace RivalsForge::Services::Store

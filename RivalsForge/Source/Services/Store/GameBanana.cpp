#include "Services/Store/GameBanana.h"
#include <algorithm>
#include <ctime>
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Version.h"

namespace RivalsForge::Services::Store::GameBanana {

namespace {

constexpr const wchar_t* kApi = L"https://gamebanana.com/apiv11";

std::vector<std::pair<std::wstring, std::wstring>> Headers() {
    return { { L"User-Agent", Text::Widen(APP_PRODUCT) + L"/" + Text::Widen(APP_VERSION_STRING) }, { L"Accept", L"application/json" } };
}

// GameBanana answers errors as { "_sErrorCode": ..., "_sMessage": ... }
Json::Value Get(const std::wstring& path) {
    const Http::Response response = Http::Get(std::wstring(kApi) + path, Headers(), 20000);
    if (response.status == 0) throw Error(Loc::Get("store_gb_error_network"));
    Json::Value json = Json::Parse(response.body);
    if (!response.Ok() || json.Has("_sErrorCode")) {
        Log::Warning(L"GameBanana: " + Text::Widen(response.body.substr(0, 400)));
        const std::wstring message = json["_sMessage"].WStr();
        throw Error(message.empty() ? Loc::Fmt("store_gb_error_http", response.status) : L"GameBanana: " + message);
    }
    return json;
}

// Unix seconds as ISO 8601, like Nexus gives its dates
std::wstring Iso(long long unix) {
    if (unix <= 0) return L"";
    const std::time_t t = static_cast<std::time_t>(unix);
    std::tm utc{};
    if (gmtime_s(&utc, &t) != 0) return L"";
    wchar_t iso[32];
    swprintf_s(iso, L"%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
    return iso;
}

// A mod as the list, the search and the profile page all describe it
StoreMod ReadMod(const Json::Value& v) {
    StoreMod m;
    m.source = Source::GameBanana;
    m.id = v["_idRow"].Int();
    m.name = Text::TrimDotNet(v["_sName"].WStr());
    m.author = v["_aSubmitter"]["_sName"].WStr();
    m.version = v["_sVersion"].WStr();
    // The hero (its sub-category: "Psylocke"), else what kind of mod ("Characters", "Sounds")
    m.category = v["_aSubCategory"]["_sName"].WStr();
    if (m.category.empty()) m.category = v["_aCategory"]["_sName"].WStr();
    if (m.category.empty()) m.category = v["_aRootCategory"]["_sName"].WStr();
    for (const auto& image : v["_aPreviewMedia"]["_aImages"].Elements()) {
        if (image["_sType"].Str() != "screenshot") continue;
        const std::wstring base = image["_sBaseUrl"].WStr() + L"/";
        m.pictureUrl = base + image["_sFile"].WStr();
        const std::wstring small = image["_sFile530"].WStr();
        m.thumbnailUrl = small.empty() ? m.pictureUrl : base + small;
        break;
    }
    m.endorsements = v["_nLikeCount"].Int();
    m.downloads = v["_nDownloadCount"].Int();
    m.views = v["_nViewCount"].Int();
    m.createdAt = Iso(v["_tsDateAdded"].Int());
    m.updatedAt = Iso(v.Has("_tsDateUpdated") ? v["_tsDateUpdated"].Int() : v["_tsDateModified"].Int());
    // Mods GameBanana hides until the viewer agrees to see them: their content ratings say why
    m.adult = v["_sInitialVisibility"].Str() == "hide";
    return m;
}

} // namespace

Page FetchPage(int page, int perPage, Sort sort, const std::wstring& search, const std::wstring& hero) {
    std::wstring words = Text::TrimDotNet(search);
    if (!hero.empty()) words = words.empty() ? hero : words + L" " + hero;
    std::wstring path;
    if (!words.empty()) {
        // The search has two orders that fit: the most liked, the newest
        const wchar_t* order = sort == Sort::Newest || sort == Sort::Updated ? L"date" : L"popularity";
        path = L"/Util/Search/Results?_sModelName=Mod&_idGameRow=" + std::to_wstring(kGameId) + L"&_sSearchString=" + Text::UrlEncode(words) +
               L"&_sOrder=" + order + L"&_nPage=" + std::to_wstring(page + 1);
    } else {
        const wchar_t* order = sort == Sort::Downloads ? L"Generic_MostDownloaded"
                               : sort == Sort::Newest  ? L"Generic_Newest"
                               : sort == Sort::Updated ? L"Generic_LatestUpdated"
                                                       : L"Generic_MostLiked";
        path = L"/Mod/Index?_aFilters%5BGeneric_Game%5D=" + std::to_wstring(kGameId) + L"&_sSort=" + order + L"&_nPerpage=" +
               std::to_wstring(std::clamp(perPage, 1, 50)) + L"&_nPage=" + std::to_wstring(page + 1);
    }
    const Json::Value json = Get(path);
    Page result;
    result.total = json["_aMetadata"]["_nRecordCount"].Int();
    for (const auto& record : json["_aRecords"].Elements()) {
        if (record["_sModelName"].Str() != "Mod") continue;
        StoreMod mod = ReadMod(record);
        // Adult mods stay out of the store, as on Nexus
        if (mod.adult) ++result.skipped;
        else result.mods.push_back(std::move(mod));
    }
    return result;
}

ModDetails FetchDetails(long long modId) {
    const Json::Value json = Get(L"/Mod/" + std::to_wstring(modId) + L"/ProfilePage");
    ModDetails details;
    details.mod = ReadMod(json);
    details.description = PlainText(json["_sText"].WStr());
    details.mod.summary = Text::TrimDotNet(json["_sDescription"].WStr());
    for (const auto& f : json["_aFiles"].Elements()) {
        if (f["_bIsArchived"].Bool()) continue;
        ModFile file;
        file.id = f["_idRow"].Int();
        file.fileName = f["_sFile"].WStr();
        file.name = file.fileName;
        file.version = f["_sVersion"].WStr();
        file.category = L"MAIN";   // GameBanana doesn't tell main files from optional ones
        file.description = PlainText(f["_sDescription"].WStr());
        file.size = f["_nFilesize"].Int();
        file.date = f["_tsDateAdded"].Int();
        file.downloadUrl = f["_sDownloadUrl"].WStr();
        details.files.push_back(std::move(file));
    }
    // The newest first
    std::stable_sort(details.files.begin(), details.files.end(), [](const ModFile& a, const ModFile& b) { return a.date > b.date; });
    return details;
}

} // namespace RivalsForge::Services::Store::GameBanana

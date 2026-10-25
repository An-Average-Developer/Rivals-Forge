#include "Services/Store/Nexus.h"
#include <algorithm>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Version.h"

namespace RivalsForge::Services::Store {

namespace {

constexpr const wchar_t* kGraphQl = L"https://api.nexusmods.com/v2/graphql";
constexpr const wchar_t* kRest = L"https://api.nexusmods.com/v1";

// Nexus asks every application to say who it is
std::vector<std::pair<std::wstring, std::wstring>> Headers(const std::wstring& apiKey = L"") {
    std::vector<std::pair<std::wstring, std::wstring>> headers = {
        { L"Application-Name", Text::Widen(APP_PRODUCT) },
        { L"Application-Version", Text::Widen(APP_VERSION_STRING) },
        { L"Accept", L"application/json" },
    };
    if (!apiKey.empty()) headers.emplace_back(L"apikey", apiKey);
    return headers;
}

// What a failed request says: Nexus's own message when it sent one, else the status
std::wstring Failure(const Http::Response& response) {
    if (response.status == 0) return Loc::Get("store_error_network");
    const Json::Value body = Json::Parse(response.body);
    const std::wstring message = body[std::string_view("message")].WStr();
    if (response.status == 401) return Loc::Get("store_error_key");
    return message.empty() ? Loc::Fmt("store_error_http", response.status) : message;
}

Json::Value Query(const std::string& query, Json::Value variables) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("query", query);
    body.Set("variables", std::move(variables));
    const Http::Response response = Http::PostJson(kGraphQl, body.Serialize(), Headers());
    if (!response.Ok()) throw Error(Failure(response));
    Json::Value json = Json::Parse(response.body);
    if (json["errors"].Size() > 0) {
        Log::Warning(L"Nexus: " + Text::Widen(response.body.substr(0, 400)));
        throw Error(Loc::Fmt("store_error_api", json["errors"][size_t{0}]["message"].WStr()));
    }
    return json["data"];
}

Json::Value Rest(const std::wstring& path, const std::wstring& apiKey) {
    const Http::Response response = Http::Get(std::wstring(kRest) + path, Headers(apiKey), 20000);
    if (!response.Ok()) throw Error(Failure(response));
    return Json::Parse(response.body);
}

constexpr const char* kModFields =
    "modId name summary author version endorsements downloads pictureUrl thumbnailUrl createdAt updatedAt adultContent modCategory { name }";

StoreMod ReadMod(const Json::Value& v) {
    StoreMod m;
    m.id = v["modId"].Int();
    m.name = Text::TrimDotNet(v["name"].WStr());
    m.summary = v["summary"].WStr();
    m.author = v["author"].WStr();
    m.version = v["version"].WStr();
    m.category = v["modCategory"]["name"].WStr();
    m.pictureUrl = v["pictureUrl"].WStr();
    m.thumbnailUrl = v["thumbnailUrl"].WStr();
    m.endorsements = v["endorsements"].Int();
    m.downloads = v["downloads"].Int();
    m.createdAt = v["createdAt"].WStr();
    m.updatedAt = v["updatedAt"].WStr();
    m.adult = v["adultContent"].Bool();
    return m;
}

Json::Value NameFilter(const std::wstring& text) {
    Json::Value condition = Json::Value::MakeObject();
    condition.Set("value", text);
    condition.Set("op", "WILDCARD");
    Json::Value filter = Json::Value::MakeObject();
    filter.Set("name", Json::Value(Json::Value::Array{ condition }));
    return filter;
}

// BBCode and the <br /> Nexus mixes into it, to plain text: tags out, links kept as their text,
// list items as "•", at most one empty line in a row
std::wstring PlainText(std::wstring text) {
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const wchar_t c = text[i];
        if (c == L'<') {
            const size_t end = text.find(L'>', i);
            if (end == std::wstring::npos) break;
            const std::wstring tag = Text::Lower(text.substr(i + 1, end - i - 1));
            if (Text::StartsWith(tag, L"br") || tag == L"p" || tag == L"/p") out += L'\n';
            i = end + 1;
            continue;
        }
        if (c == L'[') {
            const size_t end = text.find(L']', i);
            if (end != std::wstring::npos && end - i < 80) {
                const std::wstring tag = Text::Lower(text.substr(i + 1, end - i - 1));
                if (tag == L"*") out += L"\n• ";
                else if (tag == L"/list" || tag == L"/center" || tag == L"/quote" || Text::StartsWith(tag, L"list")) out += L'\n';
                else if (Text::StartsWith(tag, L"img") || Text::StartsWith(tag, L"youtube")) {
                    // The picture's / video's address isn't text
                    const size_t close = text.find(L"[/" + tag.substr(0, Text::StartsWith(tag, L"img") ? 3 : 7), end);
                    if (close != std::wstring::npos) {
                        i = text.find(L']', close) + 1;
                        continue;
                    }
                }
                i = end + 1;
                continue;
            }
        }
        if (c == L'&') {
            static const std::pair<const wchar_t*, wchar_t> entities[] = { { L"&amp;", L'&' }, { L"&lt;", L'<' }, { L"&gt;", L'>' },
                                                                           { L"&quot;", L'"' }, { L"&#39;", L'\'' }, { L"&nbsp;", L' ' } };
            bool replaced = false;
            for (const auto& [entity, character] : entities)
                if (text.compare(i, wcslen(entity), entity) == 0) {
                    out += character;
                    i += wcslen(entity);
                    replaced = true;
                    break;
                }
            if (replaced) continue;
        }
        if (c != L'\r' && c != L'﻿') out += c;
        ++i;
    }
    // Lines trimmed, runs of empty lines down to one
    std::wstring result;
    int empty = 0;
    for (const auto& line : Text::Split(out, L'\n', false)) {
        const std::wstring trimmed = Text::TrimDotNet(line);
        if (trimmed.empty()) {
            if (++empty > 1 || result.empty()) continue;
        } else {
            empty = 0;
        }
        result += trimmed + L"\n";
    }
    return Text::TrimDotNet(result);
}

} // namespace

std::wstring StoreMod::PageUrl() const { return L"https://www.nexusmods.com/" + std::wstring(kGameDomain) + L"/mods/" + std::to_wstring(id); }

Page FetchPage(int page, int perPage, Sort sort, const std::wstring& search, const std::wstring& hero) {
    Json::Value game = Json::Value::MakeObject();
    {
        Json::Value condition = Json::Value::MakeObject();
        condition.Set("value", kGameDomain);
        game.Set("gameDomainName", Json::Value(Json::Value::Array{ condition }));
    }
    Json::Value::Array parts{ game };
    const std::wstring words = Text::TrimDotNet(search);
    if (!words.empty())
        for (const auto& word : Text::Split(words, L' ', true)) parts.push_back(NameFilter(word));
    if (!hero.empty()) parts.push_back(NameFilter(hero));
    {
        // Adult mods stay out of the store
        Json::Value condition = Json::Value::MakeObject();
        condition.Set("value", false);
        Json::Value adult = Json::Value::MakeObject();
        adult.Set("adultContent", Json::Value(Json::Value::Array{ condition }));
        parts.push_back(std::move(adult));
    }
    Json::Value filter = Json::Value::MakeObject();
    filter.Set("op", "AND");
    filter.Set("filter", Json::Value(std::move(parts)));

    const char* field = sort == Sort::Downloads ? "downloads" : sort == Sort::Newest ? "createdAt" : sort == Sort::Updated ? "updatedAt" : "endorsements";
    Json::Value direction = Json::Value::MakeObject();
    direction.Set("direction", "DESC");
    Json::Value order = Json::Value::MakeObject();
    order.Set(field, std::move(direction));

    Json::Value variables = Json::Value::MakeObject();
    variables.Set("filter", std::move(filter));
    variables.Set("sort", Json::Value(Json::Value::Array{ order }));
    variables.Set("offset", page * perPage);
    variables.Set("count", perPage);
    const Json::Value data = Query(std::string("query($filter: ModsFilter, $sort: [ModsSort!], $offset: Int, $count: Int) { mods(filter: $filter, "
                                               "sort: $sort, offset: $offset, count: $count) { totalCount nodes { ") +
                                       kModFields + " } } }",
                                   std::move(variables));
    Page result;
    result.total = data["mods"]["totalCount"].Int();
    for (const auto& node : data["mods"]["nodes"].Elements()) result.mods.push_back(ReadMod(node));
    return result;
}

ModDetails FetchDetails(long long modId) {
    Json::Value variables = Json::Value::MakeObject();
    variables.Set("mod", static_cast<long long>(modId));
    variables.Set("game", kGameId);
    const Json::Value data = Query(std::string("query($mod: ID!, $game: ID!) { mod(modId: $mod, gameId: $game) { description ") + kModFields +
                                       " } modFiles(modId: $mod, gameId: $game) { fileId name version category sizeInBytes date description uri } }",
                                   std::move(variables));
    ModDetails details;
    details.mod = ReadMod(data["mod"]);
    details.description = PlainText(data["mod"]["description"].WStr());
    for (const auto& f : data["modFiles"].Elements()) {
        ModFile file;
        file.id = f["fileId"].Int();
        file.name = f["name"].WStr();
        file.version = f["version"].WStr();
        file.category = f["category"].WStr();
        file.fileName = f["uri"].WStr();
        file.description = PlainText(f["description"].WStr());
        file.size = _wtoi64(f["sizeInBytes"].WStr().c_str());
        file.date = f["date"].Int();
        if (file.category == L"ARCHIVED" || file.category == L"OLD_VERSION" || file.category == L"REMOVED") continue;
        details.files.push_back(std::move(file));
    }
    // Main files first, then the rest; the newest first within each
    std::stable_sort(details.files.begin(), details.files.end(), [](const ModFile& a, const ModFile& b) {
        const bool am = a.category == L"MAIN", bm = b.category == L"MAIN";
        return am != bm ? am : a.date > b.date;
    });
    return details;
}

std::string FetchImage(const std::wstring& url) {
    const std::wstring cache = Files::Combine(Paths::DataDir(), L"StoreCache");
    const std::wstring path = Files::Combine(cache, Text::Widen(System::Sha256Hex(Text::Narrow(url)).substr(0, 32)) + L".img");
    if (auto bytes = Files::ReadBytes(path)) return *bytes;
    const Http::Response response = Http::Get(url, {}, 20000);
    if (!response.Ok()) throw Error(Failure(response));
    Files::CreateDirectories(cache);
    Files::WriteBytes(path, response.body);
    return response.body;
}

Account Validate(const std::wstring& apiKey) {
    const Json::Value v = Rest(L"/users/validate.json", apiKey);
    Account account;
    account.name = v["name"].WStr();
    account.userId = v["user_id"].Int();
    account.premium = v["is_premium"].Bool();
    return account;
}

std::wstring DownloadLink(const std::wstring& apiKey, long long modId, long long fileId, const std::wstring& nxmKey, const std::wstring& expires) {
    std::wstring path = L"/games/" + std::wstring(kGameDomain) + L"/mods/" + std::to_wstring(modId) + L"/files/" + std::to_wstring(fileId) +
                        L"/download_link.json";
    if (!nxmKey.empty()) path += L"?key=" + nxmKey + L"&expires=" + expires;
    const Json::Value links = Rest(path, apiKey);
    if (links.Size() == 0 || links[size_t{0}]["URI"].WStr().empty()) throw Error(Loc::Get("store_error_no_link"));
    return links[size_t{0}]["URI"].WStr();
}

void Download(const std::wstring& url, const std::wstring& path, const std::function<void(uint64_t, uint64_t)>& progress) {
    std::wstring error;
    const bool ok = Http::Download(
        url, path,
        [&](uint64_t received, uint64_t total) {
            if (progress) progress(received, total);
            return true;
        },
        &error);
    if (!ok) throw Error(Loc::Fmt("store_error_download", error));
}

} // namespace RivalsForge::Services::Store

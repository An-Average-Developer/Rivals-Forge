#include "Services/Store/Store.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Store/GameBanana.h"
#include "Services/Store/Nexus.h"

namespace RivalsForge::Services::Store {

std::wstring SourceName(Source source) { return source == Source::GameBanana ? L"GameBanana" : L"Nexus Mods"; }

std::wstring StoreMod::PageUrl() const {
    if (source == Source::GameBanana) return L"https://gamebanana.com/mods/" + std::to_wstring(id);
    return L"https://www.nexusmods.com/" + std::wstring(kGameDomain) + L"/mods/" + std::to_wstring(id);
}

std::wstring PlainText(std::wstring text) {
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const wchar_t c = text[i];
        if (c == L'<') {
            const size_t end = text.find(L'>', i);
            if (end == std::wstring::npos) break;
            const std::wstring tag = Text::Lower(text.substr(i + 1, end - i - 1));
            if (tag == L"li" || Text::StartsWith(tag, L"li ")) out += L"\n• ";
            else if (Text::StartsWith(tag, L"br") || tag == L"p" || tag == L"/p" || tag == L"/ul" || tag == L"/ol" || tag == L"/li" ||
                     (tag.size() == 3 && tag[0] == L'/' && tag[1] == L'h'))
                out += L'\n';
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
            // Numeric ones: "&#92;" / "&#x5C;" (Nexus writes every backslash of a path like that)
            if (i + 2 < text.size() && text[i + 1] == L'#') {
                const bool hex = text[i + 2] == L'x' || text[i + 2] == L'X';
                const size_t start = i + (hex ? 3 : 2), end = text.find(L';', start);
                if (end != std::wstring::npos && end > start && end - start <= 7) {
                    wchar_t* stop = nullptr;
                    const unsigned long code = std::wcstoul(text.c_str() + start, &stop, hex ? 16 : 10);
                    if (stop == text.c_str() + end && code > 0 && code <= 0x10FFFF) {
                        if (code > 0xFFFF) {
                            out += static_cast<wchar_t>(0xD800 + ((code - 0x10000) >> 10));
                            out += static_cast<wchar_t>(0xDC00 + ((code - 0x10000) & 0x3FF));
                        } else {
                            out += static_cast<wchar_t>(code);
                        }
                        i = end + 1;
                        continue;
                    }
                }
            }
            static const std::pair<const wchar_t*, wchar_t> entities[] = {
                { L"&amp;", L'&' },    { L"&lt;", L'<' },     { L"&gt;", L'>' },     { L"&quot;", L'"' },   { L"&apos;", L'\'' },
                { L"&nbsp;", L' ' },   { L"&ndash;", L'–' },  { L"&mdash;", L'—' },  { L"&hellip;", L'…' }, { L"&lsquo;", L'‘' },
                { L"&rsquo;", L'’' },  { L"&ldquo;", L'“' },  { L"&rdquo;", L'”' },  { L"&bull;", L'•' },   { L"&copy;", L'©' },
                { L"&reg;", L'®' },    { L"&trade;", L'™' } };
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

std::string FetchImage(const std::wstring& url) {
    const std::wstring cache = Files::Combine(Paths::DataDir(), L"StoreCache");
    const std::wstring path = Files::Combine(cache, Text::Widen(System::Sha256Hex(Text::Narrow(url)).substr(0, 32)) + L".img");
    if (auto bytes = Files::ReadBytes(path)) return *bytes;
    const Http::Response response = Http::Get(url, {}, 20000);
    if (!response.Ok()) throw Error(response.status == 0 ? Loc::Get("store_error_network") : Loc::Fmt("store_error_http", response.status));
    Files::CreateDirectories(cache);
    Files::WriteBytes(path, response.body);
    return response.body;
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

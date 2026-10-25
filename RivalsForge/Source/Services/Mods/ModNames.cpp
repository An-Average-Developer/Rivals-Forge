#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include <algorithm>
#include "Services/Mods/ModService.h"

// ModService.cs's ZIP path helpers and unique names: the mod names never repeat (ignoring case)
// and neither do the ZIPs in MODS (a .zip and a .zip.off both count).
namespace RivalsForge::Services::Mods {

namespace {

// Path.GetInvalidFileNameChars(): the control characters and " < > | : * ? \ /
bool InvalidFileNameChar(wchar_t c) {
    if (c < 32) return true;
    switch (c) {
        case L'"': case L'<': case L'>': case L'|': case L':': case L'*': case L'?': case L'\\': case L'/': return true;
        default: return false;
    }
}

} // namespace

// Falls back to mod.Id for mods created before ZipFileName was introduced
std::wstring ModService::ZipBaseName(const Models::Mod& mod) { return mod.zipFileName.empty() ? mod.id : mod.zipFileName; }

std::wstring ModService::ZipPath(const Models::Mod& mod) {
    return Files::DotNetCombine(ModsStorageDir(), ZipBaseName(mod) + Config::ZipExtension);
}

std::wstring ModService::ZipOffPath(const Models::Mod& mod) {
    return Files::DotNetCombine(ModsStorageDir(), ZipBaseName(mod) + Config::ZipOffExtension);
}

std::wstring ModService::SanitizeFileName(const std::wstring& name) {
    std::wstring clean = name;
    for (auto& c : clean)
        if (InvalidFileNameChar(c)) c = L'_';
    clean = Text::TrimDotNet(clean);
    return clean.empty() ? L"mod" : clean;
}

// Returns a display name that doesn't collide with any existing mod name
std::wstring ModService::UniqueModName(const std::wstring& name) const {
    std::wstring candidate = name;
    int n = 0;
    const auto taken = [&](const std::wstring& c) {
        for (const auto& m : mods)
            if (Text::Equals(m.name, c, true)) return true;   // StringComparison.OrdinalIgnoreCase
        return false;
    };
    while (taken(candidate)) candidate = name + L" (" + std::to_wstring(++n) + L")";
    return candidate;
}

std::wstring ModService::ZipNameFor(const std::wstring& name, const std::vector<std::wstring>& pending) const {
    // What PrepareAddMod does, with the pending mods added one after the other first
    std::vector<std::wstring> names, zips;
    const auto unique = [&](const std::wstring& n) {
        std::wstring candidate = UniqueModName(n);
        for (int k = 0; std::any_of(names.begin(), names.end(), [&](const std::wstring& t) { return Text::Equals(t, candidate, true); });)
            candidate = UniqueModName(n + L" (" + std::to_wstring(++k) + L")");
        return candidate;
    };
    const auto zipBase = [&](const std::wstring& modName) {
        const std::wstring sanitized = SanitizeFileName(modName);
        std::wstring candidate = UniqueZipBaseName(modName);
        for (int k = 0; std::any_of(zips.begin(), zips.end(), [&](const std::wstring& t) { return Text::Equals(t, candidate, true); });)
            candidate = sanitized + L" (" + std::to_wstring(++k) + L")";
        return candidate;
    };
    for (const auto& p : pending) {
        names.push_back(unique(p));
        zips.push_back(zipBase(names.back()));
    }
    return zipBase(unique(name)) + Config::ZipExtension;
}

// Returns a base name (no extension) that doesn't collide with any existing ZIP in MODS
std::wstring ModService::UniqueZipBaseName(const std::wstring& modName) {
    const std::wstring sanitized = SanitizeFileName(modName);
    std::wstring candidate = sanitized;
    int n = 0;
    while (FileOps::FileExists(Files::DotNetCombine(ModsStorageDir(), candidate + Config::ZipExtension)) ||
           FileOps::FileExists(Files::DotNetCombine(ModsStorageDir(), candidate + Config::ZipOffExtension)))
        candidate = sanitized + L" (" + std::to_wstring(++n) + L")";
    return candidate;
}

} // namespace RivalsForge::Services::Mods

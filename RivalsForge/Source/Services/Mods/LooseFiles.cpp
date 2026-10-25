#include "Services/Mods/LooseFiles.h"
#include <windows.h>
#include <map>
#include <unordered_map>
#include <vector>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace RivalsForge::Services::Mods::LooseFiles {

namespace {

bool IsPakFile(const std::wstring& name) {
    const std::wstring ext = Text::Lower(Files::Extension(name));
    return ext == L".pak" || ext == L".ucas" || ext == L".utoc";
}

std::wstring Backslashes(std::wstring path) {
    for (auto& c : path)
        if (c == L'/') c = L'\\';
    return path;
}

std::wstring Trimmed(std::wstring path) {
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    return path;
}

// Originals\<the installation> : a short hash of its Content folder (Steam and Epic keep theirs apart)
std::wstring OriginalsRoot(const std::wstring& paks) {
    uint64_t hash = 1469598103934665603ull;   // FNV-1a
    for (wchar_t c : Text::Lower(ContentDir(paks))) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return Files::Combine(Paths::DataDir(), L"Originals", Text::Hex(hash, 16));
}

// `path` relative to `folder` ("" when it isn't inside it); paths compared without case
std::wstring Under(const std::wstring& folder, const std::wstring& path) {
    const std::wstring root = Trimmed(Files::FullPath(folder)) + L"\\";
    const std::wstring full = Files::FullPath(path);
    if (!Text::StartsWith(full, root, true)) return L"";
    return Trimmed(full.substr(root.size()));
}

// Where the copy of `path` (under Content) is kept; "" when it isn't under Content
std::wstring OriginalOf(const std::wstring& paks, const std::wstring& path) {
    const std::wstring relative = Under(ContentDir(paks), path);
    return relative.empty() ? L"" : Files::Combine(OriginalsRoot(paks), relative);
}

} // namespace

bool IsExtra(const std::wstring& name) {
    static const wchar_t* const kExtras[] = { L".txt", L".md", L".pdf", L".rtf", L".doc", L".docx", L".html", L".htm", L".url",
                                              L".png", L".jpg", L".jpeg", L".gif", L".webp", L".bmp" };
    const std::wstring ext = Text::Lower(Files::Extension(name));
    for (const wchar_t* extra : kExtras)
        if (ext == extra) return true;
    return false;
}

bool IsLoose(const Zip::Reader& archive) {
    bool any = false;
    for (const auto& entry : archive.Entries()) {
        if (entry.name.empty() || IsExtra(entry.name)) continue;
        if (IsPakFile(entry.name)) return false;
        any = true;
    }
    return any;
}

std::wstring ContentDir(const std::wstring& paks) { return Files::DirectoryName(Trimmed(paks)); }

std::wstring TargetDir(const std::wstring& paks, const std::wstring& target) {
    return target == L"." || target.empty() ? ContentDir(paks) : Files::Combine(ContentDir(paks), target);
}

std::optional<Placement> Locate(const Zip::Reader& archive, const std::wstring& paks) {
    // The game's own files under Content (Paks left out), by name
    const std::wstring content = ContentDir(paks);
    const std::wstring paksLower = Text::Lower(Trimmed(paks)) + L"\\";
    std::unordered_map<std::wstring, std::vector<std::wstring>> byName;
    for (const auto& file : Files::ListFiles(content, L"*", true)) {
        if (Text::StartsWith(Text::Lower(file), paksLower)) continue;
        byName[Text::Lower(Files::FileName(file))].push_back(file);
    }

    // Each archive file votes for the folder its copy in the game is under: the longest tail of its
    // path ("COMMON\MarvelLogoVideo_Common.bk2") that the game's file ends with; what's before that
    // tail in the archive ("Skip Intro/") is left out
    std::map<std::pair<std::wstring, std::wstring>, int> votes;   // (root, strip), the root in lower case
    std::map<std::wstring, std::wstring> roots;                    // lower case -> as on disk
    for (const auto& entry : archive.Entries()) {
        if (entry.name.empty() || IsExtra(entry.name)) continue;
        const auto found = byName.find(Text::Lower(entry.name));
        if (found == byName.end()) continue;
        const auto segments = Text::Split(Backslashes(entry.fullName), L'\\', true);
        bool matched = false;
        for (size_t k = 0; k < segments.size() && !matched; ++k) {
            std::vector<std::wstring> tail(segments.begin() + static_cast<std::ptrdiff_t>(k), segments.end());
            const std::wstring suffix = L"\\" + Text::Join(tail, L"\\");
            for (const auto& game : found->second) {
                if (!Text::EndsWith(game, suffix, true)) continue;
                const std::wstring root = game.substr(0, game.size() - suffix.size());
                std::vector<std::wstring> head(segments.begin(), segments.begin() + static_cast<std::ptrdiff_t>(k));
                ++votes[{ Text::Lower(root), head.empty() ? L"" : Text::Join(head, L"/") + L"/" }];
                roots[Text::Lower(root)] = root;
                matched = true;
                break;
            }
        }
    }
    if (votes.empty()) return std::nullopt;
    const auto best = std::max_element(votes.begin(), votes.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    Placement placement;
    const std::wstring root = roots[best->first.first];
    if (Text::Equals(Trimmed(root), Trimmed(content), true)) placement.target = L".";
    else placement.target = Under(content, root);
    if (placement.target.empty()) return std::nullopt;
    placement.strip = best->first.second;
    return placement;
}

bool KeepOriginal(const std::wstring& paks, const std::wstring& path) {
    if (!Files::Exists(path)) return true;   // a new file: nothing to keep
    const std::wstring copy = OriginalOf(paks, path);
    if (copy.empty()) return false;
    if (Files::Exists(copy)) return true;
    Files::CreateDirectories(Files::DirectoryName(copy));
    return Files::Copy(path, copy, false);
}

bool RestoreOriginal(const std::wstring& paks, const std::wstring& path) {
    const std::wstring copy = OriginalOf(paks, path);
    if (copy.empty() || !Files::Exists(copy)) return false;
    if (!Files::Copy(copy, path, true)) return false;
    Files::Delete(copy);
    return true;
}

} // namespace RivalsForge::Services::Mods::LooseFiles

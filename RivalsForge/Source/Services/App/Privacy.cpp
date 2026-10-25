#include "Services/App/Privacy.h"
#include <windows.h>
#include <mutex>
#include <vector>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace RivalsForge::Services::Privacy {

namespace {

std::mutex mutex;
std::vector<std::wstring> names;   // remembered personal names

// Replaces every `from` in `text` (case ignored) with `to`
void ReplaceAll(std::wstring& text, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    const std::wstring lowerFrom = Text::Lower(from);
    std::wstring lower = Text::Lower(text);
    for (size_t at = lower.find(lowerFrom); at != std::wstring::npos; at = lower.find(lowerFrom, at + to.size())) {
        text.replace(at, from.size(), to);
        lower.replace(at, from.size(), Text::Lower(to));
    }
}

// The Windows profile folder's name ("alan" for C:\Users\alan): what the paths give away
const std::wstring& ProfileName() {
    static const std::wstring name = [] {
        wchar_t buffer[MAX_PATH]{};
        const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH);
        return n > 0 && n < MAX_PATH ? Files::FileName(buffer) : std::wstring();
    }();
    return name;
}

} // namespace

bool Active() { return Config::PresentationMode; }

std::wstring Mask(const std::wstring& text) {
    if (!Active() || text.empty()) return text;
    std::wstring shown = text;
    // In paths only ("\Users\alan\" / "/Users/alan/"): the bare word may be part of other text
    if (const std::wstring& profile = ProfileName(); !profile.empty()) {
        ReplaceAll(shown, L"\\Users\\" + profile, std::wstring(L"\\Users\\") + Config::PresentationName);
        ReplaceAll(shown, L"/Users/" + profile, std::wstring(L"/Users/") + Config::PresentationName);
    }
    std::lock_guard lock(mutex);
    for (const auto& name : names) ReplaceAll(shown, name, Config::PresentationName);
    return shown;
}

void Remember(const std::wstring& name) {
    if (Text::Trim(name).size() < 2) return;
    std::lock_guard lock(mutex);
    for (const auto& known : names)
        if (Text::Equals(known, name, true)) return;
    names.push_back(name);
}

} // namespace RivalsForge::Services::Privacy

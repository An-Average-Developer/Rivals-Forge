#include "Services/Store/NexusAccount.h"
#include <windows.h>
#include <dpapi.h>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Services/App/Log.h"

namespace RivalsForge::Services::Store::NexusAccount {

namespace {

constexpr const wchar_t* kKeyFile = L"nexus.key";
constexpr const wchar_t* kPreviousFile = L"nxm.previous";   // the command nxm:// had before
constexpr const wchar_t* kClassKey = L"Software\\Classes\\nxm";
constexpr const wchar_t* kCommandKey = L"Software\\Classes\\nxm\\shell\\open\\command";

std::wstring OurCommand() { return L"\"" + Files::ExecutablePath() + L"\" \"%1\""; }

std::wstring ReadCommand() {
    wchar_t value[2048]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kCommandKey, nullptr, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS) return L"";
    return value;
}

bool WriteString(HKEY root, const wchar_t* path, const wchar_t* name, const std::wstring& value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    const LSTATUS status = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

std::wstring QueryValue(const std::wstring& query, const std::wstring& name) {
    for (const auto& pair : Text::Split(query, L'&', true)) {
        const size_t eq = pair.find(L'=');
        if (eq != std::wstring::npos && Text::Equals(pair.substr(0, eq), name, true)) return pair.substr(eq + 1);
    }
    return L"";
}

} // namespace

std::wstring LoadKey() {
    const auto bytes = Files::ReadBytes(Files::Combine(Paths::DataDir(), kKeyFile));
    if (!bytes || bytes->empty()) return L"";
    DATA_BLOB in{ static_cast<DWORD>(bytes->size()), reinterpret_cast<BYTE*>(const_cast<char*>(bytes->data())) }, out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return L"";
    std::wstring key(reinterpret_cast<const wchar_t*>(out.pbData), out.cbData / sizeof(wchar_t));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return key;
}

void SaveKey(const std::wstring& key) {
    const std::wstring path = Files::Combine(Paths::DataDir(), kKeyFile);
    if (key.empty()) {
        DeleteFileW(path.c_str());
        return;
    }
    DATA_BLOB in{ static_cast<DWORD>(key.size() * sizeof(wchar_t)), reinterpret_cast<BYTE*>(const_cast<wchar_t*>(key.data())) }, out{};
    if (!CryptProtectData(&in, L"Nexus Mods API key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        Log::Warning(L"Nexus: the API key could not be encrypted");
        return;
    }
    Files::WriteBytes(path, out.pbData, out.cbData);
    LocalFree(out.pbData);
}

std::optional<NxmLink> ParseNxm(const std::wstring& url) {
    // nxm://<game>/mods/<mod>/files/<file>?key=...&expires=...
    const std::wstring prefix = L"nxm://";
    if (!Text::StartsWith(url, prefix, true)) return std::nullopt;
    const std::wstring rest = url.substr(prefix.size());
    const size_t question = rest.find(L'?');
    const std::wstring path = rest.substr(0, question);
    const std::wstring query = question == std::wstring::npos ? L"" : rest.substr(question + 1);
    const auto parts = Text::Split(path, L'/', true);
    if (parts.size() < 5 || !Text::Equals(parts[1], L"mods", true) || !Text::Equals(parts[3], L"files", true)) return std::nullopt;
    NxmLink link;
    link.game = parts[0];
    link.modId = _wtoi64(parts[2].c_str());
    link.fileId = _wtoi64(parts[4].c_str());
    link.key = QueryValue(query, L"key");
    link.expires = QueryValue(query, L"expires");
    if (link.modId <= 0 || link.fileId <= 0) return std::nullopt;
    return link;
}

bool HandlerIsOurs() { return Text::Equals(ReadCommand(), OurCommand(), true); }

bool RegisterHandler() {
    if (HandlerIsOurs()) return true;
    // The program that had the links before gets them back when this is turned off
    const std::wstring previous = ReadCommand();
    Files::WriteText(Files::Combine(Paths::DataDir(), kPreviousFile), previous);
    const bool ok = WriteString(HKEY_CURRENT_USER, kClassKey, nullptr, L"URL:NXM Protocol") &&
                    WriteString(HKEY_CURRENT_USER, kClassKey, L"URL Protocol", L"") &&
                    WriteString(HKEY_CURRENT_USER, kCommandKey, nullptr, OurCommand());
    if (!ok) Log::Warning(L"Nexus: nxm:// links could not be registered");
    return ok;
}

void RepairHandler() {
    const std::wstring command = ReadCommand();
    if (command.find(L"MarvelRivalsModManager.exe") == std::wstring::npos || HandlerIsOurs()) return;
    if (WriteString(HKEY_CURRENT_USER, kCommandKey, nullptr, OurCommand())) Log::Info(L"Nexus: nxm:// links moved to " + Files::ExecutablePath());
}

void UnregisterHandler() {
    if (!HandlerIsOurs()) return;
    const std::wstring file = Files::Combine(Paths::DataDir(), kPreviousFile);
    const std::wstring previous = Text::TrimDotNet(Files::ReadText(file).value_or(L""));
    if (!previous.empty()) WriteString(HKEY_CURRENT_USER, kCommandKey, nullptr, previous);
    else RegDeleteTreeW(HKEY_CURRENT_USER, kClassKey);
    DeleteFileW(file.c_str());
}

} // namespace RivalsForge::Services::Store::NexusAccount

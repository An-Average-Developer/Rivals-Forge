#include "App/Paths.h"
#include <windows.h>
#include "App/Config.h"
#include "Core/Files.h"

namespace RivalsForge::Paths {

namespace {

// RF_DATA_DIR (Debug only), empty when not set
const std::wstring& DataDirOverride() {
    static const std::wstring dir = [] {
#ifdef _DEBUG
        wchar_t value[MAX_PATH]{};
        if (GetEnvironmentVariableW(Config::DataDirOverrideVariable, value, MAX_PATH) > 0) return Files::FullPath(value);
#endif
        return std::wstring();
    }();
    return dir;
}

} // namespace

const std::wstring& DataDir() {
    static const std::wstring dir = DataDirOverride().empty() ? Files::Combine(Files::LocalAppData(), Config::DataFolder) : DataDirOverride();
    return dir;
}

void MigrateDataFolder() {
    if (!DataDirOverride().empty()) return;
    const std::wstring legacy = Files::Combine(Files::LocalAppData(), Config::LegacyDataFolder);
    if (Files::DirectoryExists(legacy) && !Files::DirectoryExists(DataDir())) {
        // Same drive: a rename. (The log isn't up yet; the result shows in the next start's log.)
        if (!MoveFileExW(legacy.c_str(), DataDir().c_str(), MOVEFILE_COPY_ALLOWED)) Files::CopyDirectory(legacy, DataDir());
    }
    const std::wstring legacyTemp = Files::Combine(Files::TempDirectory(), Config::LegacyDataFolder);
    if (Files::DirectoryExists(legacyTemp)) Files::DeleteDirectory(legacyTemp);
}

std::wstring DataFile(const std::wstring& name) { return Files::Combine(DataDir(), name); }

std::wstring ModsDir() { return Files::Combine(DataDir(), Config::ModsFolder); }

std::wstring OldRoamingDir() {
#ifdef _DEBUG
    // Development aid: RF_ROAMING_DIR=<folder> plays the old Roaming folder (migration tests)
    wchar_t roaming[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"RF_ROAMING_DIR", roaming, MAX_PATH) > 0) return Files::FullPath(roaming);
#endif
    if (!DataDirOverride().empty()) return L"";
    return Files::Combine(Files::RoamingAppData(), Config::LegacyDataFolder);
}

const std::wstring& TempDir() {
    static const std::wstring dir = Files::Combine(Files::TempDirectory(), Config::DataFolder);
    return dir;
}

std::wstring Asset(const std::wstring& relative) { return Files::Combine(TempDir(), L"Assets", relative); }

std::wstring Translation(const std::wstring& fileName) { return Files::Combine(TempDir(), L"Translations", fileName); }

} // namespace RivalsForge::Paths

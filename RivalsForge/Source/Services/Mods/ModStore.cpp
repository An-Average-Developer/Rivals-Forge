#include <windows.h>
#include <cstdio>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Models/AppData.h"
#include "Services/App/Log.h"
#include "Services/Mods/ModService.h"

// ModService.cs's persistence: data.json in %LOCALAPPDATA%\RivalsForge, written the way
// System.Text.Json wrote it, and the two one-time migrations (the old Roaming folder, and mods.json
// from before sections existed). Unlike the C#, a data.json that can't be read is kept as a copy
// before anything overwrites it, and data.json is written to a temporary file first and then
// swapped in, so a crash half way through a save can't leave it broken.
namespace RivalsForge::Services::Mods {

namespace {

std::wstring DataFile() { return Paths::DataFile(Config::DataFileName); }
std::wstring LegacyFile() { return Paths::DataFile(Config::LegacyFileName); }

// data.json.corrupt-<yyyyMMdd-HHmmss>: where an unreadable data.json is kept
std::wstring BackupPath() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t stamp[32];
    swprintf_s(stamp, L".corrupt-%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return DataFile() + stamp;
}

// File.ReadAllText + JsonSerializer.Deserialize's parsing; null (and false) when either fails
bool ReadJson(const std::wstring& path, Json::Value& out) {
    const auto bytes = Files::ReadBytes(path);
    if (!bytes) return false;
    bool ok = false;
    out = Json::Parse(std::string_view(*bytes), &ok);
    return ok;
}

} // namespace

// Moves data.json / mods.json from the old AppData\Roaming location to Local. The C# then deleted the
// whole old folder, whatever else was in it; here only those two files go (a stale copy too, when
// Local already had one), and the folder only when that leaves it empty.
void ModService::MigrateFromRoaming() {
    const std::wstring oldDir = Paths::OldRoamingDir();
    if (oldDir.empty() || !Files::DirectoryExists(oldDir)) return;

    for (const wchar_t* file : { Config::DataFileName, Config::LegacyFileName }) {
        const std::wstring src = Files::DotNetCombine(oldDir, file);
        const std::wstring dst = Paths::DataFile(file);
        if (FileOps::FileExists(src) && !FileOps::FileExists(dst)) FileOps::Move(src, dst);
        else if (FileOps::FileExists(src)) DeleteFileW(src.c_str());
    }
    RemoveDirectoryW(oldDir.c_str());   // fails (and stays) when something else is in it
}

void ModService::Load() {
    // Migrate old mods.json -> data.json
    if (!FileOps::FileExists(DataFile()) && FileOps::FileExists(LegacyFile())) {
        Json::Value root;
        if (ReadJson(LegacyFile(), root)) {
            if (auto list = Models::ReadModList(root)) mods = std::move(*list);
        }
        Save();
        return;
    }

    if (!FileOps::FileExists(DataFile())) return;

    Json::Value root;
    std::optional<Models::AppData> data;
    if (ReadJson(DataFile(), root)) data = Models::ReadAppData(root);
    if (data) {
        paksDir = data->gamePath.value_or(L"");
        sections = std::move(data->sections);
        mods = std::move(data->mods);
    } else {
        // catch { _sections = []; _mods = []; } — the file is copied first: the next save would
        // overwrite the user's sections and mods with nothing
        const std::wstring backup = BackupPath();
        if (CopyFileW(DataFile().c_str(), backup.c_str(), TRUE)) loadBackup = backup;
        Log::Warning(L"data.json could not be read; starting with no sections or mods. Copy: " + backup);
        sections.clear();
        mods.clear();
    }
}

void ModService::Save() {
    Models::AppData data;
    data.gamePath = paksDir;
    data.sections = sections;
    data.mods = mods;
    // data.json.tmp, then swapped in (the old data.json stays whole until the new one is complete)
    const std::wstring temp = DataFile() + L".tmp";
    FileOps::WriteAllBytes(temp, Models::WriteAppData(data).SerializeSystemTextJson());
    if (!MoveFileExW(temp.c_str(), DataFile().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw ModError(FileOps::IoMessage(GetLastError(), DataFile()));
}

} // namespace RivalsForge::Services::Mods

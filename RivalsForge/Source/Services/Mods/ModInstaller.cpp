#include <windows.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_set>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Localization/Localization.h"
#include "Services/Mods/CharacterDetector.h"
#include "Services/Mods/LooseFiles.h"
#include "Services/Mods/ModService.h"

// ModService.cs's mod operations and ExtractZip / DeleteExtractedFiles, plus the file system calls
// they make with .NET's error messages. The order of every step is the C#'s, with three safety
// changes: a ZIP with an entry that would land outside Paks is refused before anything is written,
// a failed extraction removes the files it had created (and a failed Enable puts the .zip.off back),
// and removing a mod's files spares those another enabled mod also has. New: mods of loose game
// files (no .pak: the "skip intro" videos) go over the game's own files where the game has them,
// the originals kept aside first and put back when the mod is disabled or deleted (LooseFiles).
namespace RivalsForge::Services::Mods {

namespace {

[[noreturn]] void ThrowLastError(const std::wstring& path) { throw ModError(FileOps::IoMessage(GetLastError(), path)); }

// A path as Windows resolves it ("..", "/" and the like gone), lower case, for comparisons
std::wstring Resolved(const std::wstring& path) {
    const DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (n == 0) return Text::Lower(path);
    std::wstring full(n, L'\0');
    full.resize(GetFullPathNameW(path.c_str(), n, full.data(), nullptr));
    return Text::Lower(full);
}

// Whether `path` resolves to something inside `folder`
bool InsideFolder(const std::wstring& path, const std::wstring& folder) {
    std::wstring root = Resolved(folder);
    if (!root.empty() && root.back() != L'\\') root += L'\\';
    return Text::StartsWith(Resolved(path), root);
}

} // namespace

// ── FileOps ─────────────────────────────────────────────────────────────────

namespace FileOps {

bool FileExists(const std::wstring& path) { return Files::Exists(path); }

void MakeDirectory(const std::wstring& path) {
    if (Files::DirectoryExists(path)) return;
    if (!Files::CreateDirectories(path)) ThrowLastError(path);
}

void Copy(const std::wstring& from, const std::wstring& to, const Progress& progress) {
    const auto routine = [](LARGE_INTEGER total, LARGE_INTEGER done, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE,
                            LPVOID data) -> DWORD {
        const auto* report = static_cast<const Progress*>(data);
        if (*report && total.QuadPart > 0) (*report)(static_cast<float>(static_cast<double>(done.QuadPart) / static_cast<double>(total.QuadPart)));
        return PROGRESS_CONTINUE;
    };
    if (!CopyFileExW(from.c_str(), to.c_str(), routine, const_cast<Progress*>(&progress), nullptr, 0)) {
        // .NET names the source when it's the one missing, otherwise the destination
        const DWORD error = GetLastError();
        throw ModError(IoMessage(error, error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? from : to));
    }
}

void Move(const std::wstring& from, const std::wstring& to) {
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) ThrowLastError(from);
}

void Delete(const std::wstring& path) {
    if (!DeleteFileW(path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) ThrowLastError(path);
}

void WriteAllBytes(const std::wstring& path, const std::string& bytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) ThrowLastError(path);
    DWORD written = 0;
    const BOOL ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    const DWORD error = GetLastError();
    CloseHandle(h);
    if (!ok || written != bytes.size()) throw ModError(IoMessage(error, path));
}

std::wstring ZipMessage(const Zip::Failure& f, const std::wstring& path) {
    switch (f.error) {
        case Zip::Error::OpenFailed:
        case Zip::Error::WriteFailed: return FileOps::IoMessage(f.win32, path);
        case Zip::Error::NotAZip: return Loc::Get("error_zip_not_a_zip");
        case Zip::Error::Unsupported: return Loc::Get("error_zip_unsupported");
        case Zip::Error::Encrypted: return Loc::Get("error_zip_encrypted");
        case Zip::Error::BadCrc: return Loc::Fmt("error_zip_crc", path);
        default: return Loc::Get("error_zip_corrupt");
    }
}

std::wstring IoMessage(unsigned long error, const std::wstring& path) {
    switch (error) {
        case ERROR_FILE_NOT_FOUND: return Loc::Fmt("error_io_file_not_found", path);
        case ERROR_PATH_NOT_FOUND: return Loc::Fmt("error_io_path_not_found", path);
        case ERROR_ACCESS_DENIED: return Loc::Fmt("error_io_access_denied", path);
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION: return Loc::Fmt("error_io_in_use", path);
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL: return Loc::Fmt("error_io_disk_full", path);
        default: break;
    }
    // Anything else: the system's own text, like the IOException .NET builds from it
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
                   reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring message = text ? text : L"";
    if (text) LocalFree(text);
    while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r' || message.back() == L' ')) message.pop_back();
    return Loc::Fmt("error_io_other", message, path);
}

} // namespace FileOps

// ── Mods ────────────────────────────────────────────────────────────────────

const Models::Mod& ModService::AddMod(const std::wstring& sourceZipPath, const std::wstring& name, const std::optional<std::wstring>& character) {
    Models::Mod mod = PrepareAddMod(name, character);
    InstallMod(mod, sourceZipPath, paksDir);
    return CommitAddMod(std::move(mod));
}

Models::Mod ModService::PrepareAddMod(const std::wstring& name, const std::optional<std::wstring>& character) {
    EnsureConfigured();
    FileOps::MakeDirectory(ModsStorageDir());

    const std::wstring uniqueName = UniqueModName(name);
    Models::Mod mod = Models::Mod::New();
    mod.name = uniqueName;
    mod.character = character;   // the hero card's "＋ Mod"; otherwise found after the extraction
    mod.zipFileName = UniqueZipBaseName(uniqueName);
    return mod;
}

void ModService::InstallMod(Models::Mod& mod, const std::wstring& sourceZipPath, const std::wstring& paks, const Progress& progress) {
    // Copy ZIP into MODS storage under the readable name (a quarter of the progress bar), then
    // extract to Paks and record the file list. If either fails the copy goes away again (the C#
    // left it in MODS, a ZIP no mod uses); its name was free, so it's never someone else's file.
    try {
        FileOps::Copy(sourceZipPath, ZipPath(mod), [&](float k) {
            if (progress) progress(0.25f * k);
        });
        // Loose game files: where they go is where the game has the same files
        {
            Zip::Failure failure;
            const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(ZipPath(mod), &failure);
            if (!archive) throw ModError(FileOps::ZipMessage(failure, ZipPath(mod)));
            mod.target.clear();
            mod.targetStrip.clear();
            if (LooseFiles::IsLoose(*archive)) {
                const auto placement = LooseFiles::Locate(*archive, paks);
                if (!placement) throw ModError(Loc::Get("error_loose_unknown"));
                mod.target = placement->target;
                mod.targetStrip = placement->strip;
            }
        }
        mod.files = ExtractZip(mod, ZipPath(mod), paks, [&](float k) {
            if (progress) progress(0.25f + 0.75f * k);
        });
    } catch (const ModError&) {
        DeleteFileW(ZipPath(mod).c_str());
        throw;
    }
    if (!mod.character) mod.character = CharacterDetector::Detect(ZipPath(mod)).value_or(L"");
    mod.isEnabled = true;
}

const Models::Mod& ModService::CommitAddMod(Models::Mod mod) {
    mods.push_back(std::move(mod));
    Save();
    return mods.back();
}

void ModService::EnableMod(const std::wstring& modId) {
    auto mod = PrepareEnableMod(modId);
    if (!mod) return;
    RestoreMod(*mod, paksDir);
    CommitEnableMod(*mod);
}

std::wstring ModService::DisabledDir(const std::wstring& paks, const Models::Mod& mod) {
    std::wstring root = paks;
    while (!root.empty() && (root.back() == L'\\' || root.back() == L'/')) root.pop_back();
    return Files::Combine(Files::DirectoryName(root), Config::DisabledFolder, mod.id);
}

namespace {

// The folder a test version moved a disabled mod's files to, gone (and its parent once empty)
void RemoveDisabledDir(const std::wstring& dir) {
    if (!Files::DirectoryExists(dir)) return;
    Files::DeleteDirectory(dir);
    RemoveDirectoryW(Files::DirectoryName(dir).c_str());   // fails while it holds other mods
}

} // namespace

std::optional<Models::Mod> ModService::PrepareEnableMod(const std::wstring& modId) {
    const Models::Mod* mod = FindMod(modId);
    if (!mod || mod->isEnabled) return std::nullopt;
    EnsureConfigured();
    const std::wstring offPath = ZipOffPath(*mod);
    if (!FileOps::FileExists(offPath)) throw ModError(Loc::Fmt("error_archive_not_found", offPath));
    return *mod;
}

void ModService::RestoreMod(Models::Mod& mod, const std::wstring& paks, const Progress& progress) {
    FileOps::Move(ZipOffPath(mod), ZipPath(mod));
    try {
        mod.files = ExtractZip(mod, ZipPath(mod), paks, progress);
    } catch (const ModError&) {
        // The mod stays disabled: its archive goes back to .zip.off (the C# left a .zip behind)
        MoveFileExW(ZipPath(mod).c_str(), ZipOffPath(mod).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
        throw;
    }
    RemoveDisabledDir(DisabledDir(paks, mod));
    mod.isEnabled = true;
}

void ModService::CommitEnableMod(const Models::Mod& restored) {
    if (Models::Mod* mod = EditMod(restored.id)) {
        mod->files = restored.files;
        mod->isEnabled = true;
    }
    Save();
}

void ModService::DisableMod(const std::wstring& modId) {
    Models::Mod* mod = EditMod(modId);
    if (!mod || !mod->isEnabled) return;
    EnsureConfigured();

    DeleteExtractedFiles(*mod);

    const std::wstring zipPath = ZipPath(*mod);
    if (FileOps::FileExists(zipPath)) FileOps::Move(zipPath, ZipOffPath(*mod));

    mod->isEnabled = false;
    Save();
}

void ModService::DeleteMod(const std::wstring& modId) {
    const Models::Mod* mod = FindMod(modId);
    if (!mod) return;
    EnsureConfigured();

    DeleteExtractedFiles(*mod);
    RemoveDisabledDir(DisabledDir(paksDir, *mod));

    if (FileOps::FileExists(ZipPath(*mod))) FileOps::Delete(ZipPath(*mod));
    if (FileOps::FileExists(ZipOffPath(*mod))) FileOps::Delete(ZipOffPath(*mod));

    mods.erase(std::find_if(mods.begin(), mods.end(), [&](const Models::Mod& m) { return m.id == modId; }));
    Save();
}

void ModService::ForgetMod(const std::wstring& modId) {
    const auto it = std::find_if(mods.begin(), mods.end(), [&](const Models::Mod& m) { return m.id == modId; });
    if (it == mods.end()) return;
    mods.erase(it);
    Save();
}

ModService::CopyResult ModService::CopyMods(const std::vector<Models::Mod>& mods, const std::wstring& to, const Progress& progress) {
    CopyResult result;
    FileOps::MakeDirectory(to);
    for (size_t i = 0; i < mods.size(); ++i) {
        Models::Mod mod = mods[i];
        try {
            mod.files = ExtractZip(mod, ZipPath(mod), to, [&](float k) {
                if (progress) progress((static_cast<float>(i) + k) / static_cast<float>(mods.size()));
            });
            result.copied.push_back(std::move(mod));
        } catch (const ModError& e) {
            result.failed.emplace_back(mod.id, e.Message());
        }
    }
    return result;
}

void ModService::SwitchTo(const std::wstring& to, const CopyResult& copy) {
    FileOps::MakeDirectory(to);
    paksDir = to;
    for (const auto& copied : copy.copied)
        if (Models::Mod* mod = EditMod(copied.id)) mod->files = copied.files;
    // Enabled = its files are in this installation (loose files replace the game's own, which are
    // always there: those mods are enabled when they were copied over)
    for (auto& mod : mods) {
        bool present = !mod.files.empty();
        for (const auto& file : mod.files)
            if (present && !FileOps::FileExists(Files::DotNetCombine(FilesRoot(mod, paksDir), file))) present = false;
        if (!mod.target.empty())
            present = std::any_of(copy.copied.begin(), copy.copied.end(), [&](const Models::Mod& m) { return m.id == mod.id; });
        mod.isEnabled = present;
        const std::wstring zip = ZipPath(mod), off = ZipOffPath(mod);
        if (present && !FileOps::FileExists(zip) && FileOps::FileExists(off)) MoveFileExW(off.c_str(), zip.c_str(), MOVEFILE_REPLACE_EXISTING);
        if (!present && FileOps::FileExists(zip)) MoveFileExW(zip.c_str(), off.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    Save();
}

void ModService::RenameMod(const std::wstring& modId, const std::wstring& newName) {
    Models::Mod* mod = EditMod(modId);
    if (!mod) return;
    // Names don't repeat (AddMod makes them unique; the C# let a rename repeat one)
    for (const auto& other : mods)
        if (other.id != modId && Text::Equals(other.name, newName, true)) throw ModError(Loc::Fmt("error_mod_name_taken", newName));

    // Rename the ZIP on disk to match the new name
    const std::wstring oldZip = ZipPath(*mod), oldOff = ZipOffPath(*mod);
    const std::wstring sanitized = SanitizeFileName(newName);

    mod->name = newName;
    // The same file name (or only its case changes): the ZIP keeps it instead of colliding with
    // itself and becoming "<name> (1)"
    mod->zipFileName = Text::Equals(sanitized, ZipBaseName(*mod), true) ? sanitized : UniqueZipBaseName(newName);

    if (FileOps::FileExists(oldZip)) FileOps::Move(oldZip, ZipPath(*mod));
    if (FileOps::FileExists(oldOff)) FileOps::Move(oldOff, ZipOffPath(*mod));

    Save();
}

// ── Internal helpers ────────────────────────────────────────────────────────

std::wstring ModService::FilesRoot(const Models::Mod& mod, const std::wstring& paks) {
    return mod.target.empty() ? paks : LooseFiles::TargetDir(paks, mod.target);
}

std::vector<std::wstring> ModService::ExtractZip(const Models::Mod& mod, const std::wstring& zipPath, const std::wstring& paks, const Progress& progress) {
    std::vector<std::wstring> extracted;
    Zip::Failure failure;
    const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(zipPath, &failure);
    if (!archive) throw ModError(FileOps::ZipMessage(failure, zipPath));

    // Loose game files go under their target folder, the archive's own folder around them left out
    const bool loose = !mod.target.empty();
    const std::wstring root = FilesRoot(mod, paks);
    const auto relativeName = [&](const Zip::Entry& entry) -> std::optional<std::wstring> {
        if (loose && LooseFiles::IsExtra(entry.name)) return std::nullopt;   // the readme stays in the ZIP
        if (!loose || mod.targetStrip.empty()) return entry.fullName;
        std::wstring name = entry.fullName;
        for (auto& c : name)
            if (c == L'\\') c = L'/';
        if (!Text::StartsWith(name, mod.targetStrip, true)) return std::nullopt;   // outside the mod's folder
        return name.substr(mod.targetStrip.size());
    };

    // Every entry has to land inside its folder ("..\", "C:\..." or "/..." would write elsewhere):
    // otherwise the whole archive is refused before anything is written
    uint64_t total = 0, before = 0;
    for (const auto& entry : archive->Entries()) {
        total += entry.length;
        const auto name = relativeName(entry);
        if (!entry.name.empty() && name && !InsideFolder(Files::DotNetCombine(root, *name), root))
            throw ModError(Loc::Fmt("error_zip_unsafe_path", entry.fullName));
    }

    // The files this extraction creates (not the ones it overwrites): removed again if it fails; the
    // game files it replaced get their originals back
    std::vector<std::wstring> created, replaced;
    try {
        for (const auto& entry : archive->Entries()) {
            if (entry.name.empty()) continue;   // folder entries
            const auto name = relativeName(entry);
            if (!name) continue;

            const std::wstring destPath = Files::DotNetCombine(root, *name);
            const std::wstring destDir = Files::DirectoryName(destPath);
            if (!destDir.empty()) FileOps::MakeDirectory(destDir);

            if (!FileOps::FileExists(destPath)) {
                created.push_back(destPath);
            } else if (loose) {
                if (!LooseFiles::KeepOriginal(paks, destPath)) throw ModError(Loc::Fmt("error_keep_original", destPath));
                replaced.push_back(destPath);
            }
            const bool ok = archive->ExtractToFile(entry, destPath, &failure, [&](uint64_t written) {
                if (progress && total > 0) progress(static_cast<float>(static_cast<double>(before + written) / static_cast<double>(total)));
            });
            if (!ok) throw ModError(FileOps::ZipMessage(failure, destPath));
            before += entry.length;
            extracted.push_back(*name);
        }
    } catch (const ModError&) {
        for (const auto& path : created) DeleteFileW(path.c_str());
        for (const auto& path : replaced) LooseFiles::RestoreOriginal(paks, path);
        throw;
    }
    return extracted;
}

void ModService::DeleteExtractedFiles(const Models::Mod& mod) const {
    // The files of the other enabled mods stay: two mods with the same files (variants of one
    // skin) no longer break each other
    std::unordered_set<std::wstring> inUse;
    for (const auto& other : mods) {
        if (other.id == mod.id || !other.isEnabled) continue;
        for (const auto& file : other.files) inUse.insert(Resolved(Files::DotNetCombine(FilesRoot(other, paksDir), file)));
    }
    for (const auto& file : mod.files) {
        const std::wstring path = Files::DotNetCombine(FilesRoot(mod, paksDir), file);
        if (inUse.count(Resolved(path))) continue;
        // A replaced game file gets its original back; a new one goes
        if (!mod.target.empty() && LooseFiles::RestoreOriginal(paksDir, path)) continue;
        if (FileOps::FileExists(path)) FileOps::Delete(path);
    }
}

} // namespace RivalsForge::Services::Mods

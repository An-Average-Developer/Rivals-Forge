#pragma once
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Core/Zip.h"
#include "Models/Mod.h"
#include "Models/Section.h"

namespace RivalsForge::Services::Mods {

// An operation failed. Message() is what the C# exception's Message said (IOException,
// FileNotFoundException, InvalidOperationException, InvalidDataException...), in the user's language.
class ModError : public std::exception {
public:
    explicit ModError(std::wstring text) : message(std::move(text)) {}
    const std::wstring& Message() const { return message; }
    const char* what() const noexcept override { return "ModError"; }

private:
    std::wstring message;
};

// Progress of a long file operation (0..1); called on the worker thread
using Progress = std::function<void(float)>;

// Services\ModService.cs: the mods (data.json), the game's Paks folder and the mod ZIPs kept in MODS.
// Every method behaves as the C# one did, file for file; failures throw ModError. The window groups
// the mods by hero (Mod::character) instead of the C#'s sections: the sections are still read and
// written, untouched, so the C# app keeps them. Split:
//   ModService.cpp    constructor, game path, lookups, the mods' heroes
//   ModStore.cpp      Load / Save of data.json, the Roaming and mods.json migrations
//   ModInstaller.cpp  AddMod, EnableMod, DisableMod, DeleteMod, RenameMod, ExtractZip
//   ModNames.cpp      SanitizeFileName, UniqueModName, UniqueZipBaseName, the ZIP paths
// Not thread-safe: the methods run on the UI thread. AddMod and EnableMod also come in three steps
// (Prepare on the UI thread, the copy / extraction on a worker, Commit on the UI thread) so a big
// ZIP doesn't freeze the window; the steps do exactly what the one call does.
class ModService {
public:
    // The constructor: creates the data and MODS folders, migrates from Roaming, loads data.json
    // and, when no game path is saved yet, auto-detects it (and saves)
    ModService();

    // ── Game path ──
    // Full path to MarvelRivals\MarvelGame\Marvel\Content\Paks ("" = not configured)
    const std::wstring& PaksDir() const { return paksDir; }
    // True when the Paks directory has been configured
    bool IsConfigured() const { return !paksDir.empty(); }
    // data.json couldn't be read at startup: the copy kept of it ("" when it loaded fine)
    const std::wstring& LoadBackup() const { return loadBackup; }
    void SetGamePath(const std::wstring& paks);
    // Local folder where the ZIP archives are kept (independent of the game path)
    static std::wstring ModsStorageDir();

    // ── Another installation (Steam / Epic) ──
    // CopyMods extracts the given (enabled) mods into `to` as well, leaving them where they are (a
    // worker thread). SwitchTo makes `to` the game path and each mod enabled exactly when its files
    // are in that installation's Paks (its ZIP .zip / .zip.off to match): what was copied, or was
    // already there, shows enabled; the rest disabled. The other installation isn't touched.
    struct CopyResult {
        std::vector<Models::Mod> copied;
        std::vector<std::pair<std::wstring, std::wstring>> failed;   // mod id, message
    };
    static CopyResult CopyMods(const std::vector<Models::Mod>& mods, const std::wstring& to, const Progress& progress);
    void SwitchTo(const std::wstring& to, const CopyResult& copy = {});

    // ── State ──
    const std::vector<Models::Section>& Sections() const { return sections; }
    const std::vector<Models::Mod>& Mods() const { return mods; }
    const Models::Mod* FindMod(const std::wstring& id) const;

    // ── Heroes ──
    // The hero a mod is for ("" = none)
    void SetModCharacter(const std::wstring& modId, const std::wstring& character);
    // Looks in the mod's ZIP (.zip or .zip.off) for its hero: the id, or "" when none is found. Any thread.
    static std::wstring DetectCharacter(const Models::Mod& mod);
    // What kinds of mod it is (ModTypeDetector::Type bits, 0 = can't tell), from its ZIP. Any thread.
    static unsigned DetectTypes(const Models::Mod& mod);
    // The heroes found for the mods that hadn't been looked at, saved in one go
    void ApplyCharacters(const std::vector<std::pair<std::wstring, std::wstring>>& found);

    // ── Mods ──
    // Copies the ZIP into MODS under a readable unique name, extracts it to Paks, finds its hero
    // (unless `character` says it) and saves
    const Models::Mod& AddMod(const std::wstring& sourceZipPath, const std::wstring& name, const std::optional<std::wstring>& character);
    Models::Mod PrepareAddMod(const std::wstring& name, const std::optional<std::wstring>& character);
    static void InstallMod(Models::Mod& mod, const std::wstring& sourceZipPath, const std::wstring& paks, const Progress& progress = {});
    // The file name the ZIP of a new mod called `name` would get in MODS, the mods named in
    // `pending` (not added yet) counted as added
    std::wstring ZipNameFor(const std::wstring& name, const std::vector<std::wstring>& pending = {}) const;
    const Models::Mod& CommitAddMod(Models::Mod mod);

    // .zip.off -> .zip, extracted again. Prepare returns nullopt when the mod is already enabled.
    void EnableMod(const std::wstring& modId);
    std::optional<Models::Mod> PrepareEnableMod(const std::wstring& modId);
    static void RestoreMod(Models::Mod& mod, const std::wstring& paks, const Progress& progress = {});
    void CommitEnableMod(const Models::Mod& mod);

    // Its files removed from Paks (except those another enabled mod has too), .zip -> .zip.off
    void DisableMod(const std::wstring& modId);
    // Its files removed from Paks (the same way), the .zip / .zip.off deleted
    void DeleteMod(const std::wstring& modId);
    // Out of the list only: its ZIP in MODS and its files in Paks stay as they are
    void ForgetMod(const std::wstring& modId);
    // The ZIP is renamed to match
    void RenameMod(const std::wstring& modId, const std::wstring& newName);

private:
    Models::Mod* EditMod(const std::wstring& id);
    void EnsureConfigured() const;

    // ModStore.cpp
    static void MigrateFromRoaming();
    void Load();
    void Save();

    // ModNames.cpp
    static std::wstring ZipBaseName(const Models::Mod& mod);
    static std::wstring ZipPath(const Models::Mod& mod);
    static std::wstring ZipOffPath(const Models::Mod& mod);
    static std::wstring SanitizeFileName(const std::wstring& name);
    std::wstring UniqueModName(const std::wstring& name) const;
    static std::wstring UniqueZipBaseName(const std::wstring& modName);

    // ModInstaller.cpp
    // Into Paks, or (a mod of loose game files, Mod::target) over the game's own files, kept aside first
    static std::vector<std::wstring> ExtractZip(const Models::Mod& mod, const std::wstring& zipPath, const std::wstring& paks, const Progress& progress);
    // Where the mod's files are: Paks, or its target folder under Content
    static std::wstring FilesRoot(const Models::Mod& mod, const std::wstring& paks);
    void DeleteExtractedFiles(const Models::Mod& mod) const;
    // Where a test version kept a disabled mod's files: <Content>\~MRMM Disabled\<id>
    static std::wstring DisabledDir(const std::wstring& paks, const Models::Mod& mod);

    std::wstring paksDir;
    std::wstring loadBackup;
    std::vector<Models::Section> sections;
    std::vector<Models::Mod> mods;
};

// The file system calls the service makes, throwing ModError with .NET's message when they fail
// (File.Copy, File.Move, File.Delete, Directory.CreateDirectory, File.WriteAllText)
namespace FileOps {
bool FileExists(const std::wstring& path);   // File.Exists
void MakeDirectory(const std::wstring& path);
void Copy(const std::wstring& from, const std::wstring& to, const Progress& progress = {});   // overwrite: true
void Move(const std::wstring& from, const std::wstring& to);                                     // overwrite: true
void Delete(const std::wstring& path);
void WriteAllBytes(const std::wstring& path, const std::string& bytes);
// The message of a failed Win32 call on `path` (IOException / UnauthorizedAccessException texts)
std::wstring IoMessage(unsigned long error, const std::wstring& path);
// The message of a failed zip read / write on `path`
std::wstring ZipMessage(const Zip::Failure& failure, const std::wstring& path);
} // namespace FileOps

} // namespace RivalsForge::Services::Mods

#include "Services/Game/SignatureBypass.h"
#include <windows.h>
#include <memory>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Zip.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Mods/ModService.h"

namespace RivalsForge::Services::Game::SignatureBypass {

namespace {

constexpr const wchar_t* kArchive = L"Tools\\UTOC Signature Bypass Patch.zip";
constexpr const wchar_t* kLoader = L"dsound.dll";
constexpr const wchar_t* kPlugin = L"plugins\\MarvelRivalsUTOCSignatureBypass.asi";

// IsInstalled is asked every frame by the header: the disk is looked at every couple of seconds
struct Cached {
    std::wstring dir;
    ULONGLONG at = 0;
    bool installed = false;
};
Cached cache;

} // namespace

std::wstring BinariesDir(const GamePath::Installation& installation) {
    return Files::Combine(installation.root, L"MarvelGame\\Marvel\\Binaries\\Win64");
}

bool IsInstalled(const GamePath::Installation& installation) {
    const std::wstring dir = BinariesDir(installation);
    const ULONGLONG now = GetTickCount64();
    if (cache.dir != dir || now - cache.at > 2000) {
        cache = { dir, now, Files::Exists(Files::Combine(dir, kLoader)) && Files::Exists(Files::Combine(dir, kPlugin)) };
    }
    return cache.installed;
}

void Install(const GamePath::Installation& installation) {
    using Mods::ModError;
    namespace FileOps = Mods::FileOps;
    const std::wstring dir = BinariesDir(installation);
    if (!Files::DirectoryExists(dir)) throw ModError(Loc::Fmt("error_io_path_not_found", dir));
    const std::wstring archivePath = Paths::Asset(kArchive);
    Zip::Failure failure;
    const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(archivePath, &failure);
    if (!archive) throw ModError(FileOps::ZipMessage(failure, archivePath));
    for (const auto& entry : archive->Entries()) {
        if (entry.name.empty()) continue;
        const std::wstring dest = Files::DotNetCombine(dir, entry.fullName);
        FileOps::MakeDirectory(Files::DirectoryName(dest));
        if (!archive->ExtractToFile(entry, dest, &failure)) throw ModError(FileOps::ZipMessage(failure, dest));
    }
    Log::Info(L"UTOC Signature Bypass Patch installed in " + dir);
    cache = {};   // looked at again on the next check
}

} // namespace RivalsForge::Services::Game::SignatureBypass

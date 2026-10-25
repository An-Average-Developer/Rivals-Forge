#include "Services/Mods/ModPackage.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <memory>
#include <set>
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Core/ZipWriter.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"

namespace RivalsForge::Services::Mods::ModPackage {

namespace {

constexpr int kMaxDepth = 4;   // ZIPs inside ZIPs inside ZIPs...
constexpr const wchar_t* kTempPrefix = L"RF-";   // %TEMP%\RF-<guid> (MRMM-<guid> before the rename)

bool IsModFile(const std::wstring& name) {
    const std::wstring ext = Text::Lower(Files::Extension(name));
    return ext == L".pak" || ext == L".ucas" || ext == L".utoc" || ext == L".sig";
}


// "Folder/Sub/file.pak" -> "Folder/Sub" ("" at the root)
std::wstring EntryDir(const std::wstring& fullName) {
    const size_t slash = fullName.find_last_of(L"/\\");
    return slash == std::wstring::npos ? L"" : fullName.substr(0, slash);
}

std::wstring LastSegment(const std::wstring& dir) {
    const size_t slash = dir.find_last_of(L"/\\");
    return slash == std::wstring::npos ? dir : dir.substr(slash + 1);
}

std::wstring SafeFileName(std::wstring name) {
    for (auto& c : name)
        if (c < 32 || wcschr(L"\"<>|:*?\\/", c)) c = L'_';
    name = Text::TrimDotNet(name);
    return name.empty() ? L"mod" : name;
}

// "z_Y2KMantisGreen_99999999_P" -> "Y2KMantisGreen": the load-order prefix, the priority number and
// the "_P" patch suffix aren't part of the name
std::wstring NameFromStem(std::wstring stem) {
    if (Text::StartsWith(stem, L"z_", true)) stem.erase(0, 2);
    while (!stem.empty() && (stem.front() == L'~' || stem.front() == L'_')) stem.erase(0, 1);
    if (Text::EndsWith(stem, L"_P", true)) stem.resize(stem.size() - 2);
    for (;;) {
        const size_t under = stem.find_last_of(L'_');
        if (under == std::wstring::npos || under == 0 || under + 1 == stem.size()) break;
        const std::wstring tail = stem.substr(under + 1);
        if (!std::all_of(tail.begin(), tail.end(), [](wchar_t c) { return std::iswdigit(c); })) break;
        stem.resize(under);
    }
    return stem.empty() ? L"mod" : stem;
}

struct Analysis {
    std::wstring tempDir;
    int folders = 0;   // the temporary subfolders handed out so far
    std::vector<Part> parts;
    uint64_t total = 0, done = 0;
    Progress progress;
};

// Whether the ZIP is a normal mod ZIP: the files of one mod at its root, no ZIPs inside
bool IsPlain(const Zip::Reader& archive) {
    std::set<std::wstring> stems;
    bool any = false;
    for (const auto& e : archive.Entries()) {
        if (e.name.empty()) continue;
        if (IsZip(e.name)) return false;
        if (!IsModFile(e.name)) continue;
        any = true;
        if (!EntryDir(e.fullName).empty()) return false;
        stems.insert(Text::Lower(Files::FileNameWithoutExtension(e.name)));
    }
    return !any || stems.size() == 1;
}

// Extracts the mod files of `zipPath` (and of the ZIPs inside it) into parts. `container` is the
// name of the inner ZIP being read ("" for the one the user chose).
void Collect(Analysis& a, const std::wstring& zipPath, const std::wstring& container, int depth) {
    Zip::Failure failure;
    const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(zipPath, &failure);
    if (!archive) throw ModError(FileOps::ZipMessage(failure, zipPath));

    // The groups of this archive: a folder and a shared file name. Each part gets a temporary
    // folder of its own (two mods may have files of the same name)
    struct Group {
        size_t part;
        std::wstring archiveDir, tempDir;
    };
    std::map<std::wstring, Group> groups;   // "folder|stem" (lower case)
    std::vector<const Group*> order;
    for (const auto& e : archive->Entries()) {
        if (e.name.empty()) continue;
        if (IsZip(e.name)) {
            if (depth >= kMaxDepth) continue;
            const std::wstring inner = Files::Combine(a.tempDir, L"inner" + std::to_wstring(a.folders++) + L".zip");
            if (!archive->ExtractToFile(e, inner, &failure)) throw ModError(FileOps::ZipMessage(failure, inner));
            Collect(a, inner, Files::FileNameWithoutExtension(e.name), depth + 1);
            DeleteFileW(inner.c_str());
            continue;
        }
        if (!IsModFile(e.name)) continue;   // readme, pictures...
        const std::wstring dir = EntryDir(e.fullName), stem = Files::FileNameWithoutExtension(e.name);
        auto it = groups.find(Text::Lower(dir + L"|" + stem));
        if (it == groups.end()) {
            Group group{ a.parts.size(), dir, Files::Combine(a.tempDir, std::to_wstring(a.folders++)) };
            if (!Files::CreateDirectories(group.tempDir)) throw ModError(FileOps::IoMessage(GetLastError(), group.tempDir));
            Part part;
            part.stem = stem;
            a.parts.push_back(std::move(part));
            it = groups.emplace(Text::Lower(dir + L"|" + stem), std::move(group)).first;
            order.push_back(&it->second);
        }
        Part& part = a.parts[it->second.part];
        const std::wstring dest = Files::Combine(it->second.tempDir, SafeFileName(e.name));
        if (std::find(part.files.begin(), part.files.end(), dest) != part.files.end()) continue;   // the same name twice
        const uint64_t before = a.done;
        if (!archive->ExtractToFile(e, dest, &failure, [&](uint64_t written) {
                if (a.progress && a.total > 0)
                    a.progress(std::min(1.f, static_cast<float>(static_cast<double>(before + written) / static_cast<double>(a.total))));
            }))
            throw ModError(FileOps::ZipMessage(failure, dest));
        a.done += e.length;
        part.files.push_back(dest);
    }

    // Names: the folder they were in, else the inner ZIP's name when it held one mod, else the files'
    for (const Group* group : order) {
        Part& part = a.parts[group->part];
        const std::wstring folder = LastSegment(group->archiveDir);
        part.name = !folder.empty() ? folder : !container.empty() && order.size() == 1 ? container : NameFromStem(part.stem);
    }
}

} // namespace

bool IsZip(const std::wstring& path) { return Text::EndsWith(path, L".zip", true); }

Converted ToZip(const std::wstring& archive, const Progress& progress) {
    Converted converted;
    converted.folder = Files::Combine(Files::TempDirectory(), kTempPrefix + System::NewGuid());
    const std::wstring extracted = Files::Combine(converted.folder, L"files");
    if (!Files::CreateDirectories(extracted)) throw ModError(FileOps::IoMessage(GetLastError(), extracted));
    try {
        wchar_t system[MAX_PATH]{};
        GetSystemDirectoryW(system, MAX_PATH);
        const std::wstring tar = Files::Combine(system, L"tar.exe");
        if (!Files::Exists(tar)) throw ModError(Loc::Fmt("error_archive_format", Files::FileName(archive)));
        std::string output;
        unsigned long code = 1;
        if (progress) progress(0.f);
        const bool ran = Process::RunAndCapture(tar, L"-xf " + Process::QuoteArgument(archive) + L" -C " + Process::QuoteArgument(extracted), output,
                                                &code, 10 * 60 * 1000);
        if (!ran || code != 0) {
            Log::Warning(L"tar could not extract " + archive + L": " + Text::Widen(Text::Trim(output)));
            throw ModError(Loc::Fmt("error_archive_extract", Files::FileName(archive)));
        }
        if (progress) progress(0.5f);
        converted.zip = Files::Combine(converted.folder, Files::FileNameWithoutExtension(archive) + L".zip");
        Zip::Failure failure;
        const std::unique_ptr<Zip::Writer> zip = Zip::Writer::Create(converted.zip, &failure);
        if (!zip) throw ModError(FileOps::ZipMessage(failure, converted.zip));
        for (const auto& file : Files::ListFiles(extracted, L"*", true)) {
            std::wstring name = Files::RelativePath(extracted, file);
            for (auto& c : name)
                if (c == L'\\') c = L'/';
            if (!zip->AddFile(name, file, &failure)) throw ModError(FileOps::ZipMessage(failure, converted.zip));
        }
        if (!zip->Finish(&failure)) throw ModError(FileOps::ZipMessage(failure, converted.zip));
        Files::DeleteDirectory(extracted);
        if (progress) progress(1.f);
    } catch (const ModError&) {
        Files::DeleteDirectory(converted.folder);
        throw;
    }
    return converted;
}

Plan Analyze(const std::wstring& zipPath, const Progress& progress) {
    Plan plan;
    plan.source = zipPath;
    {
        Zip::Failure failure;
        const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(zipPath, &failure);
        if (!archive) throw ModError(FileOps::ZipMessage(failure, zipPath));
        if (IsPlain(*archive)) return plan;
    }

    Analysis a;
    a.tempDir = Files::Combine(Files::TempDirectory(), kTempPrefix + System::NewGuid());
    a.progress = progress;
    if (!Files::CreateDirectories(a.tempDir)) throw ModError(FileOps::IoMessage(GetLastError(), a.tempDir));
    a.total = Files::Size(zipPath).value_or(0);
    try {
        Collect(a, zipPath, L"", 0);
    } catch (const ModError&) {
        Files::DeleteDirectory(a.tempDir);
        throw;
    }
    std::erase_if(a.parts, [](const Part& p) { return p.files.empty(); });
    if (a.parts.empty()) {
        // No mod files anywhere: installed as it is, the way it always was
        Files::DeleteDirectory(a.tempDir);
        return plan;
    }
    // Two parts with the same suggested name get their files' names instead (or a number)
    for (size_t i = 0; i < a.parts.size(); ++i)
        for (size_t j = i + 1; j < a.parts.size(); ++j)
            if (Text::Equals(a.parts[i].name, a.parts[j].name, true)) a.parts[j].name = NameFromStem(a.parts[j].stem);
    for (size_t i = 0; i < a.parts.size(); ++i)
        for (size_t j = i + 1, n = 2; j < a.parts.size(); ++j)
            if (Text::Equals(a.parts[i].name, a.parts[j].name, true)) a.parts[j].name += L" " + std::to_wstring(n++);

    plan.split = true;
    plan.tempDir = a.tempDir;
    plan.parts = std::move(a.parts);
    return plan;
}

Built Build(const Plan& plan, const std::vector<std::wstring>& names, const Progress& progress) {
    // The new ZIPs, in a temporary folder of their own
    Built built;
    built.folder = Files::Combine(Files::TempDirectory(), kTempPrefix + System::NewGuid());
    if (!Files::CreateDirectories(built.folder)) {
        const DWORD error = GetLastError();
        Discard(plan);
        throw ModError(FileOps::IoMessage(error, built.folder));
    }
    for (size_t i = 0; i < plan.parts.size(); ++i) {
        const std::wstring base = SafeFileName(i < names.size() ? names[i] : plan.parts[i].name);
        std::wstring candidate = Files::Combine(built.folder, base + L".zip");
        for (int n = 2; Files::Exists(candidate) || std::find(built.paths.begin(), built.paths.end(), candidate) != built.paths.end(); ++n)
            candidate = Files::Combine(built.folder, base + L" (" + std::to_wstring(n) + L").zip");
        built.paths.push_back(candidate);
    }

    uint64_t total = 0, done = 0;
    for (const auto& part : plan.parts)
        for (const auto& file : part.files) total += Files::Size(file).value_or(0);
    try {
        for (size_t i = 0; i < plan.parts.size(); ++i) {
            Zip::Failure failure;
            const std::unique_ptr<Zip::Writer> zip = Zip::Writer::Create(built.paths[i], &failure);
            if (!zip) throw ModError(FileOps::ZipMessage(failure, built.paths[i]));
            for (const auto& file : plan.parts[i].files) {
                const uint64_t before = done;
                if (!zip->AddFile(Files::FileName(file), file, &failure, [&](uint64_t copied) {
                        if (progress && total > 0) progress(static_cast<float>(static_cast<double>(before + copied) / static_cast<double>(total)));
                    }))
                    throw ModError(FileOps::ZipMessage(failure, failure.error == Zip::Error::OpenFailed ? file : built.paths[i]));
                done += Files::Size(file).value_or(0);
            }
            if (!zip->Finish(&failure)) throw ModError(FileOps::ZipMessage(failure, built.paths[i]));
        }
    } catch (const ModError&) {
        Files::DeleteDirectory(built.folder);
        Discard(plan);
        throw;
    }
    Discard(plan);
    return built;
}

void Recycle(const std::wstring& path) {
    std::wstring from = path;
    from.push_back(L'\0');   // SHFileOperation takes a double-null-terminated list
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    SHFileOperationW(&op);
}

void Discard(const Plan& plan) {
    if (!plan.tempDir.empty()) Files::DeleteDirectory(plan.tempDir);
}

void RemoveLeftovers() {
    // Only the ones an hour old: another window might be splitting a ZIP right now
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const int64_t hour = 36'000'000'000;
    const int64_t nowTicks = static_cast<int64_t>(ULARGE_INTEGER{ { now.dwLowDateTime, now.dwHighDateTime } }.QuadPart);
    for (const auto& dir : Files::ListDirectories(Files::TempDirectory())) {
        const std::wstring name = Files::FileName(dir);
        if (!Text::StartsWith(name, kTempPrefix) && !Text::StartsWith(name, L"MRMM-")) continue;
        const auto written = Files::LastWriteTime(dir);
        if (written && nowTicks - *written > hour) Files::DeleteDirectory(dir);
    }
}

} // namespace RivalsForge::Services::Mods::ModPackage

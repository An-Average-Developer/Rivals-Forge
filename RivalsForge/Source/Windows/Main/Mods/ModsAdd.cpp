#include <algorithm>
#include <memory>
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Game/SignatureBypass.h"
#include "Services/Mods/ModPackage.h"
#include "Windows/Main/Overlays/BypassOverlay.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsView.h"
#include "Windows/Main/Overlays/AddModOverlay.h"

// AddMod_Click / DoAddMod, in steps, the slow ones on a worker thread with the progress in the
// status bar:
//   1. every ZIP chosen is looked at (ModPackage::Analyze): a normal one is one mod; one with its
//      mod in a folder, or with several mods, is split into one clean ZIP per mod
//   2. the name of every mod is asked, one card after the other ("Mod 3 of 13"; Skip takes the
//      suggested name, Skip all for the rest too, Cancel stops everything)
//   3. the ZIPs to split are split (into a temporary folder)
//   4. the mods are installed one after the other (their ZIP written into MODS, extracted to Paks)
//   5. the original ZIPs whose mods all went in go to the Recycle Bin (a download from the store is
//      deleted instead); the temporary files go
// A .7z / .rar is turned into a ZIP in step 1 and then goes the same way.
namespace RivalsForge::Main {

namespace {
using Services::Mods::ModError;
using Services::Mods::ModService;
namespace ModPackage = Services::Mods::ModPackage;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
} // namespace

struct AddJob {
    struct Source {                     // an archive chosen
        std::wstring path;
        std::wstring zip;               // the ZIP read: the archive itself, or a .7z / .rar converted
        std::wstring convertedFolder;   // the converted one's temporary folder
        ModPackage::Plan plan;          // split: its parts; otherwise the ZIP itself is the mod
        std::vector<AddModOverlay::File> entries;   // not split: its files
        ModPackage::Built built;        // split: the new ZIPs
        bool failed = false;            // the original stays where it is
    };
    struct Mod {                        // a mod to install
        size_t source = 0;
        int part = -1;                  // -1: the source ZIP itself
        std::wstring name;              // the one chosen
        std::wstring zip;               // the ZIP installed (once the split ones are built)
        bool installed = false;
    };
    std::vector<Source> sources;
    std::vector<Mod> mods;
    size_t index = 0;                   // the step being done
    bool skipAll = false;
    int succeeded = 0;
    std::vector<std::wstring> errors;   // "• name: message"
    std::wstring statusBefore;          // a single failed install leaves the status bar as it was
    AddOptions options;
    bool Single() const { return mods.size() == 1; }
};

// ── 1. Look at every ZIP ────────────────────────────────────────────────────

void ModsView::AddMod() {
    if (Busy()) return;
    // OpenFileDialog { Title = "Select Mod ZIP(s)", Filter = "ZIP archives (*.zip)|*.zip", Multiselect = true }
    std::vector<std::wstring> files =
        System::PickFiles(state.Owner(), Loc::Get("addmod_pick_title"), { { Loc::Get("addmod_filter"), L"*.zip;*.7z;*.rar" } });
    if (files.empty()) return;
    AddFiles(std::move(files));
}

void ModsView::AddFiles(std::vector<std::wstring> files, const AddOptions& options) {
    if (Busy() || files.empty()) return;
    // Installing writes to Paks: not while the game has it open (a download given up is removed)
    WhenGameClosed([this, files, options] { StartAdd(files, options); }, [options] {
        if (options.downloaded && !options.folder.empty()) Files::DeleteDirectory(options.folder);
    });
}

void ModsView::StartAdd(const std::vector<std::wstring>& files, const AddOptions& options) {
    if (Busy()) return;
    auto job = std::make_shared<AddJob>();
    job->options = options;
    for (const auto& file : files)
        if (std::none_of(job->sources.begin(), job->sources.end(), [&](const AddJob::Source& s) { return Text::Equals(s.path, file, true); }))
            job->sources.push_back({ file });
    job->statusBefore = state.statusMessage;
    state.busy = true;
    AnalyzeNext(job);
}

void ModsView::AnalyzeNext(std::shared_ptr<AddJob> job) {
    if (job->index >= job->sources.size()) {
        // Every mod found, in order: the name of each is asked next
        for (size_t s = 0; s < job->sources.size(); ++s) {
            const AddJob::Source& source = job->sources[s];
            if (source.failed) continue;
            if (!source.plan.split) job->mods.push_back({ s, -1, L"", source.zip });
            else
                for (size_t p = 0; p < source.plan.parts.size(); ++p) job->mods.push_back({ s, static_cast<int>(p) });
        }
        job->index = 0;
        AskName(job);
        return;
    }
    AddJob::Source& source = job->sources[job->index];
    state.progress = static_cast<float>(job->index) / static_cast<float>(job->sources.size());
    state.SetStatus(Loc::Fmt("status_analyzing", Files::FileName(source.path)));

    struct Analyzed {
        ModPackage::Converted converted;
        ModPackage::Plan plan;
        std::vector<AddModOverlay::File> entries;
        std::optional<std::wstring> error;
    };
    Async::Run(
        [path = source.path]() -> Analyzed {
            Analyzed result;
            try {
                // A .7z / .rar becomes a ZIP first
                if (!ModPackage::IsZip(path)) result.converted = ModPackage::ToZip(path);
                const std::wstring zip = result.converted.zip.empty() ? path : result.converted.zip;
                result.plan = ModPackage::Analyze(zip);
                if (!result.plan.split) {
                    // The files the card lists
                    Zip::Failure failure;
                    if (const auto archive = Zip::Reader::Open(zip, &failure))
                        for (const auto& e : archive->Entries())
                            if (!e.name.empty()) result.entries.push_back({ e.name, e.length });
                }
            } catch (const ModError& e) {
                if (!result.converted.folder.empty()) Files::DeleteDirectory(result.converted.folder);
                result.converted = {};
                result.error = e.Message();
            }
            return result;
        },
        [this, job](Analyzed result) {
            AddJob::Source& source = job->sources[job->index];
            source.zip = result.converted.zip.empty() ? source.path : result.converted.zip;
            source.convertedFolder = result.converted.folder;
            if (result.error) {
                Log::Warning(L"Looking at " + source.path + L": " + *result.error);
                job->errors.push_back(Loc::Fmt("addmod_error_line", Files::FileName(source.path), *result.error));
                source.failed = true;
            } else {
                source.plan = std::move(result.plan);
                source.entries = std::move(result.entries);
            }
            ++job->index;
            AnalyzeNext(job);
        });
}

// ── 2. The name of every mod ────────────────────────────────────────────────

void ModsView::AskName(std::shared_ptr<AddJob> job) {
    if (job->index >= job->mods.size()) {
        job->index = 0;
        BuildNext(job);
        return;
    }
    AddJob::Mod& mod = job->mods[job->index];
    const AddJob::Source& source = job->sources[mod.source];
    AddModOverlay::Request request;
    request.zipPath = source.path;
    request.index = static_cast<int>(job->index);
    request.count = static_cast<int>(job->mods.size());
    if (mod.part < 0) {
        request.kind = AddModOverlay::Kind::Plain;
        request.files = source.entries;
        // A download from the store: the mod's name on Nexus (once: a ZIP split into several keeps theirs)
        request.defaultName = !job->options.name.empty() ? job->options.name : Files::FileNameWithoutExtension(source.path);
    } else {
        const ModPackage::Part& part = source.plan.parts[static_cast<size_t>(mod.part)];
        request.kind = source.plan.parts.size() == 1 ? AddModOverlay::Kind::Tidy : AddModOverlay::Kind::Split;
        request.part = mod.part;
        request.parts = static_cast<int>(source.plan.parts.size());
        for (const auto& file : part.files) request.files.push_back({ Files::FileName(file), Files::Size(file).value_or(0) });
        request.defaultName = part.name;
    }
    if (job->skipAll) {
        mod.name = request.defaultName;
        ++job->index;
        AskName(job);
        return;
    }
    // The mods named before count as added: they will be by the time this one is
    std::vector<std::wstring> pending;
    for (size_t i = 0; i < job->index; ++i) pending.push_back(job->mods[i].name);
    request.zipName = [this, pending](const std::wstring& name) { return state.Service().ZipNameFor(name, pending); };
    request.modsFolder = ModService::ModsStorageDir();
    request.downloaded = job->options.downloaded;
    state.progress = 0.f;
    state.SetStatus(Loc::Fmt("status_naming", job->index + 1, job->mods.size()));

    main.AddModCard().Show(std::move(request), [this, job](AddModOverlay::Answer answer) {
        using Action = AddModOverlay::Answer::Action;
        if (answer.action == Action::Cancel) {
            // Nothing is installed and nothing is deleted
            for (auto& source : job->sources) ModPackage::Discard(source.plan);
            job->mods.clear();
            job->errors.clear();
            FinishAdd(job);
            return;
        }
        job->mods[job->index].name = answer.name;
        job->skipAll = answer.action == Action::SkipAll;
        ++job->index;
        AskName(job);
    });
}

// ── 3. Split the ZIPs that hold their mods differently ──────────────────────

void ModsView::BuildNext(std::shared_ptr<AddJob> job) {
    while (job->index < job->sources.size() && (job->sources[job->index].failed || !job->sources[job->index].plan.split)) ++job->index;
    if (job->index >= job->sources.size()) {
        job->index = 0;
        InstallNext(job);
        return;
    }
    const size_t s = job->index;
    std::vector<std::wstring> names;
    for (const auto& mod : job->mods)
        if (mod.source == s) names.push_back(mod.name);
    const std::wstring fileName = Files::FileName(job->sources[s].path);
    state.progress = 0.f;
    state.SetStatus(Loc::Fmt("status_splitting", fileName));
    struct Built {
        ModPackage::Built built;
        std::optional<std::wstring> error;
    };
    Async::Run(
        [plan = job->sources[s].plan, names, progress = ProgressTo(0.f, 1.f)]() -> Built {
            try {
                return { ModPackage::Build(plan, names, progress), std::nullopt };
            } catch (const ModError& e) {
                return { {}, e.Message() };
            }
        },
        [this, job, s, fileName](Built result) {
            AddJob::Source& source = job->sources[s];
            if (result.error) {
                Log::Warning(L"Splitting " + fileName + L": " + *result.error);
                job->errors.push_back(Loc::Fmt("addmod_error_line", fileName, *result.error));
                source.failed = true;
            } else {
                Log::Info(L"Split " + fileName + L" into " + std::to_wstring(result.built.paths.size()) + L" ZIP(s)");
                source.built = std::move(result.built);
                for (auto& mod : job->mods)
                    if (mod.source == s) mod.zip = source.built.paths[static_cast<size_t>(mod.part)];
            }
            ++job->index;
            BuildNext(job);
        });
}

// ── 4. Install them ─────────────────────────────────────────────────────────

void ModsView::InstallNext(std::shared_ptr<AddJob> job) {
    while (job->index < job->mods.size() && job->mods[job->index].zip.empty()) ++job->index;   // its ZIP couldn't be split
    if (job->index >= job->mods.size()) {
        FinishAdd(job);
        return;
    }
    const AddJob::Mod& item = job->mods[job->index];
    const std::wstring zipPath = item.zip, name = item.name;
    const float share = 1.f / static_cast<float>(job->mods.size());
    state.progress = share * static_cast<float>(job->index);
    state.SetStatus(job->Single() ? Loc::Fmt("addmod_installing", name)
                                  : Loc::Fmt("addmod_installing_many", job->index + 1, job->mods.size(), name));

    const auto failed = [this, job, name](const std::wstring& message) {
        Log::Warning(L"Installing " + name + L": " + message);
        job->sources[job->mods[job->index].source].failed = true;   // its original stays
        job->errors.push_back(Loc::Fmt("addmod_error_line", name, message));   // "• {name}: {ex.Message}"
        ++job->index;
        Async::Post([this, job] { InstallNext(job); });
    };

    Models::Mod mod;
    try {
        mod = state.Service().PrepareAddMod(name, std::nullopt);
    } catch (const ModError& e) {
        failed(e.Message());
        return;
    }
    mod.nexusId = job->options.nexusId;   // the store marks it installed (and sees updates)
    mod.nexusVersion = job->options.nexusVersion;
    mod.gameBananaId = job->options.gameBananaId;
    mod.gameBananaFile = job->options.gameBananaFile;
    struct Result {
        Models::Mod mod;
        std::optional<std::wstring> error;
    };
    const std::wstring paks = state.Service().PaksDir();
    Async::Run(
        [mod, zipPath, paks, progress = ProgressTo(state.progress, share)]() mutable -> Result {
            try {
                ModService::InstallMod(mod, zipPath, paks, progress);
                return { std::move(mod), std::nullopt };
            } catch (const ModError& e) {
                return { std::move(mod), e.Message() };
            }
        },
        [this, job, name, failed](Result result) {
            if (result.error) {
                failed(*result.error);
                return;
            }
            try {
                // Its hero's card gets it (or "no character" when none was found)
                const Models::Mod& added = state.Service().CommitAddMod(std::move(result.mod));
                job->mods[job->index].installed = true;
                ++job->succeeded;
                if (job->Single()) {
                    const int count = static_cast<int>(added.files.size());
                    Done(Loc::Fmt(count == 1 ? "status_mod_installed_one" : "status_mod_installed_many", name, count));
                }
            } catch (const ModError& e) {
                failed(e.Message());
                return;
            }
            ++job->index;
            InstallNext(job);
        });
}

// ── 5. The originals and the summary ────────────────────────────────────────

void ModsView::FinishAdd(std::shared_ptr<AddJob> job) {
    state.busy = false;
    state.progress = 0.f;
    // The originals whose mods all went in to the Recycle Bin (never one kept in MODS itself); the
    // temporary ZIPs of the split ones go
    const std::wstring mods = Text::Lower(ModService::ModsStorageDir());
    for (size_t s = 0; s < job->sources.size(); ++s) {
        const AddJob::Source& source = job->sources[s];
        if (!source.built.folder.empty()) Files::DeleteDirectory(source.built.folder);
        if (!source.convertedFolder.empty()) Files::DeleteDirectory(source.convertedFolder);
        if (job->options.downloaded) continue;   // a download: its folder goes below
        bool all = !source.failed, any = false;
        for (const auto& mod : job->mods)
            if (mod.source == s) {
                any = true;
                all = all && mod.installed;
            }
        if (all && any && !Text::StartsWith(Text::Lower(Files::DirectoryName(source.path)), mods)) ModPackage::Recycle(source.path);
    }
    if (job->options.downloaded && !job->options.folder.empty()) Files::DeleteDirectory(job->options.folder);

    const int ok = job->succeeded, failedCount = static_cast<int>(job->errors.size());
    if (job->mods.empty() && failedCount == 0) {
        state.SetStatus(job->statusBefore);   // cancelled
    } else if (job->Single() && failedCount == 0) {
        // "Mod … installed" is already in the status bar
    } else if (job->mods.size() <= 1 && ok == 0 && failedCount == 1) {
        state.SetStatus(job->statusBefore);
        const std::wstring& line = job->errors[0];
        const size_t colon = line.find(L": ");
        ShowError("addmod_failed", colon == std::wstring::npos ? line : line.substr(colon + 2));   // "Failed to install mod:\n{ex.Message}"
    } else if (failedCount == 0) {
        Done(Loc::Fmt(ok == 1 ? "status_mods_installed_one" : "status_mods_installed_many", ok));
    } else {
        std::wstring list;
        for (const auto& line : job->errors) list += (list.empty() ? L"" : L"\n") + line;
        Dialogs::Message(Loc::Fmt("addmod_some_failed", Loc::Fmt(ok == 1 ? "addmod_installed_count_one" : "addmod_installed_count_many", ok),
                                  Loc::Fmt("addmod_failed_count", failedCount), list),
                         Loc::Get("addmod_some_failed_title"), MessageButtons::Ok, MessageIcon::Warning);
        Done(Loc::Fmt("status_batch_partial", ok, failedCount));
    }
    // Mods in, but the game won't load them without the signature patch: its card says so
    if (ok > 0 && state.IsConfigured() && !Services::Game::SignatureBypass::IsInstalled(state.Current())) main.Bypass().Show(true);
    main.InstallFinished();
}

} // namespace RivalsForge::Main

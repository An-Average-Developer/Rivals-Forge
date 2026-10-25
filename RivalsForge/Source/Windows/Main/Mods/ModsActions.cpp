#include <memory>
#include "App/Config.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Mods/LooseFiles.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsView.h"
#include "Windows/Main/Overlays/DeleteOverlay.h"
#include "Windows/Main/Overlays/InputOverlay.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Overlays/HeroPickerOverlay.h"

// MainWindow.xaml.cs's mod handlers but adding (ModsAdd.cpp): toggle, rename, hero, delete and the
// folder shortcuts. Same steps, messages and status texts as the C#.
namespace RivalsForge::Main {

namespace {

using Services::Mods::ModError;
using Services::Mods::ModService;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;

} // namespace

bool ModsView::Busy() const { return state.busy; }

void ModsView::Done(const std::wstring& status) {
    if (!status.empty()) state.SetStatus(status);
}

void ModsView::ShowError(const char* key, const std::wstring& message) {
    Dialogs::Message(Loc::Fmt(key, message), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
}

Services::Mods::Progress ModsView::ProgressTo(float from, float span) {
    // The worker's progress, handed to the UI thread a step at a time
    auto last = std::make_shared<float>(-1.f);
    return [this, from, span, last](float k) {
        if (k - *last < 0.005f && k < 1.f) return;
        *last = k;
        const float value = from + span * k;
        Async::Post([this, value] { state.progress = value; });
    };
}

// ── Toggle ──────────────────────────────────────────────────────────────────

void ModsView::ToggleMod(const std::wstring& modId) {
    if (Busy()) return;
    const Models::Mod* mod = state.Service().FindMod(modId);
    if (!mod) return;
    const std::wstring name = mod->name;
    if (mod->isEnabled) {
        try {
            state.Service().DisableMod(modId);
            Done(Loc::Fmt("status_mod_disabled", name));
        } catch (const ModError& e) {
            ShowError("toggle_failed", e.Message());
        }
        return;
    }

    // Enabling extracts the ZIP again: on a worker thread, like an installation
    std::optional<Models::Mod> pending;
    try {
        pending = state.Service().PrepareEnableMod(modId);
    } catch (const ModError& e) {
        ShowError("toggle_failed", e.Message());
        return;
    }
    if (!pending) return;
    state.busy = true;
    state.progress = 0.f;
    state.SetStatus(Loc::Fmt("status_mod_enabling", name));
    struct Result {
        Models::Mod mod;
        std::optional<std::wstring> error;
    };
    const std::wstring paks = state.Service().PaksDir();
    Async::Run(
        [mod = *pending, paks, progress = ProgressTo(0.f, 1.f)]() mutable -> Result {
            try {
                ModService::RestoreMod(mod, paks, progress);
                return { std::move(mod), std::nullopt };
            } catch (const ModError& e) {
                return { std::move(mod), e.Message() };
            }
        },
        [this, name](Result result) {
            state.busy = false;
            state.progress = 0.f;
            std::optional<std::wstring> error = result.error;
            if (!error) {
                try {
                    state.Service().CommitEnableMod(result.mod);
                    Done(Loc::Fmt("status_mod_enabled", name));
                } catch (const ModError& e) {
                    error = e.Message();
                }
            }
            if (error) {
                state.SetStatus(Loc::Get("status_ready"));
                ShowError("toggle_failed", *error);
            }
            main.InstallFinished();
        });
}

// ── Rename ──────────────────────────────────────────────────────────────────

void ModsView::RenameMod(const std::wstring& modId) {
    if (Busy()) return;
    const Models::Mod* mod = state.Service().FindMod(modId);
    if (!mod) return;
    const std::wstring current = mod->name;
    main.Input().Show(Loc::Get("rename_mod_title"), Loc::Get("rename_message"), current, [this, modId, current](std::optional<std::wstring> result) {
        if (!result) return;
        const std::wstring newName = Text::TrimDotNet(*result);
        if (newName.empty() || newName == current) return;
        try {
            state.Service().RenameMod(modId, newName);
        } catch (const ModError& e) {
            // The C# had no try here (an error would have closed the app)
            Dialogs::Message(e.Message(), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
            return;
        }
        Done(Loc::Fmt("status_mod_renamed", newName));
    });
}

// ── Hero ────────────────────────────────────────────────────────────────────

// MoveMod_Click became the hero picker: the mod goes to the card of the hero chosen
void ModsView::ChangeHero(const std::wstring& modId) {
    if (Busy()) return;
    const Models::Mod* mod = state.Service().FindMod(modId);
    if (!mod) return;
    const std::wstring name = mod->name;
    main.HeroPicker().Show(name, state.HeroOf(*mod), [this, modId, name](std::optional<std::wstring> hero) {
        const Models::Mod* current = state.Service().FindMod(modId);
        if (!hero || !current || state.HeroOf(*current) == *hero) return;
        try {
            state.Service().SetModCharacter(modId, *hero);
        } catch (const ModError& e) {
            Dialogs::Message(e.Message(), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
            return;
        }
        Done(Loc::Fmt("status_mod_hero", name, ModsKit::HeroName(*hero)));
    });
}

// ── Delete ──────────────────────────────────────────────────────────────────

void ModsView::DeleteMod(const std::wstring& modId) {
    if (Busy()) return;
    const Models::Mod* mod = state.Service().FindMod(modId);
    if (!mod) return;
    const std::wstring name = mod->name;
    // Only out of the list, or the files too
    main.Delete().Show(name, mod->isEnabled, [this, modId, name](DeleteOverlay::Choice choice) {
        if (choice == DeleteOverlay::Choice::Cancel || Busy()) return;
        try {
            if (choice == DeleteOverlay::Choice::ListOnly) {
                state.Service().ForgetMod(modId);
                Done(Loc::Fmt("status_mod_forgotten", name));
            } else {
                state.Service().DeleteMod(modId);
                Done(Loc::Fmt("status_mod_deleted", name));
            }
        } catch (const ModError& e) {
            ShowError("delete_failed", e.Message());
        }
    });
}

// ── Folder shortcuts ────────────────────────────────────────────────────────

void ModsView::OpenModsFolder() {
    try {
        Services::Mods::FileOps::MakeDirectory(ModService::ModsStorageDir());
    } catch (const ModError& e) {
        Log::Warning(L"Creating the MODS folder: " + e.Message());
    }
    Process::Open(ModService::ModsStorageDir());   // Process.Start("explorer.exe", ModsStorageDir)
}

void ModsView::OpenPaksFolder() {
    if (!state.IsConfigured()) {
        Dialogs::Message(Loc::Get("not_configured_message"), Loc::Get("not_configured_title"), MessageButtons::Ok, MessageIcon::Information);
        return;
    }
    Process::Open(state.Service().PaksDir());
}

void ModsView::OpenIntrosFolder() {
    if (!state.IsConfigured()) {
        Dialogs::Message(Loc::Get("not_configured_message"), Loc::Get("not_configured_title"), MessageButtons::Ok, MessageIcon::Information);
        return;
    }
    // Content when an installation has no such folder
    const std::wstring content = Services::Mods::LooseFiles::ContentDir(state.Service().PaksDir());
    const std::wstring folder = Files::Combine(content, Config::IntroVideosSubPath);
    Process::Open(Files::DirectoryExists(folder) ? folder : content);
}

} // namespace RivalsForge::Main

#include <windows.h>
#include <algorithm>
#include <memory>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsView.h"

// The game itself (new): starting it from the installation in use, switching to another
// installation (Steam / Epic), copying the enabled mods into it when the user says so (on a worker
// thread; the installation left keeps its mods), and closing it when mods have to change while it's
// open (its files in Paks are locked then).
namespace RivalsForge::Main {

namespace {
using Services::GamePath::Installation;
using Services::Mods::ModService;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;
} // namespace

void ModsView::PlayGame() {
    const Installation current = state.Current();
    std::wstring error;
    if (!Services::GamePath::Launch(current, error)) {
        Dialogs::Message(Loc::Fmt("play_failed", error), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
        return;
    }
    state.SetStatus(Loc::Fmt("status_game_starting", Services::GamePath::StoreName(current)));
}

void ModsView::SwitchInstallation(const Installation& target) {
    if (Busy()) return;
    const Installation current = state.Current();
    if (state.IsConfigured() && Text::Equals(current.paks, target.paks, true)) return;
    std::vector<Models::Mod> enabled;
    if (state.IsConfigured())
        for (const auto& mod : state.Service().Mods())
            if (mod.isEnabled) enabled.push_back(mod);
    const std::wstring to = Services::GamePath::StoreName(target), from = Services::GamePath::StoreName(current);

    // The switch itself, after copying the mods (or not)
    const auto finish = [this, paks = target.paks, to](const ModService::CopyResult& copy) {
        state.Service().SwitchTo(paks, copy);
        state.Reload();
        Log::Info(L"Installation switched to " + paks);
        if (!copy.failed.empty()) {
            std::wstring list;
            for (const auto& [id, message] : copy.failed) {
                const Models::Mod* mod = state.Service().FindMod(id);
                list += (list.empty() ? L"" : L"\n") + Loc::Fmt("addmod_error_line", mod ? mod->name : id, message);
            }
            Dialogs::Message(Loc::Fmt("switch_some_failed", to, list), Loc::Get("switch_title"), MessageButtons::Ok, MessageIcon::Warning);
        }
        Done(Loc::Fmt("status_switched", to));
    };
    const auto copyAndFinish = [this, enabled, paks = target.paks, to, finish]() {
        if (Busy()) return;
        state.busy = true;
        state.progress = 0.f;
        state.SetStatus(Loc::Fmt("status_switching", to));
        Async::Run([enabled, paks, progress = ProgressTo(0.f, 1.f)]() { return ModService::CopyMods(enabled, paks, progress); },
                   [this, finish](ModService::CopyResult result) {
                       state.busy = false;
                       state.progress = 0.f;
                       finish(result);
                       main.InstallFinished();
                   });
    };
    if (enabled.empty()) {
        finish({});
        return;
    }
    // Copy the enabled mods too? (the installation they're in keeps them either way)
    const int count = static_cast<int>(enabled.size());
    Dialogs::Message(Loc::Fmt(count == 1 ? "switch_message_one" : "switch_message_many", to, target.paks, count, from), Loc::Get("switch_title"),
                     MessageButtons::YesNoCancel, MessageIcon::Information, [this, copyAndFinish, finish](MessageResult answer) {
                         if (answer == MessageResult::Yes) WhenGameClosed(copyAndFinish);
                         else if (answer == MessageResult::No) finish({});
                     });
}

void ModsView::WhenGameClosed(std::function<void()> action, std::function<void()> cancelled) {
    if (Services::GamePath::RunningProcesses(state.installations).empty()) {
        action();
        return;
    }
    Dialogs::Message(Loc::Get("game_open_message"), Loc::Get("game_open_title"), MessageButtons::YesNo, MessageIcon::Warning,
                     [this, action, cancelled](MessageResult answer) {
                         if (answer == MessageResult::Yes) CloseGame(action);
                         else if (cancelled) cancelled();
                     });
}

void ModsView::CloseGame(std::function<void()> then) {
    if (Busy()) return;
    state.busy = true;
    state.SetStatus(Loc::Get("status_closing_game"));
    Async::Run(
        [installations = state.installations]() -> bool {
            // The launcher can start the client again as it goes: a few rounds until nothing's left
            for (int round = 0; round < 3; ++round) {
                const std::vector<uint32_t> pids = Services::GamePath::RunningProcesses(installations);
                if (pids.empty()) break;
                for (const uint32_t pid : pids) Process::Kill(pid);
                for (int i = 0; i < 50 && std::any_of(pids.begin(), pids.end(), [](uint32_t pid) { return Process::IsAlive(pid); }); ++i)
                    Sleep(100);
            }
            if (!Services::GamePath::RunningProcesses(installations).empty()) return false;
            Sleep(500);   // Windows lets go of the files a moment after
            return true;
        },
        [this, then](bool closed) {
            state.busy = false;
            if (!closed) {
                state.SetStatus(Loc::Get("status_ready"));
                Dialogs::Message(Loc::Get("game_close_failed"), Loc::Get("game_open_title"), MessageButtons::Ok, MessageIcon::Error);
                return;
            }
            Log::Info(L"Game closed to change mods");
            state.SetStatus(Loc::Get("status_game_closed"));
            then();
        });
}

} // namespace RivalsForge::Main

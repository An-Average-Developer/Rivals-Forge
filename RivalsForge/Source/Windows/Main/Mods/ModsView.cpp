#include "Windows/Main/Mods/ModsView.h"
#include "Graphics/Animation.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/MainWindow.h"

// RefreshUI's panel visibility: SetupPanel while the game isn't configured, EmptyStatePanel with no
// mods, SectionsScroll otherwise. The panels cross-fade (the one coming in rises a little); only the
// one shown takes input.
namespace RivalsForge::Main {

ModsView::ModsView(MainWindow& window) : main(window), state(window.State()) {}

void ModsView::Draw(UI::Ui& ui, const Rect& content) {
    const bool configured = state.IsConfigured();
    const int total = state.TotalMods();
    enum Panel { Setup, Empty, List };
    const Panel shown = !configured ? Setup : total == 0 ? Empty : List;

    const bool muted = ui.InputMuted();
    ui.PushClip(content);
    for (int panel = Setup; panel <= List; ++panel) {
        const float k = Anim::EaseInOutCubic(ui.Tween(ui.MakeId("mods-panel", panel), panel == shown ? 1.f : 0.f, 0.3f));
        if (k <= 0.001f) continue;
        if (k < 1.f) ui.KeepAnimating();
        ui.SetInputMuted(muted || panel != shown);
        UI::Material::Motion::Enter enter(ui, k, panel == shown ? 18.f : 0.f);
        if (panel == Setup) SetupPanel(ui, content);
        else if (panel == Empty) EmptyState(ui, content);
        else HeroList(ui, content);
    }
    ui.SetInputMuted(muted);
    ui.PopClip();
}

} // namespace RivalsForge::Main

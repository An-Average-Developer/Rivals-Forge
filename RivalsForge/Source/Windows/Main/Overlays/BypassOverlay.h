#pragma once
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// The UTOC Signature Bypass Patch's card: whether the installation in use has it (and where it
// goes), why mods need it, its credits (DeathChaos, with a link to the mod's Nexus page) and
// Install / Install again.
class BypassOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    // `needed`: opened because mods were just added to an installation without it
    void Show(bool needed);

protected:
    float CardWidth() const override { return 560.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Install();

    bool needed = false;
};

} // namespace RivalsForge::Main

#pragma once
#include <string>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// The Nexus Mods account card: who's signed in (Premium or free) or why not, what the API key is for
// and where to get it, the key's text box with Connect / Disconnect, and the switch that makes this
// app open the site's nxm:// download links (what a free account downloads with).
class StoreAccountOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    // `needed`: opened because something needs the account (a download), which the card says
    void Show(bool needed);

protected:
    float CardWidth() const override { return 560.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    std::wstring Intro() const;

    bool needed = false;
    std::wstring key;
    bool focusPending = false;
};

} // namespace RivalsForge::Main

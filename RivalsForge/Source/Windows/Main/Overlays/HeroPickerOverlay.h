#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// Which hero a mod is for (the C#'s "Move ▾" menu, now with the heroes): a search box, the role
// tabs (All / Vanguard / Duelist / Strategist, the highlight sliding between them) and the grid of
// portraits, "No character" first. The mod's hero is ringed in red; a click picks, Enter picks the
// first one shown, Escape / a click outside / Cancel leaves it as it was. `done` gets the hero id
// ("" = none) or nothing when cancelled.
class HeroPickerOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show(const std::wstring& modName, const std::wstring& currentHero, std::function<void(std::optional<std::wstring>)> done);

protected:
    float CardWidth() const override { return 780.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    // The heroes the search and the role tab leave ("" = no character, first)
    std::vector<std::wstring> Shown() const;
    void Pick(const std::wstring& hero);

    std::wstring modName, current, search;
    int role = -1;                   // -1 = all, else Data::Role
    std::wstring shownKey;           // search + role: when it changes the grid comes in again
    double gridAt = -10.0;
    bool focusPending = false;
    bool scrollPending = false;      // the grid scrolls to the mod's hero when the card opens
    std::optional<std::wstring> result;
    std::function<void(std::optional<std::wstring>)> done;
};

} // namespace RivalsForge::Main

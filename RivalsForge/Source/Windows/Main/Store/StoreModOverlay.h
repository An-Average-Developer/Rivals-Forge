#pragma once
#include <optional>
#include <string>
#include "Services/Store/Nexus.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// A store mod's card: its picture as a banner with the name, author, version and category over it;
// its numbers and description on the left; its files on the right, each with Install (and
// "Installed" for the version already here), what the account means for downloading on top of
// them; "View on Nexus" and Close at the foot. The details load when it opens.
class StoreModOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show(const Services::Store::StoreMod& mod);

protected:
    float CardWidth() const override { return 860.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Load();
    void Banner(UI::Ui& ui, const Rect& r);
    void Description(UI::Ui& ui, const Rect& r);
    void Files(UI::Ui& ui, const Rect& r);
    float FileHeight(UI::Ui& ui, const Services::Store::ModFile& file, float width) const;

    Services::Store::StoreMod mod;
    std::optional<Services::Store::ModDetails> details;
    bool loading = false;
    std::wstring error;
    unsigned generation = 0;
};

} // namespace RivalsForge::Main

#pragma once
#include <functional>
#include "Graphics/Color.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace RivalsForge::Main {

// The in-window replacement of the C#'s dialog windows (InputDialog, IconPickerDialog, the
// MessageBox confirmations and errors): the window dims, the card springs up sliding in from the
// left and its rows come in one by one. Escape or a click outside closes it while Closable().
class CardOverlay : public Overlay {
public:
    using Overlay::Overlay;
    void Open();
    void Close();
    bool IsOpen() const { return open; }
    bool Visible() const override { return open || changedAt > -10.0; }
    void Draw(UI::Ui& ui, const Rect& screen) final;

    // Runs once the card is closed (any way)
    std::function<void()> onClosed;
    // A card opened over this one: while it's shown this one
    // ignores Escape and outside clicks
    void SetCover(const CardOverlay* card) { cover = card; }

protected:
    virtual float CardWidth() const { return 520.f; }
    virtual float CardHeight(UI::Ui& ui, float width) = 0;
    virtual void DrawCard(UI::Ui& ui, const Rect& card) = 0;
    virtual bool Closable() const { return true; }
    virtual Color Accent() const;   // the glow around the card (the Marvel red)
    // Entrance (0..1) of content row `index` while the card opens
    float Reveal(UI::Ui& ui, int index) const;
    double OpenedAt() const { return changedAt; }

    // The card's frame: shadow, glow, the background with its cut corners, the accent line along the
    // top and the halftone (call first in DrawCard)
    void Frame(UI::Ui& ui, const Rect& card) const;
    // What follows is clipped to the card's outline (close with ui.g.PopLayer())
    void PushCardClip(UI::Ui& ui, const Rect& card) const;

    static constexpr float kCut = 16.f, kPad = 26.f;

private:
    bool open = false;
    double changedAt = -10.0;   // -1 / -2: stamp on the next frame (open / close)
    const CardOverlay* cover = nullptr;
};

} // namespace RivalsForge::Main

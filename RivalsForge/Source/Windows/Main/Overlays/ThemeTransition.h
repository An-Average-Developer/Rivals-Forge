#pragma once
#include "Graphics/Graphics.h"

namespace RivalsForge::Main {

// The header's sun / moon button: a snapshot of the window in the old theme covers it and a slanted
// edge sweeps across it from the top right (where the button is) to the bottom left, uncovering the
// new theme behind it, a red light running along the edge (the reference shot's diagonal split, in
// motion). Meanwhile the button's icon spins and swaps.
class ThemeTransition {
public:
    // Asked for by the button; the next frame takes the snapshot (drawn in the old theme) and only
    // then switches the theme
    void Request() {
        if (!Animating()) pending = true;
    }
    bool Pending() const { return pending; }
    bool Animating() const { return pending || active; }
    // Called by the window with the freshly drawn old-theme snapshot: switches the theme (and saves it)
    void Begin(ComPtr<ID2D1Bitmap1> snapshot, double now);
    // Draws what's left of the snapshot over the new theme
    void Draw(Graphics& g, const Rect& screen, double now);
    // The button's icon: its turn (degrees) while the theme changes
    float IconAngle(double now) const;

private:
    bool pending = false, active = false;
    double startedAt = -10.0;
    ComPtr<ID2D1Bitmap1> snapshot;
};

} // namespace RivalsForge::Main

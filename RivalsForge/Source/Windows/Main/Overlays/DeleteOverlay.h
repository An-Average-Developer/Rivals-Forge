#pragma once
#include <functional>
#include <string>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// Deleting a mod (was the "Delete …?" Yes / No): the mod, and two choices as big buttons. "Only
// from the list" forgets it and leaves its ZIP and its files where they are (an enabled one keeps
// loading in the game, and it says so); "Also the files" deletes the ZIP and the extracted files,
// as before. Escape, a click outside or Cancel: nothing.
class DeleteOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    enum class Choice { Cancel, ListOnly, Files };
    void Show(const std::wstring& modName, bool enabled, std::function<void(Choice)> done);

protected:
    float CardWidth() const override { return 540.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Answer(Choice choice);
    // One of the two choices: a slanted icon box, the title, the explanation; true when clicked
    bool Option(UI::Ui& ui, const char* id, const Rect& r, bool danger, const std::wstring& title, const std::wstring& text,
                const std::wstring& note);
    float OptionHeight(UI::Ui& ui, float width, const std::wstring& text, const std::wstring& note) const;

    std::wstring name;
    bool enabled = false;
    Choice result = Choice::Cancel;
    std::function<void(Choice)> done;
};

} // namespace RivalsForge::Main

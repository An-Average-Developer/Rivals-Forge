#pragma once
#include <functional>
#include <optional>
#include <string>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// InputDialog.xaml(.cs) as an in-window card: the title, the message, a text box that opens focused
// with its text selected, and Cancel / OK. Enter = OK, Escape or a click outside = Cancel. `done`
// gets the text (InputDialog.Result) or nothing when cancelled.
class InputOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show(const std::wstring& title, const std::wstring& message, const std::wstring& defaultValue,
              std::function<void(std::optional<std::wstring>)> done);

protected:
    float CardWidth() const override { return 440.f; }   // Width="440"
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Finish(bool accepted);

    std::wstring title, message, text;
    std::optional<std::wstring> result;
    std::function<void(std::optional<std::wstring>)> done;
    bool focusPending = false;
};

} // namespace RivalsForge::Main

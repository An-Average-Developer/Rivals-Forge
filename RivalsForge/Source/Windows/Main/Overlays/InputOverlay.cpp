#include "Windows/Main/Overlays/InputOverlay.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kButtonW = 100.f, kButtonH = 36.f;

TextStyle TitleStyle() { return Kit::Display(22.f, DWRITE_FONT_WEIGHT_BOLD); }
TextStyle MessageStyle() { return Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true); }

UI::TextBoxStyle BoxStyle() { return Kit::InputBoxStyle(); }

} // namespace

void InputOverlay::Show(const std::wstring& dialogTitle, const std::wstring& dialogMessage, const std::wstring& defaultValue,
                        std::function<void(std::optional<std::wstring>)> callback) {
    title = dialogTitle;
    message = dialogMessage;
    text = defaultValue;
    result.reset();
    done = std::move(callback);
    focusPending = true;   // Loaded: InputBox.Focus(); InputBox.SelectAll();
    onClosed = [this] {
        auto callback = std::move(done);
        done = nullptr;
        if (callback) callback(result);
    };
    Open();
}

void InputOverlay::Finish(bool accepted) {
    if (!IsOpen()) return;
    if (accepted) result = text;   // Result = InputBox.Text; DialogResult = true
    Close();
}

float InputOverlay::CardHeight(UI::Ui& ui, float width) {
    const float messageH = ui.g.Measure(message, MessageStyle(), width - kPad * 2.f).h;
    return kPad + Kit::LineH(ui, TitleStyle()) + 6.f + messageH + 12.f + UI::TextBoxHeight(ui, BoxStyle()) + 18.f + kButtonH + kPad;
}

void InputOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        const float h = Kit::LineH(ui, TitleStyle());
        ui.g.Text(Kit::Upper(title), { inner.x, y, inner.w, h }, Kit::Palette::Text, TitleStyle());   // TitleBlock
        y += h + 6.f;
        const float messageH = ui.g.Measure(message, MessageStyle(), inner.w).h;
        ui.g.Text(message, { inner.x, y, inner.w, messageH }, Kit::Palette::TextDim, MessageStyle());   // MessageBlock
        y += messageH + 12.f;
    }
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const UI::Id box = ui.MakeId("input-box");
        if (focusPending && IsOpen()) {
            focusPending = false;
            ui.SetFocus(box);
            UI::SetTextBoxSelection(ui, box, 0, static_cast<int>(text.size()));
        }
        const float h = UI::TextBoxHeight(ui, BoxStyle());
        bool entered = false;
        UI::TextBox(ui, box, { inner.x, y, inner.w, h }, text, BoxStyle(), &entered);
        if (entered) Finish(true);
        y += h + 18.f;
    }
    M::Motion::Enter enter(ui, Reveal(ui, 2));
    const Rect ok{ inner.R() - kButtonW, y, kButtonW, kButtonH };
    const Rect cancel{ ok.x - 6.f - kButtonW, y, kButtonW, kButtonH };
    if (Kit::Button(ui, ui.MakeId("input-cancel"), cancel, Loc::Get("cancel"), Kit::DialogButton(false))) Finish(false);
    if (Kit::Button(ui, ui.MakeId("input-ok"), ok, Loc::Get("ok"), Kit::DialogButton(true))) Finish(true);
    if (IsOpen() && ui.KeyPressed(VK_RETURN)) Finish(true);
}

} // namespace RivalsForge::Main

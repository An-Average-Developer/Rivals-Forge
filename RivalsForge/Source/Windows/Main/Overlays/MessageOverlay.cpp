#include "Windows/Main/Overlays/MessageOverlay.h"
#include <cmath>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;

constexpr float kBadge = 40.f, kButtonW = 100.f, kButtonH = 36.f;   // DialogBtn (was 88 x 34)

Color IconColor(MessageIcon icon) {
    switch (icon) {
        case MessageIcon::Warning: return Kit::Palette::Gold;
        case MessageIcon::Error: return Kit::Palette::Accent;
        default: return Kit::Palette::Info;
    }
}

TextStyle BodyStyle() { return Kit::Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true); }

} // namespace

void MessageOverlay::Show(const std::wstring& text, const std::wstring& title, MessageButtons buttons, MessageIcon icon,
                          std::function<void(MessageResult)> done) {
    queue.push_back({ text, title, buttons, icon, std::move(done) });
    if (!Visible()) Next();
}

void MessageOverlay::Next() {
    if (queue.empty()) return;
    current = std::move(queue.front());
    queue.pop_front();
    // Escape / a click outside: the last button (No, or OK)
    result = current.buttons == MessageButtons::YesNo         ? MessageResult::No
             : current.buttons == MessageButtons::YesNoCancel ? MessageResult::Cancel
                                                              : MessageResult::Ok;
    onClosed = [this] {
        auto done = std::move(current.done);
        const MessageResult answer = result;
        current = {};
        if (done) done(answer);
        Next();
    };
    Open();
}

void MessageOverlay::Answer(MessageResult answer) {
    if (!IsOpen()) return;   // already closing
    result = answer;
    Close();
}

Color MessageOverlay::Accent() const { return current.icon == MessageIcon::Warning ? Kit::Palette::Gold : Kit::Palette::Accent; }

float MessageOverlay::CardHeight(UI::Ui& ui, float width) {
    const float textH = ui.g.Measure(current.text, BodyStyle(), width - kPad * 2.f).h;
    return kPad + kBadge + 14.f + textH + 22.f + kButtonH + kPad;
}

void MessageOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;

    // Icon badge + title
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        float x = inner.x;
        if (current.icon != MessageIcon::None) {
            const Color c = IconColor(current.icon);
            // A slanted badge of the icon's colour, a ring popping out of it as the card opens
            const Rect badge{ x, y, kBadge + 8.f, kBadge };
            const float pulse = M::Motion::Stagger(ui, OpenedAt() + 0.2, 0, 0.f, 0.6f);
            if (pulse < 1.f) Kit::StrokeSlant(ui, badge.Inset(-10.f * pulse), c.Fade(0.6f * (1.f - pulse)), 1.5f, 10.f);
            Kit::FillSlant(ui, badge, c.Fade(0.2f), 10.f);
            Kit::StrokeSlant(ui, badge, c.Fade(0.7f), 1.f, 10.f);
            const M::Icon icon = current.icon == MessageIcon::Warning ? M::Icon::AlertCircle
                                 : current.icon == MessageIcon::Error ? M::Icon::Close
                                                                      : M::Icon::Information;
            M::DrawIcon(ui.g, icon, badge.CX(), badge.CY(), 20.f, c);
            x = badge.R() + 14.f;
        }
        TextStyle title = Kit::Display(22.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        ui.g.Text(Kit::Upper(current.title), { x, y, inner.R() - x, kBadge }, Kit::Palette::Text, title);
    }
    y += kBadge + 14.f;

    // The message
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const float textH = ui.g.Measure(current.text, BodyStyle(), inner.w).h;
        ui.g.Text(current.text, { inner.x, y, inner.w, textH }, Kit::Palette::Body, BodyStyle());
        y += textH + 22.f;
    }

    // Buttons, right aligned: OK, Yes / No, or Yes / No / Cancel (Enter = the first, Escape = the last)
    M::Motion::Enter enter(ui, Reveal(ui, 2));
    const Rect last{ inner.R() - kButtonW, y, kButtonW, kButtonH };
    if (current.buttons == MessageButtons::YesNoCancel) {
        const Rect no{ last.x - 6.f - kButtonW, y, kButtonW, kButtonH };
        const Rect yes{ no.x - 6.f - kButtonW, y, kButtonW, kButtonH };
        if (Kit::Button(ui, ui.MakeId("msg-yes"), yes, Loc::Get("yes"), Kit::DialogButton(true))) Answer(MessageResult::Yes);
        if (Kit::Button(ui, ui.MakeId("msg-no"), no, Loc::Get("no"), Kit::DialogButton(false))) Answer(MessageResult::No);
        if (Kit::Button(ui, ui.MakeId("msg-cancel"), last, Loc::Get("cancel"), Kit::DialogButton(false))) Answer(MessageResult::Cancel);
        if (IsOpen() && ui.KeyPressed(VK_RETURN)) Answer(MessageResult::Yes);
    } else if (current.buttons == MessageButtons::YesNo) {
        const Rect yes{ last.x - 6.f - kButtonW, y, kButtonW, kButtonH };
        if (Kit::Button(ui, ui.MakeId("msg-yes"), yes, Loc::Get("yes"), Kit::DialogButton(true))) Answer(MessageResult::Yes);
        if (Kit::Button(ui, ui.MakeId("msg-no"), last, Loc::Get("no"), Kit::DialogButton(false))) Answer(MessageResult::No);
        if (IsOpen() && ui.KeyPressed(VK_RETURN)) Answer(MessageResult::Yes);
    } else {
        if (Kit::Button(ui, ui.MakeId("msg-ok"), last, Loc::Get("ok"), Kit::DialogButton(true))) Answer(MessageResult::Ok);
        if (IsOpen() && ui.KeyPressed(VK_RETURN)) Answer(MessageResult::Ok);
    }
}

} // namespace RivalsForge::Main

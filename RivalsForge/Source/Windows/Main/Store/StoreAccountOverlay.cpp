#include "App/Config.h"
#include "Windows/Main/Store/StoreAccountOverlay.h"
#include <cmath>
#include "Core/Process.h"
#include "Localization/Localization.h"
#include "Services/Store/NexusAccount.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Store/StoreView.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;
namespace NexusAccount = Services::Store::NexusAccount;

constexpr const wchar_t* kKeysPage = L"https://next.nexusmods.com/settings/api-keys";
constexpr float kStatusH = 46.f, kButtonH = 36.f, kSwitchW = 40.f, kSwitchH = 22.f;

TextStyle TitleStyle() { return Kit::Display(24.f, DWRITE_FONT_WEIGHT_BOLD); }
TextStyle TextStyleOf() { return Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true); }
TextStyle LabelStyle() { return Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD); }

} // namespace

void StoreAccountOverlay::Show(bool why) {
    needed = why;
    key = main.Store().GetAccount().key;
    focusPending = true;
    Open();
}

std::wstring StoreAccountOverlay::Intro() const { return Loc::Get(needed ? "account_intro_needed" : "account_intro"); }

float StoreAccountOverlay::CardHeight(UI::Ui& ui, float width) {
    const float inner = width - 2.f * kPad;
    float h = kPad + Kit::LineH(ui, TitleStyle()) + 14.f + kStatusH + 14.f;
    h += ui.g.Measure(Intro(), TextStyleOf(), inner).h + 14.f;
    h += Kit::LineH(ui, LabelStyle()) + 6.f + kButtonH + 22.f;
    h += 1.f + 18.f + std::max(kSwitchH, ui.g.Measure(Loc::Get("account_nxm_title"), Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD), inner - 60.f).h) + 6.f;
    h += ui.g.Measure(Loc::Get("account_nxm_text"), TextStyleOf(), inner - 60.f).h + 24.f;
    return h + kButtonH + kPad;
}

void StoreAccountOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    StoreView& store = main.Store();
    const StoreView::Account& account = store.GetAccount();
    const Rect inner = card.Inset(kPad);
    float y = inner.y;

    // Title and who's signed in
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        const float h = Kit::LineH(ui, TitleStyle());
        ui.g.Text(Kit::Upper(Loc::Get("account_title")), { inner.x, y, inner.w, h }, Kit::Palette::Text, TitleStyle());
        y += h + 14.f;
        const Rect status{ inner.x, y, inner.w, kStatusH };
        Color color = Kit::Palette::TextDim;
        std::wstring line1, line2;
        if (account.checking) {
            color = Kit::Palette::Info;
            line1 = Loc::Get("account_checking");
        } else if (account.info) {
            color = account.info->premium ? Kit::Palette::Gold : Kit::Palette::Enabled;
            line1 = Loc::Fmt("account_signed_in", account.info->name);
            line2 = Loc::Get(account.info->premium ? "account_premium" : "account_free");
        } else if (!account.error.empty()) {
            color = Kit::Palette::Danger;
            line1 = Loc::Get("account_error");
            line2 = account.error;
        } else {
            line1 = Loc::Get("account_none");
            line2 = Loc::Get("account_none_text");
        }
        Kit::FillSlant(ui, status, color.Fade(0.12f), 8.f);
        Kit::EdgeStripe(ui, status, 4.f, color, 8.f);
        Kit::DrawGlyph(ui, account.info ? Kit::Glyph::Check : Kit::Glyph::Person, status.x + 26.f, status.CY(), 17.f, color);
        TextStyle a = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        a.ellipsis = true;
        TextStyle b = Kit::Style(11.5f);
        b.ellipsis = true;
        ui.g.Text(line1, { status.x + 46.f, status.y + (line2.empty() ? 14.f : 6.f), status.w - 60.f, 18.f }, Kit::Palette::Text, a);
        if (!line2.empty()) ui.g.Text(line2, { status.x + 46.f, status.y + 24.f, status.w - 60.f, 16.f }, color, b);
        if (account.checking) ui.KeepAnimating();
        y += kStatusH + 14.f;
    }

    // What the key is for, and the key
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const std::wstring intro = Intro();
        const float h = ui.g.Measure(intro, TextStyleOf(), inner.w).h;
        ui.g.Text(intro, { inner.x, y, inner.w, h }, Kit::Palette::Body, TextStyleOf());
        y += h + 14.f;
        const float lh = Kit::LineH(ui, LabelStyle());
        ui.g.Text(Kit::Upper(Loc::Get("account_key")), { inner.x, y, inner.w * 0.5f, lh }, Kit::Palette::TextDim, LabelStyle());
        // Where to get one
        {
            TextStyle link = Kit::Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            link.align = HAlign::Right;
            const std::wstring text = Loc::Get("account_get_key");
            const float w = std::ceil(ui.g.Measure(text, link).w) + 20.f;
            const Rect r{ inner.R() - w, y - 2.f, w, lh + 4.f };
            const UI::Id id = ui.MakeId("account-get-key");
            const UI::Interaction it = ui.Interact(id, r);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            ui.g.Text(text, { r.x, r.y + 2.f, r.w - 18.f, lh }, Lerp(Kit::Palette::Info, Kit::Palette::Strong, hover), link);
            Kit::DrawGlyph(ui, Kit::Glyph::External, r.R() - 7.f, r.CY(), 11.f, Lerp(Kit::Palette::Info, Kit::Palette::Strong, hover));
            if (it.clicked) Process::OpenUrl(kKeysPage);
        }
        y += lh + 6.f;
        const bool signedIn = account.info.has_value() && key == account.key;
        const Kit::ButtonStyle connect = signedIn ? Kit::Danger() : Kit::DialogButton(true);
        const std::wstring label = Loc::Get(signedIn ? "account_disconnect" : "account_connect");
        const float bw = std::max(120.f, Kit::ButtonWidth(ui, label, connect));
        UI::TextBoxStyle box = Kit::InputBoxStyle(13.f);
        box.placeholder = Loc::Get("account_key_placeholder");
        box.password = box.password || Config::PresentationMode;   // the key never shows in presentation mode
        const UI::Id boxId = ui.MakeId("account-key");
        if (focusPending && IsOpen()) {
            focusPending = false;
            if (key.empty()) ui.SetFocus(boxId);
        }
        bool entered = false;
        UI::TextBox(ui, boxId, { inner.x, y, inner.w - bw - 8.f, kButtonH }, key, box, &entered);
        if (Kit::Button(ui, ui.MakeId("account-connect"), { inner.R() - bw, y, bw, kButtonH }, label, connect, !account.checking) || (entered && !signedIn)) {
            if (signedIn) key.clear();
            store.SetKey(key);
        }
        y += kButtonH + 22.f;
    }

    // nxm:// links
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2));
        ui.g.FillRect({ inner.x, y, inner.w, 1.f }, Kit::Palette::Line);
        y += 18.f;
        const bool on = NexusAccount::HandlerIsOurs();
        const Rect sw{ inner.x, y, kSwitchW, kSwitchH };
        if (Kit::Switch(ui, ui.MakeId("account-nxm"), sw, on)) {
            if (on) NexusAccount::UnregisterHandler();
            else if (!NexusAccount::RegisterHandler())
                Dialogs::Message(Loc::Get("store_nxm_failed"), Loc::Get("dialog_error"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        }
        const float tx = inner.x + 60.f, tw = inner.w - 60.f;
        TextStyle title = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
        const float th = ui.g.Measure(Loc::Get("account_nxm_title"), title, tw).h;
        ui.g.Text(Loc::Get("account_nxm_title"), { tx, y + 1.f, tw, th }, Kit::Palette::Text, title);
        y += std::max(kSwitchH, th) + 6.f;
        const float h = ui.g.Measure(Loc::Get("account_nxm_text"), TextStyleOf(), tw).h;
        ui.g.Text(Loc::Get("account_nxm_text"), { tx, y, tw, h }, Kit::Palette::TextDim, TextStyleOf());
        y += h + 24.f;
    }

    M::Motion::Enter enter(ui, Reveal(ui, 3));
    if (Kit::Button(ui, ui.MakeId("account-close"), { inner.R() - 110.f, y, 110.f, kButtonH }, Loc::Get("store_close"), Kit::DialogButton(false)))
        Close();
}

} // namespace RivalsForge::Main

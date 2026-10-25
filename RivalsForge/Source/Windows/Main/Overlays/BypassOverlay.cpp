#include "Windows/Main/Overlays/BypassOverlay.h"
#include <cmath>
#include "Core/Process.h"
#include "Localization/Localization.h"
#include "Services/Game/SignatureBypass.h"
#include "Services/Mods/ModService.h"
#include "UI/Material/Motion.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;
namespace Bypass = Services::Game::SignatureBypass;

constexpr float kStatusH = 50.f, kButtonH = 36.f, kButtonW = 130.f, kCreditPad = 16.f;

TextStyle TitleStyle() { return Kit::Display(24.f, DWRITE_FONT_WEIGHT_BOLD); }
TextStyle BodyStyle() { return Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true); }
TextStyle LabelStyle() { return Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD); }
TextStyle NameStyle() { return Kit::Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true); }

} // namespace

void BypassOverlay::Show(bool why) {
    needed = why;
    Open();
}

void BypassOverlay::Install() {
    MainState& state = main.State();
    const auto installation = state.Current();
    try {
        Bypass::Install(installation);
    } catch (const Services::Mods::ModError& e) {
        Dialogs::Message(Loc::Fmt("bypass_failed", e.Message()), Loc::Get("dialog_error"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        return;
    }
    state.SetStatus(Loc::Fmt("status_bypass_installed", Services::GamePath::StoreName(installation)));
    needed = false;
}

float BypassOverlay::CardHeight(UI::Ui& ui, float width) {
    const float inner = width - 2.f * kPad;
    float h = kPad + Kit::LineH(ui, TitleStyle()) + 14.f + kStatusH + 14.f;
    h += ui.g.Measure(Loc::Get(needed ? "bypass_intro_needed" : "bypass_intro"), BodyStyle(), inner).h + 16.f;
    const float cw = inner - 2.f * kCreditPad;
    h += kCreditPad + Kit::LineH(ui, LabelStyle()) + 6.f + ui.g.Measure(Bypass::kName, NameStyle(), cw).h + 4.f + 18.f + 8.f +
         ui.g.Measure(Loc::Get("bypass_credit_text"), BodyStyle(), cw).h + 12.f + 30.f + kCreditPad;
    h += 14.f + 18.f + 20.f;
    return h + kButtonH + kPad;
}

void BypassOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    MainState& state = main.State();
    const auto installation = state.Current();
    const bool configured = state.IsConfigured();
    const bool installed = configured && Bypass::IsInstalled(installation);
    const std::wstring store = Services::GamePath::StoreName(installation);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;

    // Title and whether this installation has it
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        const float h = Kit::LineH(ui, TitleStyle());
        ui.g.Text(Kit::Upper(Loc::Get("bypass_title")), { inner.x, y, inner.w, h }, Kit::Palette::Text, TitleStyle());
        y += h + 14.f;
        const Color color = installed ? Kit::Palette::Enabled : Kit::Palette::Danger;
        const Rect status{ inner.x, y, inner.w, kStatusH };
        Kit::FillSlant(ui, status, color.Fade(0.12f), 8.f);
        Kit::EdgeStripe(ui, status, 4.f, color, 8.f);
        Kit::DrawGlyph(ui, installed ? Kit::Glyph::Check : Kit::Glyph::Warning, status.x + 26.f, status.CY(), 17.f, color);
        TextStyle a = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        a.ellipsis = true;
        TextStyle b = Kit::Style(11.f);
        b.ellipsis = true;
        ui.g.Text(Loc::Fmt(installed ? "bypass_installed" : "bypass_missing", store), { status.x + 46.f, status.y + 8.f, status.w - 60.f, 18.f },
                  Kit::Palette::Text, a);
        ui.g.Text(configured ? Bypass::BinariesDir(installation) : Loc::Get("summary_game_not_found"),
                  { status.x + 46.f, status.y + 27.f, status.w - 60.f, 16.f }, Kit::Palette::TextDim, b);
        y += kStatusH + 14.f;
    }

    // Why
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const std::wstring text = Loc::Get(needed ? "bypass_intro_needed" : "bypass_intro");
        const float h = ui.g.Measure(text, BodyStyle(), inner.w).h;
        ui.g.Text(text, { inner.x, y, inner.w, h }, Kit::Palette::Body, BodyStyle());
        y += h + 16.f;
    }

    // The credits
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2));
        const float cw = inner.w - 2.f * kCreditPad;
        const float lh = Kit::LineH(ui, LabelStyle());
        const float nh = ui.g.Measure(Bypass::kName, NameStyle(), cw).h;
        const float th = ui.g.Measure(Loc::Get("bypass_credit_text"), BodyStyle(), cw).h;
        const Rect box{ inner.x, y, inner.w, kCreditPad + lh + 6.f + nh + 4.f + 18.f + 8.f + th + 12.f + 30.f + kCreditPad };
        Kit::FillSlant(ui, box, Kit::Palette::Gold.Fade(0.07f), 10.f);
        Kit::StrokeSlant(ui, box, Kit::Palette::Gold.Fade(0.35f), 1.f, 10.f);
        Kit::Halftone(ui, { box.R() - 140.f, box.y + 1.f, 120.f, box.h - 2.f }, 0.04f);
        float cy = box.y + kCreditPad;
        const float cx = box.x + kCreditPad + 6.f;
        ui.g.Text(Kit::Upper(Loc::Get("bypass_credits")), { cx, cy, cw, lh }, Kit::Palette::Gold, LabelStyle());
        cy += lh + 6.f;
        ui.g.Text(Bypass::kName, { cx, cy, cw, nh }, Kit::Palette::Text, NameStyle());
        cy += nh + 4.f;
        TextStyle by = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        by.valign = VAlign::Center;
        ui.g.Text(Loc::Fmt("bypass_credit_by", Bypass::kAuthor), { cx, cy, cw, 18.f }, Kit::Palette::Gold, by);
        cy += 18.f + 8.f;
        ui.g.Text(Loc::Get("bypass_credit_text"), { cx, cy, cw, th }, Kit::Palette::TextDim, BodyStyle());
        cy += th + 12.f;
        Kit::ButtonStyle link = Kit::Ghost();
        link.fontSize = 12.f;
        link.padding = Edges(12.f, 4.f);
        const std::wstring label = Loc::Get("bypass_credit_link");
        const float lw = Kit::ButtonWidth(ui, label, link) + 22.f;
        const Rect linkR{ cx, cy, lw, 30.f };
        if (Kit::Button(ui, ui.MakeId("bypass-page"), linkR, L"    " + label, link)) Process::OpenUrl(Bypass::kPage);
        Kit::DrawGlyph(ui, Kit::Glyph::External, linkR.x + 20.f, linkR.CY(), 12.f, Kit::Palette::TextDim);
        y = box.B() + 14.f;
    }

    // Close the game first
    {
        M::Motion::Enter enter(ui, Reveal(ui, 3));
        TextStyle note = Kit::Style(11.5f);
        note.valign = VAlign::Center;
        Kit::DrawGlyph(ui, Kit::Glyph::Warning, inner.x + 7.f, y + 9.f, 12.f, Kit::Palette::TextFaint);
        ui.g.Text(Loc::Get("bypass_close_game"), { inner.x + 20.f, y, inner.w - 20.f, 18.f }, Kit::Palette::TextFaint, note);
        y += 18.f + 20.f;
    }

    M::Motion::Enter enter(ui, Reveal(ui, 4));
    const Kit::ButtonStyle install = Kit::DialogButton(true);
    const std::wstring label = Loc::Get(installed ? "bypass_reinstall" : "bypass_install");
    const float iw = std::max(kButtonW, Kit::ButtonWidth(ui, label, install));
    const Rect installR{ inner.R() - iw, y, iw, kButtonH };
    if (Kit::Button(ui, ui.MakeId("bypass-install"), installR, label, install, configured && !state.busy)) Install();
    if (Kit::Button(ui, ui.MakeId("bypass-close"), { installR.x - 6.f - kButtonW, y, kButtonW, kButtonH }, Loc::Get("store_close"),
                    Kit::DialogButton(false)))
        Close();
}

} // namespace RivalsForge::Main

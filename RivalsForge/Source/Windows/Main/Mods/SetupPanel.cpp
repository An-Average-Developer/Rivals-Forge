#include <cmath>
#include "Core/System.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Game/GamePathService.h"
#include "UI/Material/Motion.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"

// <StackPanel x:Name="SetupPanel" MaxWidth="500">: the game wasn't found. A slanted red warning
// badge with rings pulsing out of it, the title in condensed italic capitals; Browse / Retry
// configure the Paks folder (BrowseGamePath_Click / RetryDetection_Click) and SetupHintLabel says
// why when they can't.
namespace RivalsForge::Main {

namespace {
namespace M = UI::Material;
namespace Kit = ModsKit;
constexpr float kMaxWidth = 520.f, kBadge = 64.f;
} // namespace

void ModsView::SetupPanel(UI::Ui& ui, const Rect& r) {
    const float w = std::min(kMaxWidth, r.w - 2.f * MainLayout::SideMargin);
    const float x = r.CX() - w * 0.5f;

    TextStyle title = Kit::Display(30.f, DWRITE_FONT_WEIGHT_BOLD);
    title.align = HAlign::Center;
    TextStyle text = Kit::Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    TextStyle hint = Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    TextStyle error = Kit::Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true, HAlign::Center);
    const std::wstring titleText = Kit::Upper(Loc::Get("setup_title")), bodyText = Loc::Get("setup_text"), hintText = Loc::Get("setup_hint");
    const std::wstring browseText = Loc::Get("setup_browse"), retryText = Loc::Get("setup_retry");

    const Kit::ButtonStyle browse = Kit::Primary();
    Kit::ButtonStyle retry = Kit::Ghost();
    retry.padding = browse.padding;
    retry.fontSize = browse.fontSize;
    const float buttonsH = Kit::ButtonHeight(ui, browse);

    const float titleH = Kit::LineH(ui, title);
    const float bodyH = ui.g.Measure(bodyText, text, w).h, hintH = ui.g.Measure(hintText, hint, w).h;
    const float errorH = setupHint.empty() ? 0.f : 16.f + ui.g.Measure(setupHint, error, w - 40.f).h + 14.f;
    const float total = kBadge + 22.f + titleH + 8.f + bodyH + 6.f + hintH + 26.f + buttonsH + errorH;
    float y = r.CY() - total * 0.5f;
    double& shownAt = ui.Data<double>(ui.MakeId("setup-shown"));
    if (shownAt == 0.0) shownAt = ui.Time();
    const auto pop = [&](int i) { return M::Motion::Stagger(ui, shownAt, i, 0.08f, 0.45f); };

    // The badge: rings pulse out of it, a light crosses it now and then
    {
        const float in = pop(0);
        M::Motion::Enter enter(ui, in, 0.f);
        const float t = static_cast<float>(ui.Time());
        ui.KeepAnimatingThrottled();
        const Rect badge{ r.CX() - kBadge * 0.5f, y, kBadge, kBadge };
        ui.PushScale(0.6f + 0.4f * Anim::EaseOutBack(in), badge.CX(), badge.CY());
        for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(t * 0.6f + i * 0.5f, 1.f);
            const Rect ring = badge.Inset(-30.f * k);
            Kit::StrokeSlant(ui, ring, Kit::Palette::Accent.Fade(0.5f * (1.f - k)), 1.5f, 14.f + 6.f * k);
        }
        ui.g.GlowRound(badge.Inset(8.f, 4.f), 4.f, Kit::Palette::Accent, 24.f, 0.5f);
        Kit::FillSlant(ui, badge, Kit::Palette::Accent, 14.f);
        Kit::PushSlantClip(ui, badge, 14.f);
        Kit::Halftone(ui, badge, 0.12f);
        Kit::Sheen(ui, badge, std::fmod(t * 0.35f, 2.f), 1.f);
        ui.g.PopLayer();
        Kit::DrawGlyph(ui, Kit::Glyph::Warning, badge.CX(), badge.CY(), 30.f, Colors::White);
        ui.PopTransform();
        y += kBadge + 22.f;
    }
    {
        M::Motion::Enter enter(ui, pop(1), 10.f);
        ui.g.Text(titleText, { x, y, w, titleH }, Kit::Palette::Text, title);
        y += titleH + 8.f;
        ui.g.Text(bodyText, { x, y, w, bodyH }, Kit::Palette::TextDim, text);
        y += bodyH + 6.f;
        ui.g.Text(hintText, { x, y, w, hintH }, Kit::Palette::TextFaint, hint);
        y += hintH + 26.f;
    }

    const float browseW = Kit::ButtonWidth(ui, browseText, browse), retryW = Kit::ButtonWidth(ui, retryText, retry);
    const float bx = r.CX() - (browseW + 10.f + retryW) * 0.5f;
    {
        M::Motion::Enter enter(ui, pop(2), 10.f);
        if (Kit::Button(ui, ui.MakeId("setup-browse"), { bx, y, browseW, buttonsH }, browseText, browse)) BrowseGamePath();
        if (Kit::Button(ui, ui.MakeId("setup-retry"), { bx + browseW + 10.f, y, retryW, buttonsH }, retryText, retry)) RetryDetection();
    }
    y += buttonsH;

    // SetupHintLabel: each new hint slides in, in a red slanted box
    if (!setupHint.empty()) {
        auto& shown = ui.Data<std::wstring>(ui.MakeId("setup-hint"));
        double& at = ui.Data<double>(ui.MakeId("setup-hint-at"));
        if (shown != setupHint) {
            shown = setupHint;
            at = ui.Time();
        }
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at, 0, 0.f, 0.3f), 8.f);
        const Rect box{ x, y + 16.f, w, errorH - 16.f };
        Kit::FillSlant(ui, box, Kit::Palette::Accent.Fade(0.14f), 8.f);
        Kit::EdgeStripe(ui, box, 5.f, Kit::Palette::Accent, 8.f);
        ui.g.Text(setupHint, box.Inset(Edges(20.f, 7.f)), Kit::Palette::AccentHover, error);
    }
}

void ModsView::BrowseGamePath() {
    // OpenFolderDialog { Title = "Select the MarvelGame folder  (e.g. …\MarvelRivals\MarvelGame)" }
    const std::wstring selected = System::PickFolder(state.Owner(), Loc::Get("setup_browse_title"));
    if (selected.empty()) return;

    const auto paksPath = Services::GamePath::ResolvePaksFromSelection(selected);
    if (!paksPath) {
        setupHint = Loc::Get("setup_paks_not_found");
        return;
    }
    try {
        Services::Mods::FileOps::MakeDirectory(*paksPath);
        state.Service().SetGamePath(*paksPath);
    } catch (const Services::Mods::ModError& e) {
        // The C# had no try here (the app would have closed); the error is shown instead
        Log::Error(L"Setting the game path: " + e.Message());
        Dialogs::Message(e.Message(), Loc::Get("dialog_error"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        return;
    }
    setupHint.clear();
    state.Reload();
    state.SetStatus(Loc::Fmt("status_path_configured", *paksPath));
}

void ModsView::RetryDetection() {
    if (const auto found = Services::GamePath::TryDetect()) {
        try {
            state.Service().SetGamePath(*found);
        } catch (const Services::Mods::ModError& e) {
            Log::Error(L"Setting the game path: " + e.Message());
            Dialogs::Message(e.Message(), Loc::Get("dialog_error"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
            return;
        }
        setupHint.clear();
        state.Reload();
        state.SetStatus(Loc::Fmt("status_game_detected", *found));
    } else {
        setupHint = Loc::Get("setup_detect_failed");
    }
}

} // namespace RivalsForge::Main

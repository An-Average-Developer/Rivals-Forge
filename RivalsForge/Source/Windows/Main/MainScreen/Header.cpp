#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"
#include "Core/Text.h"
#include "Windows/Main/Overlays/BypassOverlay.h"
#include "Windows/Main/Overlays/HeaderPopups.h"
#include "Windows/Main/Overlays/ThemeTransition.h"
#include "Services/App/VersionCompare.h"
#include "Services/Game/SignatureBypass.h"
#include "Windows/Main/Store/StoreAccountOverlay.h"
#include "Windows/Main/Store/StoreView.h"

// The header (was Grid.Row="1": "Installed Mods" and SummaryLabel, "＋ Add Mod"): two tabs in big
// condensed italic capitals, the installed mods and the store, a red slash under the one shown;
// under them the summary as three counters (mods, enabled, heroes) that count up to their numbers
// — or "Game not found" in red — or, in the store, which store it browses (Nexus Mods /
// GameBanana) and, for Nexus, the account. On the right
// the language flag and "＋ Add Mod", a light crossing it now and then. Everything comes in once at
// startup.
namespace RivalsForge::Main::Header {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

double& StartedAt() {
    static double t = -1.0;
    return t;
}

float Pop(UI::Ui& ui, int index) { return M::Motion::Stagger(ui, StartedAt() + 0.1, index, 0.07f, 0.45f); }

// The current language's flag in a slanted frame; opens the language popup
void LanguageButton(UI::Ui& ui, MainWindow& main, const Rect& r) {
    const UI::Id id = ui.MakeId("hdr-lang");
    const UI::Interaction it = ui.Interact(id, r);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, fb.hover));
    Kit::StrokeSlant(ui, r, Lerp(Kit::Palette::Line, Kit::Palette::TextDim, fb.hover), 1.f);
    const float s = 1.f + 0.08f * fb.hover - 0.06f * fb.press;
    LanguagePopup::Banner(ui, { r.CX() - 13.f * s, r.CY() - 8.5f * s, 26.f * s, 17.f * s }, LanguagePopup::Flag(Loc::CurrentLang()), fb.hover, 0.f);
    ui.Tooltip(id, Loc::Get("language_tooltip"));
    if (it.clicked) main.Languages().Toggle(r);
}

// A sun (in the dark theme) or a moon (in the light one) in a slanted frame: switches the theme,
// the icon spinning while the new one sweeps in
void ThemeButton(UI::Ui& ui, MainWindow& main, const Rect& r) {
    const UI::Id id = ui.MakeId("hdr-theme");
    const UI::Interaction it = ui.Interact(id, r, !main.Theme().Animating());
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, fb.hover));
    Kit::StrokeSlant(ui, r, Lerp(Kit::Palette::Line, Kit::Palette::TextDim, fb.hover), 1.f);
    const bool light = Kit::IsLight();
    const Color color = Lerp(light ? Hex(0x5B6CD6) : Kit::Palette::Gold, Kit::Palette::Strong, 0.25f * fb.hover);
    const float s = 1.f + 0.1f * fb.hover - 0.08f * fb.press;
    const float angle = main.Theme().IconAngle(ui.Time()) + 20.f * fb.hover;
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(r.CX(), r.CY())));
    M::DrawIcon(ui.g, light ? M::Icon::WeatherNight : M::Icon::WeatherSunny, r.CX(), r.CY(), 20.f * s, color);
    ui.g.PopTransform();
    ui.Tooltip(id, Loc::Get(light ? "theme_dark_tooltip" : "theme_light_tooltip"));
    if (it.clicked) main.ToggleTheme();
}

// One counter: the number (counting up) over its label, in a slanted dark chip; returns its width
float Stat(UI::Ui& ui, float x, float y, float h, const char* idName, int value, const std::wstring& label, Color numberColor) {
    TextStyle number = Kit::Display(19.f, DWRITE_FONT_WEIGHT_BOLD);
    number.valign = VAlign::Center;
    TextStyle caption = Kit::Display(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    caption.valign = VAlign::Center;
    const float shown = Kit::Count(ui, ui.MakeId(idName), static_cast<float>(value));
    const std::wstring digits = std::to_wstring(static_cast<int>(std::lround(shown)));
    const std::wstring text = Kit::Upper(label);
    const float nw = std::ceil(ui.g.Measure(std::to_wstring(value), number).w) + 4.f;
    const float lw = std::ceil(ui.g.Measure(text, caption).w);
    const float w = 14.f + nw + 6.f + lw + 16.f;
    const Rect chip{ x, y, w, h };
    const float pop = Kit::Changed(ui, ui.MakeId(idName), value, 0.5f);
    Kit::FillSlant(ui, chip, Lerp(Kit::Palette::Ghost, numberColor.Fade(0.25f), pop), 7.f);
    ui.g.Text(digits, { x + 14.f, y, nw, h }, numberColor, number);
    ui.g.Text(text, { x + 14.f + nw + 6.f, y + 1.f, lw + 2.f, h }, Kit::Palette::TextDim, caption);
    return w;
}

// The game, right of the tabs: the installations (Steam | Epic) as slanted segments, a highlight
// sliding to the one in use (a click moves the mods to another), and the green PLAY button with a
// light crossing it now and then; returns their left edge
float GameControls(UI::Ui& ui, MainWindow& main, float right, float y) {
    MainState& state = main.State();
    const float h = 34.f;
    const auto current = state.Current();

    // Play
    Kit::ButtonStyle play = Kit::Primary();
    play.background = Hex(0x2FA862);
    play.hoverBackground = Hex(0x3CC777);
    play.pressedBackground = Hex(0x23864D);
    play.fontSize = 15.f;
    const std::wstring playText = Loc::Get("header_play");
    const float playW = Kit::ButtonWidth(ui, playText, play) + 22.f;
    const Rect playR{ right - playW, y, playW, h };
    const UI::Id playId = ui.MakeId("hdr-play");
    if (Kit::Button(ui, playId, playR, L"     " + playText, play, state.IsConfigured())) main.Mods().PlayGame();
    Kit::DrawGlyph(ui, Kit::Glyph::Play, playR.x + 26.f, playR.CY(), 14.f, Colors::White.Fade(state.IsConfigured() ? 1.f : 0.4f));
    ui.Tooltip(playId, Loc::Fmt("header_play_tooltip", Services::GamePath::StoreName(current)));

    // The installations
    if (state.installations.empty()) return playR.x;
    TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    std::vector<float> widths;
    float total = 0.f;
    std::vector<std::wstring> labels;
    for (const auto& i : state.installations) {
        labels.push_back(Kit::Upper(i.store == Services::GamePath::Store::Epic ? L"Epic" : Services::GamePath::StoreName(i)));
        widths.push_back(std::ceil(ui.g.Measure(labels.back(), t).w) + 34.f);
        total += widths.back();
    }
    float x = playR.x - 12.f - total - 4.f * static_cast<float>(widths.size() - 1);
    int chosen = -1;
    float chosenX = x, cx = x;
    for (size_t i = 0; i < widths.size(); ++i) {
        if (state.IsConfigured() && Text::Equals(state.installations[i].paks, current.paks, true)) {
            chosen = static_cast<int>(i);
            chosenX = cx;
        }
        cx += widths[i] + 4.f;
    }
    Kit::FillSlant(ui, { x - 4.f, y, total + 4.f * static_cast<float>(widths.size() - 1) + 8.f, h }, Kit::Palette::Ghost, 8.f);
    if (chosen >= 0) {
        const float hx = ui.Animate(ui.MakeId("hdr-store-x"), chosenX, 16.f);
        const float hw = ui.Animate(ui.MakeId("hdr-store-w"), widths[static_cast<size_t>(chosen)], 16.f);
        if (std::fabs(hx - chosenX) > 0.3f) ui.KeepAnimating();
        Kit::FillSlant(ui, { hx, y + 3.f, hw, h - 6.f }, Kit::Palette::GhostHover, 7.f);
        Kit::EdgeStripe(ui, { hx, y + 3.f, hw, h - 6.f }, 3.f, Kit::Palette::Accent, 7.f);
    }
    cx = x;
    for (size_t i = 0; i < widths.size(); ++i) {
        const Rect seg{ cx, y, widths[i], h };
        cx += widths[i] + 4.f;
        const UI::Id id = ui.MakeId("hdr-store", static_cast<int64_t>(i));
        const bool on = static_cast<int>(i) == chosen;
        const UI::Interaction it = ui.Interact(id, seg, !state.busy);
        if (it.hovered && !on) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
        ui.g.Text(labels[i], seg, on ? Kit::Palette::Text : Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, hover), t);
        ui.Tooltip(id, Loc::Fmt(on ? "header_store_current" : "header_store_switch", Services::GamePath::StoreName(state.installations[i]),
                                state.installations[i].paks));
        if (it.clicked && !on) main.Mods().SwitchInstallation(state.installations[i]);
    }
    return x - 4.f;
}

// The UTOC Signature Bypass Patch of the installation in use: "Patch OK" in green, or "Patch
// missing" in red, breathing; opens its card
void BypassChip(UI::Ui& ui, MainWindow& main, float x, float y, float h) {
    MainState& state = main.State();
    if (!state.IsConfigured()) return;
    const bool installed = Services::Game::SignatureBypass::IsInstalled(state.Current());
    const Color color = installed ? Kit::Palette::Enabled : Kit::Palette::Danger;
    TextStyle t = Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.valign = VAlign::Center;
    const std::wstring label = Kit::Upper(Loc::Get(installed ? "bypass_chip_ok" : "bypass_chip_missing"));
    const Rect r{ x, y, std::ceil(ui.g.Measure(label, t).w) + 46.f, h };
    const UI::Id id = ui.MakeId("hdr-bypass");
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
    float glow = 0.f;
    if (!installed) {
        glow = 0.5f + 0.5f * std::sin(static_cast<float>(ui.Time()) * 3.f);
        ui.KeepAnimatingThrottled();
    }
    Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, color.Fade(0.22f), std::max(hover, 0.5f * glow)), 7.f);
    Kit::EdgeStripe(ui, r, 4.f, color, 7.f);
    Kit::DrawGlyph(ui, installed ? Kit::Glyph::Check : Kit::Glyph::Warning, r.x + 20.f, r.CY(), 13.f, color);
    ui.g.Text(label, { r.x + 32.f, r.y, r.w - 36.f, r.h }, Lerp(installed ? Kit::Palette::TextDim : color, Kit::Palette::Strong, hover), t);
    ui.Tooltip(id, Loc::Get("bypass_chip_tooltip"));
    if (it.clicked) main.Bypass().Show(false);
}

// Which store the Mod Store browses: a slanted chip per store, the chosen one lit with a highlight
// sliding between them; returns their width
float SourceSwitch(UI::Ui& ui, MainWindow& main, float x, float y, float h) {
    namespace Store = Services::Store;
    StoreView& store = main.Store();
    const Store::Source sources[] = { Store::Source::Nexus, Store::Source::GameBanana };
    TextStyle t = Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    float widths[2], chosenX = x, cx = x;
    for (int i = 0; i < 2; ++i) {
        widths[i] = std::ceil(ui.g.Measure(Kit::Upper(Store::SourceName(sources[i])), t).w) + 30.f;
        if (sources[i] == store.GetSource()) chosenX = cx;
        cx += widths[i] + 3.f;
    }
    const float chosenW = widths[store.GetSource() == Store::Source::GameBanana ? 1 : 0];
    const float hx = ui.Animate(ui.MakeId("hdr-source-x"), chosenX, 18.f);
    const float hw = ui.Animate(ui.MakeId("hdr-source-w"), chosenW, 18.f);
    if (std::fabs(hx - chosenX) > 0.3f || std::fabs(hw - chosenW) > 0.3f) ui.KeepAnimating();
    Kit::FillSlant(ui, { hx, y, hw, h }, Kit::Palette::Accent, 7.f);
    cx = x;
    for (int i = 0; i < 2; ++i) {
        const Rect r{ cx, y, widths[i], h };
        const bool on = sources[i] == store.GetSource();
        const UI::Id id = ui.MakeId("hdr-source", i);
        const UI::Interaction it = ui.Interact(id, r, !store.Busy());
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
        if (!on) Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, Kit::Palette::GhostHover, hover), 7.f);
        ui.g.Text(Kit::Upper(Store::SourceName(sources[i])), r, on ? Colors::White : Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, hover), t);
        ui.Tooltip(id, Loc::Fmt("store_source_tooltip", Store::SourceName(sources[i])));
        if (it.clicked && !on) store.SetSource(sources[i]);
        cx += widths[i] + 3.f;
    }
    return cx - 3.f - x;
}

// The Nexus account in a slanted chip: who's signed in (and Premium), or "Sign in"; opens its card;
// returns its width
float AccountChip(UI::Ui& ui, MainWindow& main, Rect r) {
    const StoreView::Account& account = main.Store().GetAccount();
    std::wstring text;
    Color color = Kit::Palette::TextDim;
    if (account.checking) text = Loc::Get("account_checking");
    else if (account.info) {
        text = Loc::Fmt("store_chip_signed_in", account.info->name);
        if (account.info->premium) text += L"  ·  PREMIUM";
        color = account.info->premium ? Kit::Palette::Gold : Kit::Palette::Enabled;
    } else {
        text = Loc::Get("store_chip_sign_in");
        color = Kit::Palette::Info;
    }
    TextStyle t = Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.valign = VAlign::Center;
    const std::wstring label = Kit::Upper(L"Nexus Mods  ·  " + text);
    r.w = std::ceil(ui.g.Measure(label, t).w) + 50.f;
    const UI::Id id = ui.MakeId("hdr-account");
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
    Kit::FillSlant(ui, r, Lerp(Kit::Palette::Ghost, color.Fade(0.2f), hover), 7.f);
    Kit::EdgeStripe(ui, r, 4.f, color, 7.f);
    Kit::DrawGlyph(ui, account.info ? Kit::Glyph::Check : Kit::Glyph::Person, r.x + 20.f, r.CY(), 13.f, color);
    ui.g.Text(label, { r.x + 32.f, r.y, r.w - 36.f, r.h }, Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, hover), t);
    if (account.checking) ui.KeepAnimating();
    if (it.clicked) main.StoreAccount().Show(false);
    return r.w;
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& header) {
    if (StartedAt() < 0.0) StartedAt() = ui.Time();
    MainState& state = main.State();
    // AddModBtn.IsEnabled = configured (and not while ZIPs are being installed)
    const bool configured = state.IsConfigured() && !state.busy;
    const Rect inner = header.Inset(Edges(MainLayout::SideMargin, 14.f, MainLayout::SideMargin, 12.f));

    // Right: language and "＋ Add Mod", bottom aligned
    const Kit::ButtonStyle primary = Kit::Primary();
    const std::wstring addMod = Loc::Get("header_add_mod");
    const float h = Kit::ButtonHeight(ui, primary);
    const float addW = Kit::ButtonWidth(ui, addMod, primary);
    const Rect addR{ inner.R() - addW, inner.B() - h, addW, h };
    const Rect langR{ addR.x - 10.f - (h + 14.f), addR.y, h + 14.f, h };
    const Rect themeR{ langR.x - 6.f - (h + 14.f), addR.y, h + 14.f, h };
    {
        M::Motion::Enter enter(ui, Pop(ui, 3), 0.f, 24.f);
        if (Kit::Button(ui, ui.MakeId("hdr-add-mod"), addR, addMod, primary, configured)) main.Mods().AddMod();   // AddMod_Click
    }
    {
        M::Motion::Enter enter(ui, Pop(ui, 2), 0.f, 24.f);
        LanguageButton(ui, main, langR);
    }
    {
        M::Motion::Enter enter(ui, Pop(ui, 2), 0.f, 24.f);
        ThemeButton(ui, main, themeR);
    }

    // Right of the tabs: the installations and Play
    float controlsX = inner.R();
    {
        M::Motion::Enter enter(ui, Pop(ui, 4), 0.f, 24.f);
        controlsX = GameControls(ui, main, inner.R(), inner.y - 4.f);
    }

    // Left: the three tabs (the installed mods, the store, the updates), a red slash sliding under
    // the one shown; they shrink when the window is too narrow for them next to the game controls
    TextStyle title = Kit::Display(28.f, DWRITE_FONT_WEIGHT_BOLD);
    const float titleH = Kit::LineH(ui, title);
    {
        M::Motion::Enter enter(ui, Pop(ui, 0), 0.f, -24.f);
        constexpr int kTabs = 3;
        const char* keys[kTabs] = { "header_title", "header_store", "header_updates" };
        std::wstring texts[kTabs];
        float total = 0.f;
        for (int i = 0; i < kTabs; ++i) {
            texts[i] = Kit::Upper(Loc::Get(keys[i]));
            total += std::ceil(ui.g.Measure(texts[i], title).w) + 30.f;
        }
        const float room = controlsX - 24.f - inner.x;
        if (total - 30.f > room) title.size = std::max(18.f, title.size * room / (total - 30.f));
        const float gap = 30.f * title.size / 28.f;
        // The changelog belongs to the updates' tab
        const int shown = state.view == MainState::View::Changelog ? 2 : static_cast<int>(state.view);
        float x = inner.x, slashX = inner.x, slashW = 0.f;
        for (int i = 0; i < kTabs; ++i) {
            const std::wstring& text = texts[i];
            const float w = std::ceil(ui.g.Measure(text, title).w);
            const Rect tab{ x, inner.y - 4.f + (titleH - Kit::LineH(ui, title)) * 0.5f, w + 4.f, Kit::LineH(ui, title) };
            const UI::Id id = ui.MakeId("hdr-tab", i);
            const UI::Interaction it = ui.Interact(id, tab.Inset(-6.f, -2.f));
            const bool on = shown == i;
            if (it.hovered && !(on && state.view != MainState::View::Changelog)) ui.SetCursor(IDC_HAND);
            const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            const float sel = ui.Tween(UI::HashId(id, 0x7A), on ? 1.f : 0.f, 0.25f);
            ui.g.Text(text, tab, Lerp(Lerp(Kit::Palette::TextFaint, Kit::Palette::TextDim, hover), Kit::Palette::Text, sel), title);
            // Updates: a green dot breathing next to it when a newer version is out
            if (i == 2 && state.updates.available) {
                const float k = 0.5f + 0.5f * std::sin(static_cast<float>(ui.Time()) * 3.f);
                ui.KeepAnimatingThrottled();
                const float dx = tab.R() + 6.f, dy = tab.y + 8.f;
                ui.g.FillEllipse(dx, dy, 4.f + 3.f * k, 4.f + 3.f * k, Kit::Palette::Enabled.Fade(0.3f * (1.f - k)));
                ui.g.FillEllipse(dx, dy, 4.f, 4.f, Kit::Palette::Enabled);
                ui.Tooltip(id, Loc::Fmt("status_update_tooltip", Versions::WithV(state.updates.latest)));
            }
            if (on) {
                slashX = x;
                slashW = w;
            }
            if (it.clicked && static_cast<int>(state.view) != i) {
                if (i == 2) state.ShowUpdates();
                else state.view = static_cast<MainState::View>(i);
            }
            x += w + gap;
        }
        const float sx = ui.Animate(ui.MakeId("hdr-slash-x"), slashX, 16.f);
        const float sw = ui.Animate(ui.MakeId("hdr-slash-w"), slashW * 0.45f, 16.f);
        if (std::fabs(sx - slashX) > 0.3f || std::fabs(sw - slashW * 0.45f) > 0.3f) ui.KeepAnimating();
        const float grow = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - StartedAt() - 0.25) / 0.6)));
        if (grow < 1.f) ui.KeepAnimating();
        Kit::FillSlant(ui, { sx + 2.f, inner.y - 4.f + titleH - 2.f, sw * grow, 4.f }, Kit::Palette::Accent, 3.f);
    }

    // In the store: which store, and the Nexus account, instead of the counters
    if (state.view == MainState::View::Store) {
        M::Motion::Enter enter(ui, Pop(ui, 1), 8.f);
        float x = inner.x + SourceSwitch(ui, main, inner.x, inner.B() - 28.f, 28.f) + 12.f;
        if (main.Store().GetSource() == Services::Store::Source::Nexus) x += AccountChip(ui, main, { x, inner.B() - 28.f, 0.f, 28.f }) + 8.f;
        BypassChip(ui, main, x, inner.B() - 28.f, 28.f);
        return;
    }

    // Under it: the counters, or "Game not found"
    M::Motion::Enter enter(ui, Pop(ui, 1), 8.f);
    const float chipH = 28.f, y = inner.B() - chipH;
    if (!state.IsConfigured()) {
        TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        t.valign = VAlign::Center;
        const std::wstring text = Kit::Upper(Loc::Get("summary_game_not_found"));
        const Rect chip{ inner.x, y, std::ceil(ui.g.Measure(text, t).w) + 34.f, chipH };
        Kit::FillSlant(ui, chip, Kit::Palette::Accent.Fade(0.2f), 7.f);
        Kit::StrokeSlant(ui, chip, Kit::Palette::Accent.Fade(0.7f), 1.f, 7.f);
        ui.g.Text(text, chip.Inset(Edges(17.f, 0.f)), Kit::Palette::AccentHover, t);
        return;
    }
    int enabled = 0, heroes = 0;
    for (const auto& mod : state.Service().Mods()) enabled += mod.isEnabled ? 1 : 0;
    for (const auto& group : state.Groups()) heroes += group.hero.empty() ? 0 : 1;
    const int total = state.TotalMods();
    float x = inner.x;
    x += Stat(ui, x, y, chipH, "stat-mods", total, Loc::Get(total == 1 ? "stat_mods_one" : "stat_mods_many"), Kit::Palette::Text) + 8.f;
    x += Stat(ui, x, y, chipH, "stat-enabled", enabled, Loc::Get(enabled == 1 ? "stat_enabled_one" : "stat_enabled_many"), Kit::Palette::Enabled) + 8.f;
    x += Stat(ui, x, y, chipH, "stat-heroes", heroes, Loc::Get(heroes == 1 ? "stat_heroes_one" : "stat_heroes_many"), Kit::Palette::Gold) + 8.f;
    BypassChip(ui, main, x, y, chipH);
}

} // namespace RivalsForge::Main::Header

#include "Windows/Main/MainWindow.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/Privacy.h"
#include "Services/App/VersionCompare.h"
#include "Services/Mods/ModPackage.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"
#include "Windows/Main/Overlays/AddModOverlay.h"
#include "Windows/Main/Store/StoreAccountOverlay.h"
#include "Windows/Main/Store/StoreModOverlay.h"
#include "Windows/Main/Store/StoreView.h"
#include "Windows/Main/Updates/ChangelogView.h"
#include "Windows/Main/Updates/UpdatesView.h"
#include "Windows/Main/Overlays/BypassOverlay.h"
#include "Windows/Main/Overlays/DeleteOverlay.h"
#include "Windows/Main/Overlays/HeaderPopups.h"
#include "Windows/Main/Overlays/HeroPickerOverlay.h"
#include "Windows/Main/Overlays/InputOverlay.h"
#include "Windows/Main/Overlays/MessageOverlay.h"
#include "Windows/Main/Overlays/ThemeTransition.h"

namespace RivalsForge::Main {

namespace {
MainWindow* instance = nullptr;
namespace Kit = ModsKit;
}

MainWindow* MainWindow::Instance() { return instance; }

MainWindow::MainWindow() {
    instance = this;
    languages = std::make_unique<LanguagePopup>(*this);
    messages = std::make_unique<MessageOverlay>(*this);
    deleteCard = std::make_unique<DeleteOverlay>(*this);
    bypass = std::make_unique<BypassOverlay>(*this);
    input = std::make_unique<InputOverlay>(*this);
    addMod = std::make_unique<AddModOverlay>(*this);
    heroPicker = std::make_unique<HeroPickerOverlay>(*this);
    storeCard = std::make_unique<StoreModOverlay>(*this);
    storeAccount = std::make_unique<StoreAccountOverlay>(*this);
    themeTransition = std::make_unique<ThemeTransition>();
    // The message card goes on top: the errors of a card's action show over it
    overlays = { languages.get(), storeCard.get(), storeAccount.get(), input.get(), addMod.get(), heroPicker.get(), deleteCard.get(), bypass.get(), messages.get() };
}

MainWindow::~MainWindow() {
    if (instance == this) instance = nullptr;
}

bool MainWindow::Open() {
    UI::Window::Options o;
    o.title = L"Rivals Forge";   // Title (taskbar / Alt+Tab)
    // WindowStyle="None" AllowsTransparency="True" ResizeMode="CanResize": no Windows frame, the
    // rounded RootBorder drawn on a transparent window, resizable from every edge
    o.style = WS_OVERLAPPEDWINDOW;
    o.transparent = true;
    o.customFrame = true;
    // The default size, kept inside the work area (94% of it at most) on smaller screens
    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    const float dips = 96.f / static_cast<float>(GetDpiForSystem());
    const float maxW = (area.right - area.left) * dips * 0.94f, maxH = (area.bottom - area.top) * dips * 0.94f;
    o.width = static_cast<int>(std::max(MainLayout::MinWidth, std::min(MainLayout::WindowWidth, maxW)));
    o.height = static_cast<int>(std::max(MainLayout::MinHeight, std::min(MainLayout::WindowHeight, maxH)));
    o.minWidth = static_cast<int>(MainLayout::MinWidth);
    o.minHeight = static_cast<int>(MainLayout::MinHeight);
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    // Presentation mode: nothing personal on screen (Config::PresentationMode)
    if (Services::Privacy::Active()) Graphics::SetTextFilter(&Services::Privacy::Mask);
    state = std::make_unique<MainState>(hwnd);   // new ModService(): loads data.json, detects the game
    mods = std::make_unique<ModsView>(*this);
    store = std::make_unique<StoreView>(*this);
    updates = std::make_unique<UpdatesView>(*this);
    changelog = std::make_unique<ChangelogView>(*this);
    // A newer version found at startup (or by a later check): offer to see it
    state->onUpdateFound = [this] {
        Dialogs::Message(Loc::Fmt("updates_found_message", Versions::WithV(state->updates.latest), Versions::WithV(Versions::Current())),
                         Loc::Get("updates_found_title"), Dialogs::MessageButtons::YesNo, Dialogs::MessageIcon::Information,
                         [this](Dialogs::MessageResult r) {
                             if (r == Dialogs::MessageResult::Yes) state->ShowUpdates(true);
                         });
    };
    Async::Run([] { Services::Mods::ModPackage::RemoveLeftovers(); });
    ui.tooltipRenderer = [](Graphics& gr, const std::wstring& text, float mx, float my, float age, float left) {
        Kit::DrawTooltip(gr, text, mx, my, age, left);
    };

    // Centred on the work area (the C# window opened where Windows put it; centring is clearer)
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT r{};
    GetWindowRect(hwnd, &r);
    MoveTo(work.left + (work.right - work.left - (r.right - r.left)) / 2, work.top + (work.bottom - work.top - (r.bottom - r.top)) / 2);

    // data.json couldn't be read: say so (the C# just started empty) and where the copy is
    if (!state->Service().LoadBackup().empty())
        Dialogs::Message(Loc::Fmt("data_corrupt_message", state->Service().LoadBackup()), Loc::Get("data_corrupt_title"),
                         Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Warning);

    // Started by an nxm:// link (the site's "Mod Manager Download")
    if (!startupLink.empty())
        Async::After(600, [this, url = startupLink] { store->HandleNxm(url); });

#ifdef _DEBUG
    // Development aid: RF_OPEN=<card> opens one of the in-window cards at startup (screenshots):
    // input, heroes, message, languages, maximized, smallest; updates, changelog, manual,
    // fakeupdate / fakeupdate:install (a pretend 1.0.1), toggletheme (saves theme.cfg);
    // install:<zip>|<zip>... runs DoAddMod on those files, as if picked in the dialog;
    // addnow:<zip> installs it at once as "Test mod"; toggle:<mod name> enables / disables a mod;
    // filter:<hero id> (or "-": no hero) filters the list as the hero strip does
    wchar_t card[2048]{};
    if (GetEnvironmentVariableW(L"RF_OPEN", card, 2048) > 0) {
        const std::wstring name = card;
        Async::After(300, [this, name] {
            if (name == L"store") ShowStore();
            else if (name == L"toggletheme") ToggleTheme();
            else if (name.rfind(L"filter:", 0) == 0) state->filter = name.substr(7);   // a hero id, "-" for no hero
            else if (name.rfind(L"addnow:", 0) == 0) {
                // Installs that ZIP straight away (no cards), as "Test mod"
                try {
                    state->Service().AddMod(name.substr(7), L"Test mod", std::nullopt);
                } catch (const Services::Mods::ModError& e) {
                    Dialogs::Message(e.Message(), Loc::Get("dialog_error"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
                }
            } else if (name.rfind(L"toggle:", 0) == 0) {
                // Enables / disables the mod with that name
                for (const auto& mod : state->Service().Mods())
                    if (mod.name == name.substr(7)) {
                        const std::wstring id = mod.id;
                        if (mod.isEnabled) state->Service().DisableMod(id);
                        else state->Service().EnableMod(id);
                        break;
                    }
            }
            else if (name == L"updates") state->ShowUpdates();
            else if (name == L"changelog") state->view = MainState::View::Changelog;
            else if (name == L"manual") {
                state->ShowUpdates();
                updates->OpenManual();
            } else if (name.rfind(L"fakeupdate", 0) == 0) {
                // A pretend update to 1.0.1 with the embedded changelog (":install" opens its installation)
                auto& u = state->updates;
                u.checked = u.reachable = u.available = true;
                u.latest = L"1.0.1";
                u.fileName = L"RivalsForge.exe";
                u.fileSize = L"7.0 MB";
                u.changelog = std::wstring(L"1.0.1\n## Additions\n- Loose-file mods: mods that replace the game's own files, like the skip intro videos\n"
                                           L"- Mod tags: see at a glance what each mod changes (mesh, texture, audio, video...)\n\n"
                                           L"## Bug Fixes\n- The No character filter now shows only the mods with no hero\n\n") +
                              Services::Changelog::EmbeddedText();
                state->ShowUpdates(name == L"fakeupdate:install");
            }
            else if (name == L"bypass") bypass->Show(false);
            else if (name == L"account") {
                ShowStore();
                storeAccount->Show(false);
            } else if (name.rfind(L"storemod:", 0) == 0) {
                ShowStore();
                Services::Store::StoreMod m;
                m.id = _wtoi64(name.substr(9).c_str());
                m.name = L"Nexus " + name.substr(9);
                storeCard->Show(m);
            } else if (name == L"input") input->Show(Loc::Get("addmod_name_title"), Loc::Get("addmod_name_message"), L"Bunny Venom", {});
            else if (name == L"heroes" && !state->Service().Mods().empty()) mods->ChangeHero(state->Service().Mods().front().id);
            else if (name == L"message")
                Dialogs::Message(Loc::Get("not_configured_message"), Loc::Get("not_configured_title"), Dialogs::MessageButtons::Ok,
                                 Dialogs::MessageIcon::Warning);
            else if (name == L"delete" && !state->Service().Mods().empty()) mods->DeleteMod(state->Service().Mods().front().id);
            else if (name == L"languages") languages->Toggle({ g.Width() - 260.f, 70.f, 34.f, 34.f });
            else if (name == L"maximized") ToggleMaximize();
            else if (name == L"smallest") Resize(static_cast<int>(MainLayout::MinWidth), static_cast<int>(MainLayout::MinHeight));
            else if (name.rfind(L"install:", 0) == 0) mods->AddFiles(Text::Split(name.substr(8), L'|', true));
        });
    }
    // RF_THEME=light / dark: that theme for this run (not saved)
    wchar_t theme[16]{};
    if (GetEnvironmentVariableW(L"RF_THEME", theme, 16) > 0) Kit::ApplyTheme(std::wstring(theme) == L"light");
    wchar_t shot[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"RF_SCREENSHOT", shot, MAX_PATH) > 0) {
        screenshotPath = shot;
        Show(SW_SHOWNOACTIVATE);
        wchar_t delay[16]{};
        const unsigned ms = GetEnvironmentVariableW(L"RF_SCREENSHOT_DELAY", delay, 16) > 0 ? static_cast<unsigned>(_wtoi(delay)) : 1500u;
        Async::After(ms, [this] {
            screenshotDue = true;
            Invalidate();
        });
        return true;
    }
#endif
    Show(SW_SHOW);
    Activate();
    // Looks for a newer version once the window is up
    Async::After(1200, [this] { state->CheckForUpdates(true); });
    return true;
}

void MainWindow::ToggleTheme() { themeTransition->Request(); }

bool MainWindow::IsCaption(float x, float y) const { return layout.Caption().Contains(x, y); }

bool MainWindow::OnMessage(UINT msg, WPARAM, LPARAM lp, LRESULT& result) {
    if (msg != WM_COPYDATA) return false;
    const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lp);
    if (!data || data->dwData != kNxmMessage || !data->lpData) return false;
    const std::wstring url(static_cast<const wchar_t*>(data->lpData), data->cbData / sizeof(wchar_t));
    // Handled after this message returns (it may open cards and start a download)
    Async::Post([this, url] { store->HandleNxm(url); });
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    result = TRUE;
    return true;
}

void MainWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
#ifdef _DEBUG
    if (screenshotDue) {
        screenshotDue = false;
        auto bitmap = g.CreateWindowBitmap();
        g.BeginOffscreen(bitmap.Get());
        g.Clear(Colors::Transparent);
        DrawFrame(screen);
        themeTransition->Draw(g, screen, ui.Time());
        g.EndOffscreen();
        g.SavePng(bitmap.Get(), screenshotPath);
        Close();
        return;
    }
#endif
    if (themeTransition->Pending()) {
        // The window in the old theme, kept to sweep away; then the theme switches
        auto snapshot = g.CreateWindowBitmap();
        if (snapshot) {
            g.BeginOffscreen(snapshot.Get());
            g.Clear(Colors::Transparent);
            DrawFrame(screen);
            g.EndOffscreen();
            g.DrawBitmap(snapshot.Get(), screen);
        } else {
            DrawFrame(screen);
        }
        themeTransition->Begin(snapshot, ui.Time());
        ui.KeepAnimating();
        return;
    }
    DrawFrame(screen);
    themeTransition->Draw(g, screen, ui.Time());
    if (themeTransition->Animating()) ui.KeepAnimating();
}

// Behind the title bar and the header: a red light from the top right, two big slanted bands
// drifting very slowly and the halftone, fading towards the list
void MainWindow::Backdrop(const Rect& screen) {
    const Rect top{ screen.x, screen.y, screen.w, layout.separator.y - screen.y };
    const float t = static_cast<float>(ui.Time());
    g.FillRadial({ screen.R() - 520.f, screen.y - 260.f, 760.f, 460.f }, { { 0.f, Kit::Palette::Accent.Fade(0.16f) }, { 1.f, Kit::Palette::Accent.Fade(0.f) } });
    g.PushClip(top);
    const float drift = std::sin(t * 0.25f) * 14.f;
    Kit::FillSlant(ui, { screen.R() - 330.f + drift, top.y, 120.f, top.h }, Kit::Palette::Shade.Fade(Kit::IsLight() ? 0.03f : 0.018f), 60.f);
    Kit::FillSlant(ui, { screen.R() - 190.f + drift * 0.6f, top.y, 46.f, top.h }, Kit::Palette::Accent.Fade(0.06f), 60.f);
    const Rect dots{ screen.R() - 420.f, top.y, 420.f, top.h };
    Kit::Halftone(ui, dots, 0.04f);
    g.FillLinear(dots, 0.f, { { 0.f, Kit::Palette::Window }, { 0.7f, Kit::Palette::Window.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.PopClip();
    ui.KeepAnimatingThrottled();
}

void MainWindow::DrawFrame(const Rect& screen) {
    layout = MainLayout::Compute(screen);
    UI::ScrollArea::SetMaterialColors(UI::Material::P().scrollBarForeground, UI::Material::P().foreground);

    // An open card or popup is modal: what's behind it doesn't see the mouse, and a text box behind
    // it loses the keyboard when it opens
    const bool modal = std::any_of(overlays.begin(), overlays.end(), [](const Overlay* o) { return o->Visible(); });
    if (modal && !modalShown) ui.ClearFocus();
    modalShown = modal;

    // RootBorder: CornerRadius 10 (0 while maximized, Window_StateChanged), #0D0D0D, 1 px #222
    const float radius = IsMaximized() ? 0.f : MainLayout::Radius;
    if (radius > 0.f) g.PushRoundClip(screen, radius);
    g.FillRect(screen, Kit::Palette::Window);
    Backdrop(screen);

    ui.SetInputMuted(modal);
    TitleBar::Draw(ui, *this, layout.titleBar);
    Header::Draw(ui, *this, layout.header);
    // The separator: a red line fading out to the right
    g.FillLinear(layout.separator, 0.f, { { 0.f, Kit::Palette::Accent.Fade(0.8f) }, { 0.35f, Kit::Palette::Line }, { 1.f, Kit::Palette::Line } },
                 { 0.f, 0.f }, { 1.f, 0.f });
    // The installed mods, the store, the updates or the changelog, cross-fading: the one leaving
    // slides out to the left, the one coming in slides in from the right
    {
        const int shown = static_cast<int>(state->view);
        const bool muted = ui.InputMuted();
        for (int i = 0; i < 4; ++i) {
            const float k = Anim::EaseInOutCubic(ui.Tween(ui.MakeId("view", i), shown == i ? 1.f : 0.f, 0.3f));
            if (k > 0.f && k < 1.f) ui.KeepAnimating();
            if (k <= 0.001f) continue;
            ui.SetInputMuted(muted || shown != i);
            if (i > 0) ui.PushClip(layout.content);   // the mods' list draws its own
            ui.g.PushOpacity(k);
            ui.PushTranslate((shown == i ? 30.f : -30.f) * (1.f - k), 0.f);
            switch (static_cast<MainState::View>(i)) {
                case MainState::View::Mods: mods->Draw(ui, layout.content); break;
                case MainState::View::Store: store->Draw(ui, layout.content); break;
                case MainState::View::Updates: updates->Draw(ui, layout.content); break;
                case MainState::View::Changelog: changelog->Draw(ui, layout.content); break;
            }
            ui.PopTransform();
            ui.g.PopLayer();
            if (i > 0) ui.PopClip();
        }
        ui.SetInputMuted(muted);
    }
    StatusBar::Draw(ui, *this, layout.statusBar);
    ui.SetInputMuted(false);
    for (Overlay* o : overlays)
        if (o->Visible()) o->Draw(ui, screen);

    if (radius > 0.f) {
        g.PopLayer();
        g.StrokeRound(screen.Inset(0.5f), radius - 0.5f, Kit::Palette::WindowBorder, 1.f);
    } else {
        g.StrokeRect(screen.Inset(0.5f), Kit::Palette::WindowBorder, 1.f);
    }
}

void MainWindow::OnClosing(bool& cancel) {
    // ZIPs are being copied / extracted on a worker thread: closing now would cut a file in half in
    // the Paks folder, so the window closes once the installation is done
    // While a name is asked for (a ZIP being split) nothing is half done: it just closes (the
    // temporary files go at the next start)
    if (state && state->busy && !input->IsOpen() && !addMod->IsOpen()) {
        cancel = true;
        closeWhenIdle = true;
        state->SetStatus(Loc::Get("status_closing_after_install"));
    }
}

void MainWindow::InstallFinished() {
    if (closeWhenIdle) Close();
}

void MainWindow::OnDestroyed() { PostQuitMessage(0); }

} // namespace RivalsForge::Main

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "UI/Window.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/MainState.h"

namespace RivalsForge::Main {

// WM_COPYDATA's dwData for an nxm:// link handed over by a second launch ("NXM")
inline constexpr ULONG_PTR kNxmMessage = 0x4E584D;

class Overlay;
class LanguagePopup;
class MessageOverlay;
class InputOverlay;
class DeleteOverlay;
class BypassOverlay;
class AddModOverlay;
class HeroPickerOverlay;
class ModsView;
class StoreView;
class UpdatesView;
class ChangelogView;
class StoreModOverlay;
class StoreAccountOverlay;
class ThemeTransition;

// MainWindow.xaml(.cs): the borderless, resizable window (RootBorder with its rounded corners and
// 1 px border), the title bar, the header, the content and the status bar, plus the in-window
// cards and popups drawn over them. Bars live in MainScreen\, the content in Mods\, the cards and
// popups in Overlays\.
class MainWindow : public UI::Window {
public:
    MainWindow();
    ~MainWindow() override;
    bool Open();
    static MainWindow* Instance();

    MainState& State() { return *state; }
    ModsView& Mods() { return *mods; }
    StoreView& Store() { return *store; }
    UpdatesView& Updates() { return *updates; }
    StoreModOverlay& StoreModCard() { return *storeCard; }
    StoreAccountOverlay& StoreAccount() { return *storeAccount; }
    void ShowStore() { state->view = MainState::View::Store; }
    // The header's sun / moon button: switches between the dark and the light theme with a sweep
    void ToggleTheme();
    const ThemeTransition& Theme() const { return *themeTransition; }
    // An nxm:// link the app was started with (Main.cpp), handled once the window is up
    void SetStartupLink(const std::wstring& url) { startupLink = url; }
    const MainLayout& Layout() const { return layout; }
    LanguagePopup& Languages() { return *languages; }
    MessageOverlay& Messages() { return *messages; }
    DeleteOverlay& Delete() { return *deleteCard; }
    BypassOverlay& Bypass() { return *bypass; }
    InputOverlay& Input() { return *input; }
    AddModOverlay& AddModCard() { return *addMod; }
    HeroPickerOverlay& HeroPicker() { return *heroPicker; }
    // MinimizeBtn_Click / MaximizeBtn_Click / CloseBtn_Click
    void Quit() { Close(); }
    // The background installation finished: a close asked for meanwhile goes ahead
    void InstallFinished();

protected:
    void OnFrame() override;
    bool IsCaption(float x, float y) const override;
    // WM_COPYDATA: an nxm:// link from a second launch of the app (Main.cpp)
    bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) override;
    void OnClosing(bool& cancel) override;
    void OnDestroyed() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    // RootBorder and everything inside it
    void DrawFrame(const Rect& screen);
    void Backdrop(const Rect& screen);

    std::unique_ptr<MainState> state;
    std::unique_ptr<ModsView> mods;
    std::unique_ptr<StoreView> store;
    std::unique_ptr<UpdatesView> updates;
    std::unique_ptr<ChangelogView> changelog;
    std::unique_ptr<StoreModOverlay> storeCard;
    std::unique_ptr<StoreAccountOverlay> storeAccount;
    std::wstring startupLink;
    MainLayout layout;
    std::unique_ptr<LanguagePopup> languages;
    std::unique_ptr<MessageOverlay> messages;
    std::unique_ptr<DeleteOverlay> deleteCard;
    std::unique_ptr<BypassOverlay> bypass;
    std::unique_ptr<InputOverlay> input;
    std::unique_ptr<AddModOverlay> addMod;
    std::unique_ptr<HeroPickerOverlay> heroPicker;
    std::unique_ptr<ThemeTransition> themeTransition;
    std::vector<Overlay*> overlays;   // z-order (first = bottom)
    bool closeWhenIdle = false;       // closed while installing: closes once it's done
    bool modalShown = false;
#ifdef _DEBUG
    // Development aid: RF_SCREENSHOT=<file.png> renders a frame into that file once the entrance
    // animations are over, then closes (the window is never activated)
    std::wstring screenshotPath;
    bool screenshotDue = false;
#endif
};

} // namespace RivalsForge::Main

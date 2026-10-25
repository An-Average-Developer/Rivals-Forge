#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include "App/MessageLoop.h"
#include "Core/Async.h"
#include "Graphics/Device.h"
#include "Graphics/Fonts.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/App/ResourceExtractor.h"
#include "App/Paths.h"
#include "Services/Store/NexusAccount.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"

using namespace RivalsForge;

namespace {

// The nxm:// link the app was started with (the site's "Mod Manager Download"), or ""
std::wstring NxmArgument() {
    int count = 0;
    wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &count);
    std::wstring link;
    for (int i = 1; args && i < count; ++i)
        if (_wcsnicmp(args[i], L"nxm://", 6) == 0) link = args[i];
    if (args) LocalFree(args);
    return link;
}

// The app already open takes the link (WM_COPYDATA) and comes to the front: true when it did
bool HandOver(const std::wstring& link) {
    HWND running = FindWindowW(L"RivalsForge.Window", L"Rivals Forge");
    if (!running) return false;
    COPYDATASTRUCT data{ Main::kNxmMessage, static_cast<DWORD>((link.size() + 1) * sizeof(wchar_t)), const_cast<wchar_t*>(link.c_str()) };
    DWORD_PTR answer = 0;
    AllowSetForegroundWindow(ASFW_ANY);
    return SendMessageTimeoutW(running, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 5000, &answer) && answer;
}

} // namespace

// App.xaml.cs (StartupUri="MainWindow.xaml")
int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    const std::wstring link = NxmArgument();
    if (!link.empty() && HandOver(link)) return 0;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Paths::MigrateDataFolder();   // %LOCALAPPDATA%\MarvelRivalsModManager -> RivalsForge, once
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Async::Initialize();
    Device::Get().Init();

    int code = 0;
    Services::ResourceExtractor::Extract();   // flags and translations to %TEMP%\RivalsForge
    Loc::LoadSaved();
    Main::ModsKit::LoadTheme();   // theme.cfg: dark (default) or light
    Fonts::Initialize();
    Log::Start();
    Services::Store::NexusAccount::RepairHandler();
    {
        Main::MainWindow window;
        window.SetStartupLink(link);
        if (window.Open()) code = App::RunMessageLoop();
        else Log::Error(L"The main window could not be opened");
    }
    Log::Info(L"Manager closed");
    Async::Shutdown();
    Device::Get().Shutdown();
    CoUninitialize();
    return code;
}

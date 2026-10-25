#pragma once
#include <optional>
#include "Services/App/UpdateService.h"
#include "UI/Ui.h"
#include "Windows/Main/Updates/ChangelogCards.h"

namespace RivalsForge::Main {

class MainWindow;

// The Updates page (the Inazuma Eleven VR Save Editor's Updates tab, in this window's look): the
// title row over two columns. Overview: the update status (checking / up to date / update
// available / GitHub unreachable) with its actions on the left; your version, where updates come
// from and how they work on the right. Installation: what's new since your version on the left, the
// install card (from → to, the file, Download & Install or the progress ring) on the right. Manual
// update: always fetches the latest published release, whatever the version comparison says, to
// force a reinstall or open it in the browser.
class UpdatesView {
public:
    explicit UpdatesView(MainWindow& window) : main(window) {}
    void Draw(UI::Ui& ui, const Rect& content);
    void OpenManual();

private:
    enum class Manual { Checking, Ready, Unreachable, Downloading, Failed };

    float DrawOverview(UI::Ui& ui, const Rect& area);
    float DrawInstallation(UI::Ui& ui, const Rect& area);
    float DrawManual(UI::Ui& ui, const Rect& area);
    void StatusHero(UI::Ui& ui, const Rect& r);
    void ManualHero(UI::Ui& ui, const Rect& r);
    float VersionCard(UI::Ui& ui, const Rect& r, bool draw);
    float SourcesCard(UI::Ui& ui, const Rect& r, bool draw);
    float StepsCard(UI::Ui& ui, const Rect& r, bool draw, const std::wstring& title, const std::wstring* steps, int count);
    float InstallCard(UI::Ui& ui, const Rect& r, bool draw);
    float ReleaseCard(UI::Ui& ui, const Rect& r, bool draw);
    void ManualCheck();
    void ManualDownload();

    MainWindow& main;
    double shownAt = -1.0;        // entrance of the page (again when the view inside it changes)
    int shownView = -1;           // 0 overview, 1 installation, 2 manual
    ChangelogCards news{ "upd-news" };   // What's New of the installation

    bool manualView = false;
    Manual manual = Manual::Checking;
    std::optional<Services::Updates::UpdateInfo> manualRelease;
    int manualProgress = 0;
};

} // namespace RivalsForge::Main

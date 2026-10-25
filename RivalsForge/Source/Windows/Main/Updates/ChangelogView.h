#pragma once
#include <string>
#include <vector>
#include "UI/Ui.h"
#include "Windows/Main/Updates/ChangelogCards.h"

namespace RivalsForge::Main {

class MainWindow;

// The Changelog page (the Inazuma Eleven VR Save Editor's Changelogs tab, in this window's look):
// the title row over two columns, the whole history as folding version cards on the left and, on
// the right, the history at a glance (versions, changes, installed / latest) and the changes by
// type as growing bars. The history is the published changelog.txt once an update check got it,
// the embedded one until then.
class ChangelogView {
public:
    explicit ChangelogView(MainWindow& window) : main(window) {}
    void Draw(UI::Ui& ui, const Rect& content);

private:
    struct TypeCount {
        std::wstring title;
        int count = 0;
    };
    void Summarize(const std::wstring& text);
    float Glance(UI::Ui& ui, const Rect& r, bool draw);
    float ByType(UI::Ui& ui, const Rect& r, bool draw);

    MainWindow& main;
    double shownAt = -1.0;
    bool wasShown = false;
    ChangelogCards cards{ "chl-history" };
    // The history summarized (from `summarized`)
    std::wstring summarized;
    int versions = 0, changes = 0;
    std::wstring latestVersion, latestLabel;
    std::vector<TypeCount> types;   // most changes first
};

} // namespace RivalsForge::Main

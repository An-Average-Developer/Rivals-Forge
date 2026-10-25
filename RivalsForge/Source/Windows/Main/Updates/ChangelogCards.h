#pragma once
#include <string>
#include <vector>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

namespace RivalsForge::Main {

// The changelog as folding version cards (the Inazuma Eleven VR Save Editor's ChangelogCards, in
// this window's look): the newest open and lit in red with a LATEST tag, chips counting each card's
// changes per section, the "## Section" headings as coloured tags and the changes as lines.
class ChangelogCards {
public:
    explicit ChangelogCards(std::string idKey) : key(std::move(idKey)) {}
    // `changelog` is changelog.txt ("1.0.1\n## Additions\n- ...\n\n1.0.0\n..."); only the versions
    // newer than `current` and up to `latest` are shown (or the latest one, when none match)
    void Draw(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest);
    // Every version (the Changelog page), the `installed` one tagged INSTALLED
    void DrawHistory(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& installed);

    // Colour and icon of a section ("Additions", "Bug Fixes"...), its name in the current language
    // (the changelog itself is written in English) and a colour made readable as text in this theme
    static Color SectionColor(const std::wstring& title);
    static UI::Material::Icon SectionIcon(const std::wstring& title);
    static std::wstring SectionName(const std::wstring& title);
    static Color Readable(Color c);

private:
    struct Line {
        std::wstring text;
        std::vector<TextSpan> spans;   // bold "Topic:" prefix
        float y = 0.f, h = 0.f;        // relative to the card body
    };
    struct Section {
        std::wstring title;            // empty: changes before any section line
        float y = 0.f;
        std::vector<Line> lines;
    };
    struct Card {
        std::wstring version, label, date;
        std::vector<Section> sections;
        int changes = 0;
        float bodyH = 0.f;
    };
    void DrawList(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest);
    void Build(UI::Ui& ui, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest, float width);
    float DrawCard(UI::Ui& ui, size_t index, float x, float y, float w);

    std::string key;          // id seed (one instance per page)
    bool history = false;
    std::vector<Card> cards;
    std::vector<bool> open;
    std::wstring source, sourceCurrent, sourceLatest;
    float builtWidth = -1.f;
    bool builtLight = false;  // theme the line colours were taken from
    bool builtHistory = false;
    double shownAt = -10.0;
    float viewTop = 0.f, viewBottom = 0.f;   // the visible part of the list, in its coordinates
};

} // namespace RivalsForge::Main

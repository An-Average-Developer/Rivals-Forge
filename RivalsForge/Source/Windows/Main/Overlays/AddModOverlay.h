#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace RivalsForge::Main {

// The name of each mod being added (was InputDialog's "Name Your Mod"), one card per mod: the ZIP
// it comes from, what will happen (a normal ZIP installed as it is, a ZIP tidied up, or one of the
// mods of a ZIP that holds several), where it goes (MODS) and that the original ZIP goes to the
// Recycle Bin, which mod of how many (a bar of steps), its files (a tag per type, the size), the
// name in a text box and the ZIP it will become in MODS, updated as it's typed. Next / OK takes the
// name, Skip takes the suggested one, Skip all the suggested ones for the rest too, Cancel (Escape,
// a click outside) stops everything.
class AddModOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;

    enum class Kind { Plain, Tidy, Split };   // a normal ZIP / one mod in a folder / one of several
    struct File {
        std::wstring name;
        uint64_t size = 0;
    };
    struct Request {
        Kind kind = Kind::Plain;
        std::wstring zipPath;               // the ZIP chosen
        int part = 0, parts = 1;            // Split: this mod of the ZIP's mods
        int index = 0, count = 1;           // this mod (0-based) of all being added
        std::vector<File> files;
        std::wstring defaultName;
        std::function<std::wstring(const std::wstring&)> zipName;   // the ZIP's file name in MODS for a mod name
        std::wstring modsFolder;
        bool downloaded = false;            // from the store: no original to send to the Recycle Bin
    };
    struct Answer {
        enum class Action { Cancel, Name, Skip, SkipAll } action = Action::Cancel;
        std::wstring name;                  // Name / Skip / SkipAll: the name to use
    };
    void Show(Request request, std::function<void(Answer)> done);

protected:
    float CardWidth() const override { return 560.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Finish(Answer::Action action);
    std::wstring Intro() const;
    // The name the ZIP will get in MODS for the name typed
    std::wstring ResultName() const;

    Request request;
    std::wstring text;
    Answer answer;
    std::function<void(Answer)> done;
    bool focusPending = false;
};

} // namespace RivalsForge::Main

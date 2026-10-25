#include "Windows/Main/Overlays/AddModOverlay.h"
#include <algorithm>
#include <cmath>
#include "Core/Files.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kButtonW = 104.f, kButtonH = 36.f, kSourceH = 34.f, kRowH = 28.f, kStepH = 18.f, kFolderH = 44.f;
constexpr size_t kMaxRows = 6;   // more files: the first ones and "+ N more"
constexpr int kMaxSegments = 30; // more mods: one continuous bar

TextStyle TitleStyle() { return Kit::Display(24.f, DWRITE_FONT_WEIGHT_BOLD); }
TextStyle IntroStyle() { return Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true); }
TextStyle LabelStyle() { return Kit::Display(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD); }
UI::TextBoxStyle BoxStyle() { return Kit::InputBoxStyle(); }

// Each file type in its colour: the container (.pak), the data (.ucas), the table (.utoc)
Color TypeColor(const std::wstring& ext) {
    if (ext == L"PAK") return Kit::Palette::Gold;
    if (ext == L"UCAS") return Kit::Palette::Info;
    if (ext == L"UTOC") return Kit::Palette::Enabled;
    return Kit::Palette::TextDim;
}

size_t Rows(size_t files) { return files > kMaxRows ? kMaxRows : std::max<size_t>(files, 1); }

} // namespace

void AddModOverlay::Show(Request r, std::function<void(Answer)> callback) {
    request = std::move(r);
    text = request.defaultName;
    answer = {};
    done = std::move(callback);
    focusPending = true;
    onClosed = [this] {
        auto callback = std::move(done);
        done = nullptr;
        if (callback) callback(answer);
    };
    Open();
}

void AddModOverlay::Finish(Answer::Action action) {
    if (!IsOpen()) return;
    answer.action = action;
    const std::wstring trimmed = Text::TrimDotNet(text);
    answer.name = action == Answer::Action::Name && !trimmed.empty() ? trimmed : request.defaultName;
    Close();
}

std::wstring AddModOverlay::Intro() const {
    switch (request.kind) {
        case Kind::Tidy: return Loc::Get("split_intro_one");
        case Kind::Split: return Loc::Fmt("split_intro_many", request.parts, request.part + 1);
        default: return Loc::Get("install_intro");
    }
}

std::wstring AddModOverlay::ResultName() const {
    const std::wstring trimmed = Text::TrimDotNet(text);
    const std::wstring name = trimmed.empty() ? request.defaultName : trimmed;
    return request.zipName ? request.zipName(name) : name + L".zip";
}

float AddModOverlay::CardHeight(UI::Ui& ui, float width) {
    const float inner = width - kPad * 2.f;
    float h = kPad + Kit::LineH(ui, TitleStyle()) + 12.f + kSourceH + 14.f;
    h += ui.g.Measure(Intro(), IntroStyle(), inner).h + 10.f + kFolderH + 8.f + 18.f + 16.f;   // intro, MODS, the Recycle Bin
    if (request.count > 1) h += kStepH + 10.f;
    const size_t rows = Rows(request.files.size()) + (request.files.size() > kMaxRows ? 1 : 0);
    h += Kit::LineH(ui, LabelStyle()) + 6.f + kRowH * static_cast<float>(rows) + 12.f + 18.f;
    h += Kit::LineH(ui, LabelStyle()) + 6.f + UI::TextBoxHeight(ui, BoxStyle()) + 10.f + 18.f + 22.f;
    return h + kButtonH + kPad;
}

void AddModOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;

    // Title and the ZIP chosen
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        const float h = Kit::LineH(ui, TitleStyle());
        const char* title = request.kind == Kind::Tidy ? "split_title_one" : request.kind == Kind::Split ? "split_title_many" : "install_title";
        ui.g.Text(Kit::Upper(Loc::Get(title)), { inner.x, y, inner.w, h }, Kit::Palette::Text, TitleStyle());
        y += h + 12.f;
        const Rect source{ inner.x, y, inner.w, kSourceH };
        Kit::FillSlant(ui, source, Kit::Palette::InputBg, 8.f);
        Kit::EdgeStripe(ui, source, 5.f, Kit::Palette::Accent, 8.f);
        Kit::DrawGlyph(ui, Kit::Glyph::Box, source.x + 26.f, source.CY(), 16.f, Kit::Palette::AccentHover);
        TextStyle name = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        ui.g.Text(Files::FileName(request.zipPath), { source.x + 44.f, source.y, source.w - 60.f, source.h }, Kit::Palette::Text, name);
        y += kSourceH + 14.f;
    }

    // What happens: the intro, where it goes (MODS) and the original to the Recycle Bin
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const std::wstring intro = Intro();
        const float h = ui.g.Measure(intro, IntroStyle(), inner.w).h;
        ui.g.Text(intro, { inner.x, y, inner.w, h }, Kit::Palette::Body, IntroStyle());
        y += h + 10.f;
        const Rect folder{ inner.x, y, inner.w, kFolderH };
        Kit::FillSlant(ui, folder, Kit::Palette::Info.Fade(0.1f), 8.f);
        Kit::EdgeStripe(ui, folder, 4.f, Kit::Palette::Info, 8.f);
        Kit::DrawGlyph(ui, Kit::Glyph::Folder, folder.x + 24.f, folder.CY(), 16.f, Kit::Palette::Info);
        TextStyle head = Kit::Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        head.ellipsis = true;
        TextStyle path = Kit::Style(11.f);
        path.ellipsis = true;
        ui.g.Text(Loc::Get("split_to_mods"), { folder.x + 42.f, folder.y + 6.f, folder.w - 56.f, 16.f }, Kit::Palette::Text, head);
        ui.g.Text(request.modsFolder, { folder.x + 42.f, folder.y + 23.f, folder.w - 56.f, 15.f }, Kit::Palette::TextDim, path);
        y += kFolderH + 8.f;
        Kit::DrawGlyph(ui, request.downloaded ? Kit::Glyph::Check : Kit::Glyph::Trash, inner.x + 7.f, y + 9.f, 13.f, Kit::Palette::Gold);
        TextStyle note = Kit::Style(12.f);
        note.valign = VAlign::Center;
        ui.g.Text(Loc::Get(request.downloaded ? "store_downloaded_note" : "split_recycle"), { inner.x + 20.f, y, inner.w - 20.f, 18.f },
                  Kit::Palette::Gold, note);
        y += 18.f + 16.f;
    }

    // Which mod of how many: "MOD 3 OF 13" and a bar of steps
    if (request.count > 1) {
        M::Motion::Enter enter(ui, Reveal(ui, 2));
        TextStyle step = Kit::Display(14.f, DWRITE_FONT_WEIGHT_BOLD);
        step.valign = VAlign::Center;
        const std::wstring label = Kit::Upper(Loc::Fmt("split_step", request.index + 1, request.count));
        const float lw = std::ceil(ui.g.Measure(label, step).w) + 16.f;
        ui.g.Text(label, { inner.x, y, lw, kStepH }, Kit::Palette::AccentHover, step);
        const float bx = inner.x + lw, bw = inner.R() - bx;
        const float cy = y + kStepH * 0.5f - 3.f;
        if (request.count > kMaxSegments) {
            Kit::FillSlant(ui, { bx, cy, bw, 6.f }, Kit::Palette::Ghost, 3.f);
            Kit::FillSlant(ui, { bx, cy, bw * static_cast<float>(request.index + 1) / static_cast<float>(request.count), 6.f }, Kit::Palette::Accent, 3.f);
        } else {
            const float gap = request.count > 12 ? 3.f : 6.f;
            const float segment = (bw - gap * static_cast<float>(request.count - 1)) / static_cast<float>(request.count);
            for (int i = 0; i < request.count; ++i) {
                const Color color = i < request.index ? Kit::Palette::Enabled : i == request.index ? Kit::Palette::Accent : Kit::Palette::Ghost;
                Kit::FillSlant(ui, { bx + i * (segment + gap), cy, segment, 6.f }, color, 3.f);
            }
        }
        y += kStepH + 10.f;
    }

    // Its files: a tag per type, the name, the size
    {
        M::Motion::Enter enter(ui, Reveal(ui, 3));
        const float lh = Kit::LineH(ui, LabelStyle());
        ui.g.Text(Kit::Upper(Loc::Get("split_files")), { inner.x, y, inner.w, lh }, Kit::Palette::TextDim, LabelStyle());
        y += lh + 6.f;
        const size_t shown = std::min(request.files.size(), kMaxRows);
        const size_t rows = Rows(request.files.size()) + (request.files.size() > kMaxRows ? 1 : 0);
        const Rect box{ inner.x, y, inner.w, kRowH * static_cast<float>(rows) + 12.f };
        ui.g.FillRect(box, Kit::Palette::Sunken);
        ui.g.StrokeRect(box.Inset(0.5f), Kit::Palette::Line, 1.f);
        TextStyle tag = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD, false);
        tag.align = HAlign::Center;
        tag.valign = VAlign::Center;
        TextStyle file = Kit::Style(12.f);
        file.valign = VAlign::Center;
        file.ellipsis = true;
        TextStyle size = Kit::Style(11.5f);
        size.valign = VAlign::Center;
        size.align = HAlign::Right;
        float ry = box.y + 6.f;
        for (size_t i = 0; i < shown; ++i) {
            const std::wstring extension = Files::Extension(request.files[i].name);
            const std::wstring ext = Text::Upper(extension.empty() ? L"?" : extension.substr(1));
            const Color c = TypeColor(ext);
            const Rect t{ box.x + 10.f, ry + 5.f, 48.f, kRowH - 10.f };
            Kit::FillSlant(ui, t, c.Fade(0.2f), 4.f);
            ui.g.Text(ext, t, c, tag);
            ui.g.Text(request.files[i].name, { t.R() + 10.f, ry, box.R() - t.R() - 100.f, kRowH }, Kit::Palette::Text, file);
            ui.g.Text(Text::FormatBytes(request.files[i].size), { box.R() - 90.f, ry, 80.f, kRowH }, Kit::Palette::TextFaint, size);
            ry += kRowH;
        }
        if (request.files.empty()) ui.g.Text(Loc::Get("install_no_files"), { box.x + 12.f, ry, box.w - 24.f, kRowH }, Kit::Palette::TextFaint, file);
        if (request.files.size() > kMaxRows)
            ui.g.Text(Loc::Fmt("install_more_files", static_cast<int>(request.files.size() - kMaxRows)), { box.x + 68.f, ry, box.w - 80.f, kRowH },
                      Kit::Palette::TextDim, file);
        y = box.B() + 18.f;
    }

    // The name, and the ZIP it becomes
    {
        M::Motion::Enter enter(ui, Reveal(ui, 4));
        const float lh = Kit::LineH(ui, LabelStyle());
        ui.g.Text(Kit::Upper(Loc::Get("split_name")), { inner.x, y, inner.w, lh }, Kit::Palette::TextDim, LabelStyle());
        y += lh + 6.f;
        const UI::Id box = ui.MakeId("addmod-box");
        if (focusPending && IsOpen()) {
            focusPending = false;
            ui.SetFocus(box);
            UI::SetTextBoxSelection(ui, box, 0, static_cast<int>(text.size()));
        }
        const float h = UI::TextBoxHeight(ui, BoxStyle());
        bool entered = false;
        UI::TextBox(ui, box, { inner.x, y, inner.w, h }, text, BoxStyle(), &entered);
        if (entered) Finish(Answer::Action::Name);
        y += h + 10.f;

        Kit::DrawGlyph(ui, Kit::Glyph::Folder, inner.x + 7.f, y + 9.f, 13.f, Kit::Palette::TextDim);
        TextStyle label = Kit::Style(12.f);
        label.valign = VAlign::Center;
        const std::wstring prefix = Loc::Get("split_result") + L"  ";
        const float pw = std::ceil(ui.g.Measure(prefix, label).w);
        ui.g.Text(prefix, { inner.x + 20.f, y, pw + 2.f, 18.f }, Kit::Palette::TextDim, label);
        TextStyle zip = Kit::Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        zip.valign = VAlign::Center;
        zip.ellipsis = true;
        ui.g.Text(ResultName(), { inner.x + 20.f + pw, y, inner.w - 20.f - pw, 18.f }, Kit::Palette::Text, zip);
        y += 18.f + 22.f;
    }

    // Cancel on the left; Skip (all), Next / OK on the right
    M::Motion::Enter enter(ui, Reveal(ui, 5));
    const bool last = request.index + 1 >= request.count;
    const Rect ok{ inner.R() - kButtonW, y, kButtonW, kButtonH };
    Rect skip{ ok.x - 6.f - kButtonW, y, kButtonW, kButtonH };
    if (Kit::Button(ui, ui.MakeId("addmod-ok"), ok, Loc::Get(last ? "ok" : "split_next"), Kit::DialogButton(true))) Finish(Answer::Action::Name);
    {
        const UI::Id id = ui.MakeId("addmod-skip");
        if (Kit::Button(ui, id, skip, Loc::Get("split_skip"), Kit::DialogButton(false))) Finish(Answer::Action::Skip);
        ui.Tooltip(id, Loc::Fmt("split_skip_tooltip", request.defaultName));
    }
    if (!last) {
        const Kit::ButtonStyle style = Kit::DialogButton(false);
        const std::wstring label = Loc::Get("split_skip_all");
        const float w = std::max(kButtonW, Kit::ButtonWidth(ui, label, style));
        const Rect all{ skip.x - 6.f - w, y, w, kButtonH };
        const UI::Id id = ui.MakeId("addmod-skip-all");
        if (Kit::Button(ui, id, all, label, style)) Finish(Answer::Action::SkipAll);
        ui.Tooltip(id, Loc::Fmt("split_skip_all_tooltip", request.count - request.index - 1));
    }
    if (Kit::Button(ui, ui.MakeId("addmod-cancel"), { inner.x, y, kButtonW, kButtonH }, Loc::Get("cancel"), Kit::Danger())) Finish(Answer::Action::Cancel);
    if (IsOpen() && ui.KeyPressed(VK_RETURN)) Finish(Answer::Action::Name);
}

} // namespace RivalsForge::Main

#include "Windows/Main/Overlays/DeleteOverlay.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kButtonW = 110.f, kButtonH = 36.f, kNameH = 34.f, kIcon = 44.f, kOptionPad = 14.f, kOptionGap = 10.f;

TextStyle TitleStyle() { return Kit::Display(24.f, DWRITE_FONT_WEIGHT_BOLD); }
TextStyle QuestionStyle() { return Kit::Style(13.f); }
TextStyle OptionTitleStyle() { return Kit::Display(16.f, DWRITE_FONT_WEIGHT_BOLD); }
TextStyle OptionTextStyle() { return Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true); }

// The text's width inside an option: right of the icon box
float TextWidth(float optionWidth) { return optionWidth - kOptionPad * 2.f - kIcon - 14.f; }

} // namespace

void DeleteOverlay::Show(const std::wstring& modName, bool modEnabled, std::function<void(Choice)> callback) {
    name = modName;
    enabled = modEnabled;
    result = Choice::Cancel;
    done = std::move(callback);
    onClosed = [this] {
        auto callback = std::move(done);
        done = nullptr;
        if (callback) callback(result);
    };
    Open();
}

void DeleteOverlay::Answer(Choice choice) {
    if (!IsOpen()) return;
    result = choice;
    Close();
}

float DeleteOverlay::OptionHeight(UI::Ui& ui, float width, const std::wstring& text, const std::wstring& note) const {
    const float w = TextWidth(width);
    float h = Kit::LineH(ui, OptionTitleStyle()) + 4.f + ui.g.Measure(text, OptionTextStyle(), w).h;
    if (!note.empty()) h += 6.f + ui.g.Measure(note, OptionTextStyle(), w - 18.f).h;
    return std::max(kIcon, h) + kOptionPad * 2.f;
}

float DeleteOverlay::CardHeight(UI::Ui& ui, float width) {
    const float inner = width - kPad * 2.f;
    float h = kPad + Kit::LineH(ui, TitleStyle()) + 12.f + kNameH + 14.f + Kit::LineH(ui, QuestionStyle()) + 12.f;
    h += OptionHeight(ui, inner, Loc::Get("delete_list_text"), enabled ? Loc::Get("delete_list_enabled") : L"") + kOptionGap;
    h += OptionHeight(ui, inner, Loc::Get("delete_files_text"), L"") + 22.f;
    return h + kButtonH + kPad;
}

bool DeleteOverlay::Option(UI::Ui& ui, const char* idName, const Rect& r, bool danger, const std::wstring& title, const std::wstring& text,
                           const std::wstring& note) {
    const UI::Id id = ui.MakeId(idName);
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Color accent = danger ? Kit::Palette::Danger : Kit::Palette::Gold;

    ui.PushTranslate(3.f * fb.hover, 0.f);
    ui.PushScale(1.f - 0.015f * fb.press, r.CX(), r.CY());
    if (fb.hover > 0.f) ui.g.GlowRound(r.Inset(6.f, 0.f), 2.f, accent, 14.f, 0.35f * Kit::GlowStrength() * fb.hover);
    Kit::FillSlant(ui, r, Lerp(Kit::Palette::InputBg, Lerp(Kit::Palette::InputBg, accent, 0.12f), fb.hover), 8.f);
    Kit::StrokeSlant(ui, r, Lerp(Kit::Palette::Line, accent.Fade(0.8f), fb.hover), 1.f, 8.f);
    Kit::EdgeStripe(ui, r, 5.f + 3.f * fb.hover, accent.Fade(0.5f + 0.5f * fb.hover), 8.f);

    // The icon box: a list for "only from the list", the bin for the files
    const Rect icon{ r.x + kOptionPad + 4.f, r.y + kOptionPad, kIcon, kIcon };
    Kit::FillSlant(ui, icon, accent.Fade(0.15f + 0.15f * fb.hover), 8.f);
    if (danger) {
        Kit::DrawGlyph(ui, Kit::Glyph::Trash, icon.CX(), icon.CY(), 22.f, accent);
    } else {
        for (int i = 0; i < 3; ++i) {
            const float ly = icon.CY() - 7.f + i * 7.f;
            ui.g.FillEllipse(icon.CX() - 8.f, ly, 1.8f, 1.8f, accent);
            ui.g.Line(icon.CX() - 3.f, ly, icon.CX() + 10.f, ly, accent, 2.f, true);
        }
    }

    // Title, explanation and, maybe, the warning
    const float x = icon.R() + 14.f, w = TextWidth(r.w);
    float y = r.y + kOptionPad;
    const float th = Kit::LineH(ui, OptionTitleStyle());
    ui.g.Text(Kit::Upper(title), { x, y, w, th }, danger ? Lerp(Kit::Palette::Text, Kit::Palette::Danger, 0.5f + 0.5f * fb.hover) : Kit::Palette::Text,
              OptionTitleStyle());
    y += th + 4.f;
    const float textH = ui.g.Measure(text, OptionTextStyle(), w).h;
    ui.g.Text(text, { x, y, w, textH }, Kit::Palette::TextDim, OptionTextStyle());
    y += textH;
    if (!note.empty()) {
        y += 6.f;
        Kit::DrawGlyph(ui, Kit::Glyph::Warning, x + 6.f, y + 8.f, 12.f, Kit::Palette::Gold);
        const float noteH = ui.g.Measure(note, OptionTextStyle(), w - 18.f).h;
        ui.g.Text(note, { x + 18.f, y, w - 18.f, noteH }, Kit::Palette::Gold, OptionTextStyle());
    }
    ui.PopTransform();
    ui.PopTransform();
    return it.clicked;
}

void DeleteOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;

    // Title and the mod
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        const float h = Kit::LineH(ui, TitleStyle());
        ui.g.Text(Kit::Upper(Loc::Get("delete_title")), { inner.x, y, inner.w, h }, Kit::Palette::Text, TitleStyle());
        y += h + 12.f;
        const Rect chip{ inner.x, y, inner.w, kNameH };
        Kit::FillSlant(ui, chip, Kit::Palette::InputBg, 8.f);
        Kit::EdgeStripe(ui, chip, 5.f, Kit::Palette::Accent, 8.f);
        ui.g.FillEllipse(chip.x + 24.f, chip.CY(), 4.f, 4.f, enabled ? Kit::Palette::Enabled : Kit::Palette::Disabled);
        TextStyle mod = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        mod.valign = VAlign::Center;
        mod.ellipsis = true;
        ui.g.Text(name, { chip.x + 40.f, chip.y, chip.w - 56.f, chip.h }, Kit::Palette::Text, mod);
        y += kNameH + 14.f;
        const float qh = Kit::LineH(ui, QuestionStyle());
        ui.g.Text(Loc::Get("delete_question"), { inner.x, y, inner.w, qh }, Kit::Palette::Body, QuestionStyle());
        y += qh + 12.f;
    }

    // The two choices
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 0.f, 20.f);
        const std::wstring text = Loc::Get("delete_list_text"), note = enabled ? Loc::Get("delete_list_enabled") : L"";
        const float h = OptionHeight(ui, inner.w, text, note);
        if (Option(ui, "delete-list", { inner.x, y, inner.w, h }, false, Loc::Get("delete_list_title"), text, note)) Answer(Choice::ListOnly);
        y += h + kOptionGap;
    }
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2), 0.f, 20.f);
        const std::wstring text = Loc::Get("delete_files_text");
        const float h = OptionHeight(ui, inner.w, text, L"");
        if (Option(ui, "delete-files", { inner.x, y, inner.w, h }, true, Loc::Get("delete_files_title"), text, L"")) Answer(Choice::Files);
        y += h + 22.f;
    }

    M::Motion::Enter enter(ui, Reveal(ui, 3));
    if (Kit::Button(ui, ui.MakeId("delete-cancel"), { inner.R() - kButtonW, y, kButtonW, kButtonH }, Loc::Get("cancel"), Kit::DialogButton(false)))
        Answer(Choice::Cancel);
}

} // namespace RivalsForge::Main

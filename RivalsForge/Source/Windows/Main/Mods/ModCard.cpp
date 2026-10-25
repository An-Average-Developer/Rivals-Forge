#include <cmath>
#include "Core/Time.h"
#include "Localization/Localization.h"
#include "Services/Mods/ModTypeDetector.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Mods/ModsView.h"

// A mod (was <!-- MOD CARD -->): a slanted row with a stripe of its status colour on the left, the
// enable switch (was Enable / Disable), the name over "N files · date · ENABLED / DISABLED", what
// kind of mod it is as coloured tags (MESH, TEXTURE, AUDIO...), and three icon buttons: Rename,
// Hero (was Move ▾) and Delete. Hovered, the row lights up and slides a
// little to the right, the stripe grows; toggling sends a flash along the row. What ModViewModel
// derived (StatusText, StatusBrush, ToggleButton*) is worked out here.
namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr Edges kPad{ 16.f, 10.f };
constexpr float kSkew = 6.f, kSwitchW = 38.f, kSwitchH = 20.f, kIcon = 30.f;

TextStyle NameStyle() {
    TextStyle t = Kit::Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.ellipsis = true;
    return t;
}
TextStyle MetaStyle(float size = 11.f, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) { return Kit::Style(size, weight); }

namespace Types = Services::Mods::ModTypeDetector;

// Each kind's tag: its colour and its name / tooltip
struct TypeLook {
    Color color;
    const char* key;
};
TypeLook LookOf(unsigned type) {
    switch (type) {
        case Types::Mesh: return { Hex(0x35B8C9), "mod_type_mesh" };
        case Types::Texture: return { Hex(0xE3A72F), "mod_type_texture" };
        case Types::Material: return { Hex(0xA47BEF), "mod_type_material" };
        case Types::Animation: return { Hex(0xE8609A), "mod_type_animation" };
        case Types::VFX: return { Hex(0xF2703C), "mod_type_vfx" };
        case Types::Audio: return { Hex(0x4CC27A), "mod_type_audio" };
        case Types::Video: return { Hex(0x5B8DEF), "mod_type_video" };
        case Types::Outdated: return { Kit::Palette::Danger, "mod_outdated" };
        default: return { Hex(0xD94CC0), "mod_type_ui" };
    }
}

// The kind's colour made readable as text on the row
Color TagText(Color c) { return Kit::IsLight() ? Lerp(c, Colors::Black, 0.38f) : Lerp(c, Colors::White, 0.25f); }

// DateAdded: AddedAt.ToString("MMM d, yyyy"), in the language's own culture
std::wstring DateAdded(const Models::Mod& mod) {
    const auto when = Time::Parse(mod.addedAt);
    return when ? Time::Format(*when, Loc::Get("date_format"), Loc::Get("date_locale")) : L"";
}

} // namespace

float ModsView::ModCardHeight(UI::Ui& ui) { return kPad.V() + Kit::LineH(ui, NameStyle()) + 3.f + Kit::LineH(ui, MetaStyle()); }

void ModsView::ModCard(UI::Ui& ui, const Rect& r, const ModRow& row, bool interactive) {
    const Models::Mod& mod = row.mod;
    const int64_t key = Key(row.id);
    const bool canAct = interactive && !Busy();

    const UI::Id cardId = ui.MakeId("mod-card", key);
    const UI::Interaction cardIt = ui.Interact(cardId, r, interactive, false);
    if (cardIt.hovered) ui.SetCursor(nullptr);   // the row itself isn't clickable, only its controls
    const float hover = M::Motion::Hover(ui, cardId, cardIt.hovered, false).hover;

    // StatusBrush, fading between the two colours when the mod is toggled
    const float on = ui.Tween(ui.MakeId("mod-on", key), mod.isEnabled ? 1.f : 0.f, 0.3f);
    if (on > 0.f && on < 1.f) ui.KeepAnimating();
    const Color status = Lerp(Kit::Palette::Disabled, Kit::Palette::Enabled, on);
    const float flash = Kit::Changed(ui, ui.MakeId("mod-flash", key), mod.isEnabled ? 1 : 0, 0.7f);

    // The row, its status lighting it from the left; a toggle sweeps a light along it
    const Rect row_ = r.Offset(3.f * hover, 0.f);
    Kit::FillSlant(ui, row_, Lerp(Kit::Palette::PanelRaised, Kit::Palette::PanelHover, hover), kSkew);
    Kit::PushSlantClip(ui, row_, kSkew);
    ui.g.FillLinear(row_, 0.f, { { 0.f, status.Fade(0.06f + 0.08f * hover) }, { 0.45f, status.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    if (flash > 0.f) Kit::Sheen(ui, row_, 1.f - flash, 1.2f);
    ui.g.PopLayer();
    Kit::EdgeStripe(ui, row_, 5.f + 3.f * hover, status, kSkew);
    const Rect inner = row_.Inset(kPad);

    // The switch
    const Rect sw{ inner.x + 2.f, inner.CY() - kSwitchH * 0.5f, kSwitchW, kSwitchH };
    {
        const UI::Id id = ui.MakeId("mod-toggle", key);
        if (Kit::Switch(ui, id, sw, mod.isEnabled, canAct)) ToggleMod(row.id);
        ui.Tooltip(id, Loc::Get(mod.isEnabled ? "mod_disable" : "mod_enable"));   // ToggleButtonText
    }

    // Icon buttons, right to left; dim until the row is hovered
    float right = inner.R();
    const auto place = [&] {
        const Rect b{ right - kIcon, inner.CY() - kIcon * 0.5f, kIcon, kIcon };
        right = b.x - 5.f;
        return b;
    };
    ui.g.PushOpacity(0.55f + 0.45f * hover);
    if (Kit::IconButton(ui, ui.MakeId("mod-delete", key), place(), Kit::Glyph::Trash, Kit::Palette::Danger, Loc::Get("mod_delete"), canAct))
        DeleteMod(row.id);
    if (Kit::IconButton(ui, ui.MakeId("mod-hero", key), place(), Kit::Glyph::Person, Kit::Palette::Gold, Loc::Get("mod_hero"), canAct))
        ChangeHero(row.id);
    if (Kit::IconButton(ui, ui.MakeId("mod-rename", key), place(), Kit::Glyph::Pencil, Kit::Palette::Strong, Loc::Get("mod_rename"), canAct))
        RenameMod(row.id);
    ui.g.PopLayer();

    // What kind of mod it is: a tag per kind, right to left before the buttons, as far as they fit
    // (fading in once the ZIP has been read); a mod packed the old way has a solid red OLD tag of
    // its own, nearest the buttons so it's always shown
    if (const unsigned types = state.TypesOf(mod)) {
        const float shown = ui.Tween(ui.MakeId("mod-types", key), 1.f, 0.3f, 0.f);
        if (shown < 1.f) ui.KeepAnimating();
        TextStyle t = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD);
        t.align = HAlign::Center;
        t.valign = VAlign::Center;
        const float minLeft = sw.R() + 14.f + 120.f;   // the name keeps some room
        ui.g.PushOpacity(shown);
        std::vector<unsigned> kinds;
        for (unsigned type : Types::kAllTypes)
            if (types & type) kinds.push_back(type);
        if (types & Types::Outdated) kinds.push_back(Types::Outdated);
        for (auto it = kinds.rbegin(); it != kinds.rend(); ++it) {
            const bool old = *it == Types::Outdated;
            const TypeLook look = LookOf(*it);
            const std::wstring label = Kit::Upper(Loc::Get(look.key));
            const float tw = std::ceil(ui.g.Measure(label, t).w) + 22.f;
            const Rect tag{ right - 6.f - tw, inner.CY() - 11.f, tw, 22.f };
            if (tag.x < minLeft) break;
            Kit::FillSlant(ui, tag, old ? look.color : look.color.Fade(Kit::IsLight() ? 0.16f : 0.14f), 6.f);
            Kit::StrokeSlant(ui, tag, look.color.Fade(old ? 1.f : 0.55f), 1.f, 6.f);
            ui.g.Text(label, tag.Offset(1.f, 0.f), old ? Colors::White : TagText(look.color), t);
            const UI::Id tagId = UI::HashId(ui.MakeId("mod-type", key), static_cast<int64_t>(*it));
            ui.Interact(tagId, tag, interactive, false);
            ui.Tooltip(tagId, Loc::Get(std::string(look.key) + "_tooltip"));
            right = tag.x - 2.f;
        }
        ui.g.PopLayer();
    }

    // Name, then "N file(s)  ·  date  ·  STATUS"
    const float x = sw.R() + 14.f;
    const float w = std::max(0.f, right - 8.f - x);
    const float nameH = Kit::LineH(ui, NameStyle()), metaH = Kit::LineH(ui, MetaStyle());
    const float y = inner.CY() - (nameH + 3.f + metaH) * 0.5f;
    ui.g.Text(mod.name, { x, y, w, nameH }, Lerp(Kit::IsLight() ? Hex(0x26262E) : Hex(0xE6E6EC), Kit::Palette::Strong, hover), NameStyle());

    ui.PushClip({ x, y + nameH, w, metaH + 3.f });
    float mx = x;
    const float my = y + nameH + 3.f;
    const auto piece = [&](const std::wstring& text, Color color, const TextStyle& style) {
        TextStyle t = style;
        t.valign = VAlign::Center;
        const float tw = std::ceil(ui.g.Measure(text, t).w);
        ui.g.Text(text, { mx, my, tw + 1.f, metaH }, color, t);
        mx += tw;
    };
    const int files = static_cast<int>(mod.files.size());
    piece(std::to_wstring(files), Kit::Palette::Text, MetaStyle(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD));
    piece(L" " + Loc::Get(files == 1 ? "mod_files_one" : "mod_files_many"), Kit::Palette::TextDim, MetaStyle());
    piece(L"   /   ", Kit::Palette::TextFaint, MetaStyle());
    piece(DateAdded(mod), Kit::Palette::TextDim, MetaStyle());
    piece(L"   /   ", Kit::Palette::TextFaint, MetaStyle());
    TextStyle statusStyle = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD);
    piece(Loc::Get(mod.isEnabled ? "mod_enabled" : "mod_disabled"), status, statusStyle);   // StatusText
    ui.PopClip();
}

} // namespace RivalsForge::Main

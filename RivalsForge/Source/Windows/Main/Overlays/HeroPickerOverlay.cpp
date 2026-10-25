#include "Windows/Main/Overlays/HeroPickerOverlay.h"
#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Data/Heroes.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Mods/ModsKit.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kTileW = 104.f, kTileH = 128.f, kTileGap = 10.f, kControlsH = 36.f, kButtonW = 100.f, kButtonH = 36.f;
constexpr float kTabW = 100.f, kTileSkew = 12.f;
const char* const kTabKeys[] = { "picker_all", "role_vanguard", "role_duelist", "role_strategist" };

int Columns(float width) { return std::max(1, static_cast<int>((width + kTileGap) / (kTileW + kTileGap))); }

} // namespace

void HeroPickerOverlay::Show(const std::wstring& mod, const std::wstring& currentHero, std::function<void(std::optional<std::wstring>)> callback) {
    modName = mod;
    current = currentHero;
    search.clear();
    role = -1;
    shownKey = L"\x01";   // anything: the grid comes in when the card opens
    focusPending = true;
    scrollPending = true;
    result.reset();
    done = std::move(callback);
    onClosed = [this] {
        auto callback = std::move(done);
        done = nullptr;
        if (callback) callback(result);
    };
    Open();
}

void HeroPickerOverlay::Pick(const std::wstring& hero) {
    if (!IsOpen()) return;
    result = hero;
    Close();
}

std::vector<std::wstring> HeroPickerOverlay::Shown() const {
    std::vector<std::wstring> shown;
    const std::wstring needle = Text::TrimDotNet(search);
    if (role < 0 && (needle.empty() || Text::Contains(Kit::HeroName(L""), needle, true))) shown.push_back(L"");
    for (const auto& hero : Data::Heroes()) {
        if (role >= 0 && static_cast<int>(hero.role) != role) continue;
        if (!needle.empty() && !Text::Contains(hero.name, needle, true)) continue;
        shown.push_back(hero.id);
    }
    return shown;
}

float HeroPickerOverlay::CardHeight(UI::Ui&, float) { return 640.f; }

void HeroPickerOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    const double now = ui.Time();
    float y = inner.y;

    // Title and the mod
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        TextStyle title = Kit::Display(26.f, DWRITE_FONT_WEIGHT_BOLD);
        const float th = Kit::LineH(ui, title);
        ui.g.Text(Kit::Upper(Loc::Get("picker_title")), { inner.x, y, inner.w, th }, Kit::Palette::Text, title);
        TextStyle sub = Kit::Style(12.f);
        sub.ellipsis = true;
        const float sh = Kit::LineH(ui, sub);
        ui.g.Text(modName, { inner.x, y + th + 2.f, inner.w, sh }, Kit::Palette::TextDim, sub);
        y += th + 2.f + sh + 14.f;
    }

    // Search box and the role tabs
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        UI::TextBoxStyle box = Kit::InputBoxStyle(13.f);
        box.placeholder = Loc::Get("picker_search");
        box.padding = Edges(12.f, 0.f);
        const UI::Id boxId = ui.MakeId("picker-search");
        if (focusPending && IsOpen()) {
            focusPending = false;
            ui.SetFocus(boxId);
        }
        bool entered = false;
        UI::TextBox(ui, boxId, { inner.x, y, 260.f, kControlsH }, search, box, &entered);
        if (entered || (IsOpen() && ui.KeyPressed(VK_RETURN))) {
            const auto shown = Shown();
            if (!shown.empty()) Pick(shown.front());
        }

        // Segments with a highlight that slides to the chosen one
        const Rect tabs{ inner.R() - 4.f * kTabW, y, 4.f * kTabW, kControlsH };
        Kit::FillSlant(ui, tabs, Kit::Palette::InputBg, 10.f);
        const float target = tabs.x + static_cast<float>(role + 1) * kTabW;
        const float hx = ui.Animate(ui.MakeId("picker-tab-x"), target, 18.f);
        if (std::fabs(hx - target) > 0.3f) ui.KeepAnimating();
        const Color tabColor = role < 0 ? Kit::Palette::Accent : Data::RoleColor(static_cast<Data::Role>(role));
        const Rect highlight{ hx + 3.f, tabs.y + 3.f, kTabW - 6.f, tabs.h - 6.f };
        ui.g.GlowRound(highlight.Inset(6.f, 0.f), 2.f, tabColor, 10.f, 0.35f * Kit::GlowStrength());
        Kit::FillSlant(ui, highlight, tabColor.Fade(0.9f), 8.f);
        for (int i = 0; i < 4; ++i) {
            const Rect seg{ tabs.x + i * kTabW, tabs.y, kTabW, tabs.h };
            const UI::Id id = ui.MakeId("picker-tab", i);
            const UI::Interaction it = ui.Interact(id, seg);
            const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            if (it.hovered) ui.SetCursor(IDC_HAND);
            TextStyle t = Kit::Display(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            t.align = HAlign::Center;
            t.valign = VAlign::Center;
            const bool on = role == i - 1;
            ui.g.Text(Kit::Upper(Loc::Get(kTabKeys[i])), seg, on ? Colors::White : Lerp(Kit::Palette::TextDim, Kit::Palette::Strong, hover), t);
            if (it.clicked) role = i - 1;
        }
        y += kControlsH + 14.f;
    }

    // The grid; it comes in again whenever the search or the tab changes
    const std::vector<std::wstring> shown = Shown();
    const std::wstring key = search + L"|" + std::to_wstring(role);
    if (key != shownKey) {
        shownKey = key;
        gridAt = gridAt < 0.0 ? OpenedAt() + 0.2 : now;
    }
    const float bottom = inner.B() - kButtonH - 14.f;
    const Rect gridR{ inner.x - 6.f, y, inner.w + 12.f, bottom - y };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2));
        ui.g.FillRect(gridR, Kit::Palette::Sunken);
        ui.g.StrokeRect(gridR.Inset(0.5f), Kit::Palette::Line, 1.f);
        const UI::Id gridId = ui.MakeId("picker-grid");
        const Rect viewport = gridR.Inset(Edges(10.f, 10.f, 6.f, 10.f));
        UI::ScrollArea scroll(ui, gridId, viewport, false, 64.f);
        const Rect area = scroll.viewport;
        const int cols = Columns(area.w);
        const float rowW = cols * kTileW + (cols - 1) * kTileGap;
        const float x0 = area.x + (area.w - rowW) * 0.5f;
        for (size_t i = 0; i < shown.size(); ++i) {
            const std::wstring& hero = shown[i];
            const int col = static_cast<int>(i) % cols, row = static_cast<int>(i) / cols;
            const Rect tile{ x0 + col * (kTileW + kTileGap), area.y + 4.f + row * (kTileH + kTileGap), kTileW, kTileH };
            if (!ui.Visible(tile, 30.f)) continue;
            const float in = M::Motion::Stagger(ui, gridAt, static_cast<int>(i), 0.012f, 0.3f, 48);
            const UI::Id id = ui.MakeId("picker-tile", static_cast<int64_t>(std::hash<std::wstring>{}(hero)));
            const UI::Interaction it = ui.Interact(id, tile);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
            const bool selected = hero == current;
            const Color roleColor = Kit::HeroColor(hero);

            M::Motion::Enter appear(ui, in, 16.f);
            ui.PushTranslate(0.f, -5.f * fb.hover + 2.f * fb.press);
            ui.PushScale(Anim::Lerp(0.85f, 1.f, in) * (1.f + 0.04f * fb.hover - 0.03f * fb.press), tile.CX(), tile.CY());
            if (selected) ui.g.GlowRound(tile.Inset(kTileSkew * 0.5f, 0.f), 2.f, Kit::Palette::Accent, 14.f, 0.6f * Kit::GlowStrength());
            else if (fb.hover > 0.f) ui.g.GlowRound(tile.Inset(kTileSkew * 0.5f, 0.f), 2.f, roleColor, 14.f, 0.5f * Kit::GlowStrength() * fb.hover);
            Kit::SlantPortrait(ui, hero, tile, kTileSkew, 1.f + 0.08f * fb.hover);
            // Name over a fade of the role colour at the foot, the role's line under it; a light
            // crosses the tile as the mouse comes in
            Kit::PushSlantClip(ui, tile, kTileSkew);
            ui.g.FillLinear(tile, 0.f, { { 0.45f, Colors::Black.Fade(0.f) }, { 0.8f, Colors::Black.Fade(0.75f) }, { 1.f, roleColor.Fade(0.6f) } },
                            { 0.f, 0.f }, { 0.f, 1.f });
            ui.g.FillRect({ tile.x, tile.B() - 3.f, tile.w, 3.f }, roleColor);
            const float sweep = ui.Tween(UI::HashId(id, 0x5EE), it.hovered ? 1.f : 0.f, 0.45f);
            if (it.hovered && sweep < 1.f) Kit::Sheen(ui, tile, sweep, 1.f);
            ui.g.PopLayer();
            TextStyle name = Kit::Display(13.f, DWRITE_FONT_WEIGHT_BOLD);
            name.align = HAlign::Center;
            name.valign = VAlign::Bottom;
            name.wrap = true;
            ui.g.Text(Kit::Upper(Kit::HeroName(hero)), { tile.x + 8.f, tile.y, tile.w - 16.f, tile.h - 8.f }, Colors::White, name);
            if (selected || fb.hover > 0.f)
                Kit::StrokeSlant(ui, tile, selected ? Kit::Palette::AccentHover : Colors::White.Fade(0.5f * fb.hover), selected ? 2.5f : 1.f, kTileSkew);
            if (selected) {
                // Check tag on the corner
                const Rect tag{ tile.R() - 30.f, tile.y + 6.f, 24.f, 18.f };
                Kit::FillSlant(ui, tag, Kit::Palette::Accent, 5.f);
                Kit::DrawGlyph(ui, Kit::Glyph::Check, tag.CX(), tag.CY(), 12.f, Colors::White);
            }
            ui.PopTransform();
            ui.PopTransform();
            ui.Tooltip(id, Kit::HeroName(hero));
            if (it.clicked) Pick(hero);
        }
        if (shown.empty()) {
            TextStyle none = Kit::Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
            ui.g.Text(Loc::Get("picker_no_results"), { area.x, area.y + 40.f, area.w, 20.f }, Kit::Palette::TextFaint, none);
        }
        const int rowsCount = (static_cast<int>(shown.size()) + cols - 1) / cols;
        scroll.SetContentSize(8.f + rowsCount * (kTileH + kTileGap));
    }
    if (scrollPending) {
        // The mod's hero in view (its row a little below the top). After the first frame of the
        // grid: before it, the scroll area doesn't know how tall the grid is and would keep it at 0
        scrollPending = false;
        const auto at = std::find(shown.begin(), shown.end(), current);
        const int cols = Columns(gridR.w - 16.f);
        const int row = at == shown.end() ? 0 : static_cast<int>(at - shown.begin()) / cols;
        UI::ScrollArea::ScrollTo(ui, ui.MakeId("picker-grid"), std::max(0.f, row * (kTileH + kTileGap) - kTileH * 0.5f));
    }

    // Cancel (a click on a hero picks it)
    M::Motion::Enter enter(ui, Reveal(ui, 3));
    TextStyle hint = Kit::Style(11.5f);
    hint.valign = VAlign::Center;
    ui.g.Text(Loc::Fmt("picker_count", static_cast<int>(shown.size())), { inner.x, bottom + 14.f, inner.w * 0.6f, kButtonH }, Kit::Palette::TextFaint, hint);
    if (Kit::Button(ui, ui.MakeId("picker-cancel"), { inner.R() - kButtonW, bottom + 14.f, kButtonW, kButtonH }, Loc::Get("cancel"),
                    Kit::DialogButton(false)))
        Close();
}

} // namespace RivalsForge::Main

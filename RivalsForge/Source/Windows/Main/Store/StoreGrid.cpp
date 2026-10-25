#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Store/StoreModOverlay.h"
#include "Windows/Main/Store/StoreView.h"

// The store's grid: as many columns as fit, each card the mod's picture (its category in a slanted
// tag, "Installed" / "Update" for the ones already here), its name, its author and its endorsements
// and downloads. Hovered, a card rises, its picture zooms and a light crosses it; a click opens the
// mod's card. "Load more" at the end; while the first page loads, cards shimmer in place; an error
// or an empty search says so.
namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;

constexpr float kMinCardW = 210.f, kGap = 16.f, kInfoH = 92.f;
constexpr size_t kPageSize = 24;   // the cards of each page come in one after the other

float CardHeight(float w) { return std::floor(w * 9.f / 16.f) + kInfoH; }

} // namespace

void StoreView::Card(UI::Ui& ui, const Rect& r, size_t index) {
    const StoreMod& mod = mods[index];
    const UI::Id id = ui.MakeId("store-card", mod.id);
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Models::Mod* installed = Installed(mod);
    // GameBanana's files have no version to compare: only Nexus mods say "Update"
    const bool update = installed && mod.source == Services::Store::Source::Nexus && !mod.version.empty() && !installed->nexusVersion.empty() &&
                        installed->nexusVersion != mod.version;

    ui.PushTranslate(0.f, -4.f * fb.hover + 1.5f * fb.press);
    if (fb.hover > 0.f) ui.g.GlowRound(r, 4.f, Kit::Palette::Accent, 16.f, 0.35f * Kit::GlowStrength() * fb.hover);
    ui.g.PushRoundClip(r, 4.f);
    ui.g.FillRect(r, Lerp(Kit::Palette::Panel, Kit::Palette::PanelRaised, fb.hover));

    // The picture, zooming on hover; adult mods keep theirs hidden
    const Rect pic{ r.x, r.y, r.w, r.h - kInfoH };
    ui.g.FillLinear(pic, 0.f, { { 0.f, Kit::IsLight() ? Hex(0xE4E4EA) : Hex(0x1E1E25) }, { 1.f, Kit::IsLight() ? Hex(0xD6D6DD) : Hex(0x131317) } }, { 0.f, 0.f }, { 0.f, 1.f });
    const ImagePtr image = mod.adult ? nullptr : Picture(mod.thumbnailUrl.empty() ? mod.pictureUrl : mod.thumbnailUrl, 480);
    if (image && image->bitmap && image->w > 0.f) {
        const float zoom = 1.f + 0.06f * fb.hover;
        const float scale = std::max(pic.w / image->w, pic.h / image->h) * zoom;
        const float w = image->w * scale, h = image->h * scale;
        ui.g.PushClip(pic);
        ui.g.DrawBitmap(image->bitmap.Get(), { pic.CX() - w * 0.5f, pic.CY() - h * 0.5f, w, h });
        ui.g.PopClip();
    } else {
        Kit::Halftone(ui, pic, 0.05f);
        TextStyle mark = Kit::Display(mod.adult ? 28.f : 14.f, DWRITE_FONT_WEIGHT_BOLD);
        mark.align = HAlign::Center;
        mark.valign = VAlign::Center;
        ui.g.Text(mod.adult ? L"18+" : Kit::Upper(Services::Store::SourceName(mod.source)), pic, Kit::Palette::TextFaint, mark);
    }
    ui.g.FillLinear(pic, 0.f, { { 0.55f, Colors::Black.Fade(0.f) }, { 1.f, Kit::Palette::Panel.Fade(0.9f) } }, { 0.f, 0.f }, { 0.f, 1.f });
    const float sweep = ui.Tween(UI::HashId(id, 0x5EE), it.hovered ? 1.f : 0.f, 0.5f);
    if (it.hovered && sweep < 1.f) Kit::Sheen(ui, pic, sweep, 0.8f);

    // Tags on the picture: the category, and installed / update
    TextStyle tag = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD);
    tag.align = HAlign::Center;
    tag.valign = VAlign::Center;
    if (!mod.category.empty()) {
        const std::wstring text = Kit::Upper(mod.category);
        const float w = std::ceil(ui.g.Measure(text, tag).w) + 20.f;
        const Rect t{ pic.x + 8.f, pic.y + 8.f, w, 20.f };
        Kit::FillSlant(ui, t, Colors::Black.Fade(0.65f), 6.f);
        ui.g.Text(text, t, Kit::Palette::TextDim, tag);
    }
    if (installed) {
        const std::wstring text = Kit::Upper(Loc::Get(update ? "store_update" : "store_installed"));
        const float w = std::ceil(ui.g.Measure(text, tag).w) + 20.f;
        const Rect t{ pic.R() - 8.f - w, pic.y + 8.f, w, 20.f };
        Kit::FillSlant(ui, t, update ? Kit::Palette::Gold : Kit::Palette::Enabled, 6.f);
        ui.g.Text(text, t, Hex(0x0B0B0E), tag);
    }

    // Name, author, numbers
    const Rect info{ r.x + 12.f, pic.B() + 8.f, r.w - 24.f, kInfoH - 14.f };
    TextStyle name = Kit::Style(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    name.ellipsis = true;
    const float lineH = Kit::LineH(ui, Kit::Style(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD));
    ui.g.PushClip({ info.x, info.y, info.w, lineH * 2.f + 1.f });
    ui.g.Text(mod.name, { info.x, info.y, info.w, lineH * 2.f + 1.f }, Kit::Palette::Text, name);
    ui.g.PopClip();
    TextStyle meta = Kit::Style(11.5f);
    meta.valign = VAlign::Center;
    meta.ellipsis = true;
    const float rowY = info.B() - 18.f;
    ui.g.Text(Loc::Fmt("store_by", mod.author), { info.x, rowY - 20.f, info.w, 18.f }, Kit::Palette::TextDim, meta);
    float x = info.x;
    const auto stat = [&](Kit::Glyph glyph, Color color, const std::wstring& text) {
        Kit::DrawGlyph(ui, glyph, x + 6.f, rowY + 9.f, 12.f, color);
        const float w = std::ceil(ui.g.Measure(text, meta).w);
        ui.g.Text(text, { x + 16.f, rowY, w + 2.f, 18.f }, Kit::Palette::TextDim, meta);
        x += 16.f + w + 14.f;
    };
    stat(Kit::Glyph::Heart, Kit::Palette::Accent, Number(mod.endorsements, true));
    // GameBanana's list has no downloads, only views
    if (mod.source == Services::Store::Source::GameBanana) stat(Kit::Glyph::Eye, Kit::Palette::Info, Number(mod.views, true));
    else stat(Kit::Glyph::Download, Kit::Palette::Info, Number(mod.downloads, true));

    // Being downloaded: a bar along the foot
    if (downloadingId == mod.id) {
        const float t = static_cast<float>(ui.Time());
        const Rect bar{ r.x, r.B() - 4.f, r.w, 4.f };
        ui.g.FillRect(bar, Kit::Palette::Ghost);
        ui.g.FillRect({ bar.x, bar.y, std::max(bar.w * state.progress, 8.f), bar.h }, Kit::Palette::Accent);
        ui.g.FillRect({ bar.x + std::fmod(t * 220.f, bar.w + 60.f) - 60.f, bar.y, 60.f, bar.h }, Kit::Palette::Shade.Fade(0.3f));
    }
    ui.g.PopLayer();
    ui.g.StrokeRound(r.Inset(0.5f), 4.f, Lerp(Kit::Palette::Line, Kit::Palette::Accent.Fade(0.8f), fb.hover), 1.f);
    ui.PopTransform();
    ui.Tooltip(id, mod.name);
    if (it.clicked) main.StoreModCard().Show(mod);
}

void StoreView::States(UI::Ui& ui, const Rect& r) {
    TextStyle title = Kit::Display(22.f, DWRITE_FONT_WEIGHT_BOLD);
    title.align = HAlign::Center;
    TextStyle text = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    const float w = std::min(480.f, r.w - 2.f * MainLayout::SideMargin), x = r.CX() - w * 0.5f;
    if (!error.empty()) {
        const float th = Kit::LineH(ui, title), eh = ui.g.Measure(error, text, w).h;
        float y = r.CY() - (th + 8.f + eh + 20.f + 36.f) * 0.5f;
        ui.g.Text(Kit::Upper(Loc::Get("store_error_title")), { x, y, w, th }, Kit::Palette::Text, title);
        y += th + 8.f;
        ui.g.Text(error, { x, y, w, eh }, Kit::Palette::TextDim, text);
        y += eh + 20.f;
        const Kit::ButtonStyle retry = Kit::Primary();
        const float bw = Kit::ButtonWidth(ui, Loc::Get("store_retry"), retry);
        if (Kit::Button(ui, ui.MakeId("store-retry"), { r.CX() - bw * 0.5f, y, bw, 36.f }, Loc::Get("store_retry"), retry)) Load(false);
        return;
    }
    const float th = Kit::LineH(ui, title);
    const float y = r.CY() - th;
    ui.g.Text(Kit::Upper(Loc::Get("store_empty_title")), { x, y, w, th }, Kit::Palette::Text, title);
    ui.g.Text(Loc::Get("store_empty_text"), { x, y + th + 6.f, w, 40.f }, Kit::Palette::TextDim, text);
}

void StoreView::Grid(UI::Ui& ui, const Rect& r) {
    const float side = MainLayout::SideMargin;
    if (mods.empty() && !loading && loadedOnce) {
        States(ui, r);
        return;
    }
    if (mods.empty() && !error.empty()) {
        States(ui, r);
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("store-scroll"), r);
    const float areaW = scroll.ContentWidth() - 2.f * side;
    const int cols = std::max(1, static_cast<int>((areaW + kGap) / (kMinCardW + kGap)));
    const float cardW = std::floor((areaW - kGap * static_cast<float>(cols - 1)) / static_cast<float>(cols));
    const float cardH = CardHeight(cardW);
    const float x0 = r.x + side;
    float y = r.y + 18.f;

    if (mods.empty()) {
        // The first page on its way: shimmering placeholders
        const float t = static_cast<float>(ui.Time());
        for (int i = 0; i < cols * 2; ++i) {
            const Rect c{ x0 + (i % cols) * (cardW + kGap), y + (i / cols) * (cardH + kGap), cardW, cardH };
            ui.g.FillRound(c, 4.f, Kit::Palette::Panel);
            ui.g.PushRoundClip(c, 4.f);
            Kit::Sheen(ui, c, std::fmod(t * 0.7f + i * 0.08f, 1.4f), 0.4f);
            ui.g.PopLayer();
        }
        ui.KeepAnimating();
        scroll.SetContentSize(r.h);
        return;
    }

    for (size_t i = 0; i < mods.size(); ++i) {
        const Rect c{ x0 + static_cast<float>(i % cols) * (cardW + kGap), y + static_cast<float>(i / cols) * (cardH + kGap), cardW, cardH };
        if (!ui.Visible(c, 40.f)) continue;
        // Each page's cards come in one after the other
        const float in = M::Motion::Stagger(ui, listShownAt, static_cast<int>(i % kPageSize), 0.03f, 0.35f, 40);
        M::Motion::Enter enter(ui, in, 18.f);
        Card(ui, c, i);
    }
    y += std::ceil(static_cast<float>(mods.size()) / static_cast<float>(cols)) * (cardH + kGap) + 6.f;

    // More of them
    if (static_cast<long long>(mods.size()) < total) {
        const Kit::ButtonStyle more = Kit::Ghost();
        const std::wstring text = loading ? Loc::Get("store_loading") : Loc::Get("store_more");
        const float bw = std::max(180.f, Kit::ButtonWidth(ui, text, more));
        if (Kit::Button(ui, ui.MakeId("store-more"), { r.CX() - bw * 0.5f, y, bw, 38.f }, text, more, !loading)) Load(true);
        y += 38.f + 24.f;
    }
    scroll.SetContentSize(y - r.y);
}

} // namespace RivalsForge::Main

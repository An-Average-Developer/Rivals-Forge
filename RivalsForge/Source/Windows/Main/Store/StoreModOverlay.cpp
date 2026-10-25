#include "Windows/Main/Store/StoreModOverlay.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Core/Time.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsKit.h"
#include "Windows/Main/Store/StoreAccountOverlay.h"
#include "Windows/Main/Store/StoreView.h"

namespace RivalsForge::Main {

namespace {

namespace M = UI::Material;
namespace Kit = ModsKit;
namespace Store = Services::Store;

constexpr float kBannerH = 210.f, kFooterH = 36.f, kFilesW = 330.f, kFilePad = 12.f, kInstallW = 104.f, kInstallH = 30.f;

std::wstring DateOf(const std::wstring& iso) {
    const auto when = Time::Parse(iso);
    return when ? Time::Format(*when, Loc::Get("date_format"), Loc::Get("date_locale")) : L"";
}

std::wstring DateOf(long long unix) {
    const std::time_t t = static_cast<std::time_t>(unix);
    std::tm utc{};
    if (gmtime_s(&utc, &t) != 0) return L"";
    wchar_t iso[32];
    swprintf_s(iso, L"%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
    return DateOf(std::wstring(iso));
}

// MAIN / OPTIONAL / ... in the user's language, with a colour
std::pair<std::wstring, Color> CategoryOf(const std::wstring& category) {
    if (category == L"MAIN") return { Loc::Get("store_file_main"), Kit::Palette::Accent };
    if (category == L"OPTIONAL") return { Loc::Get("store_file_optional"), Kit::Palette::Info };
    if (category == L"UPDATE") return { Loc::Get("store_file_update"), Kit::Palette::Gold };
    return { Loc::Get("store_file_misc"), Kit::Palette::TextDim };
}

TextStyle FileNameStyle() { return Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true); }
TextStyle FileTextStyle() { return Kit::Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true); }

} // namespace

void StoreModOverlay::Show(const Store::StoreMod& m) {
    mod = m;
    details.reset();
    error.clear();
    Load();
    Open();
}

void StoreModOverlay::Load() {
    const unsigned request = ++generation;
    loading = true;
    error.clear();
    struct Result {
        Store::ModDetails details;
        std::optional<std::wstring> error;
    };
    Async::Run(
        [id = mod.id]() -> Result {
            try {
                return { Store::FetchDetails(id), std::nullopt };
            } catch (const Store::Error& e) {
                return { {}, e.Message() };
            }
        },
        [this, request](Result result) {
            if (request != generation) return;
            loading = false;
            if (result.error) {
                error = *result.error;
                return;
            }
            details = std::move(result.details);
            mod = details->mod;   // the newest numbers (and everything, when only the id was known)
        });
}

float StoreModOverlay::CardHeight(UI::Ui&, float) { return 680.f; }

float StoreModOverlay::FileHeight(UI::Ui& ui, const Store::ModFile& file, float width) const {
    const float w = width - 2.f * kFilePad;
    float h = kFilePad + ui.g.Measure(file.name, FileNameStyle(), w).h + 4.f + 16.f + 8.f;
    if (!file.description.empty()) h += std::min(ui.g.Measure(file.description, FileTextStyle(), w).h, 46.f) + 8.f;
    return h + kInstallH + kFilePad;
}

void StoreModOverlay::Banner(UI::Ui& ui, const Rect& r) {
    StoreView& store = main.Store();
    ui.g.FillLinear(r, 0.f, { { 0.f, Kit::IsLight() ? Hex(0xE6E6EC) : Hex(0x24242C) }, { 1.f, Kit::Palette::Dialog } }, { 0.f, 0.f }, { 0.f, 1.f });
    if (const ImagePtr image = mod.adult ? nullptr : store.Picture(mod.pictureUrl.empty() ? mod.thumbnailUrl : mod.pictureUrl, 1000);
        image && image->bitmap && image->w > 0.f) {
        // Cover, drifting slowly
        const float scale = std::max(r.w / image->w, r.h / image->h) * 1.06f;
        const float w = image->w * scale, h = image->h * scale;
        const float drift = std::sin(static_cast<float>(ui.Time()) * 0.3f) * (w - r.w) * 0.4f;
        ui.KeepAnimatingThrottled();
        ui.g.PushClip(r);
        ui.g.DrawBitmap(image->bitmap.Get(), { r.CX() - w * 0.5f + drift, r.CY() - h * 0.5f, w, h });
        ui.g.PopClip();
    } else {
        Kit::Halftone(ui, r, 0.06f);
    }
    ui.g.FillLinear(r, 0.f, { { 0.25f, Kit::Palette::Dialog.Fade(0.f) }, { 1.f, Kit::Palette::Dialog } }, { 0.f, 0.f }, { 0.f, 1.f });
    ui.g.FillLinear(r, 0.f, { { 0.f, Kit::Palette::Dialog.Fade(0.85f) }, { 0.6f, Kit::Palette::Dialog.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });

    const Rect inner = r.Inset(Edges(kPad, 0.f, kPad, 14.f));
    TextStyle name = Kit::Display(30.f, DWRITE_FONT_WEIGHT_BOLD);
    name.ellipsis = true;
    const float nameH = Kit::LineH(ui, name);
    float y = inner.B() - nameH - 24.f;
    if (!mod.category.empty()) {
        TextStyle tag = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD);
        tag.align = HAlign::Center;
        tag.valign = VAlign::Center;
        const std::wstring text = Kit::Upper(mod.category);
        const Rect t{ inner.x, y - 26.f, std::ceil(ui.g.Measure(text, tag).w) + 22.f, 20.f };
        Kit::FillSlant(ui, t, Kit::Palette::Accent, 6.f);
        ui.g.Text(text, t, Colors::White, tag);
    }
    ui.g.Text(Kit::Upper(mod.name), { inner.x, y, inner.w, nameH }, Kit::Palette::Text, name);
    TextStyle by = Kit::Style(12.5f);
    by.valign = VAlign::Center;
    std::wstring line = Loc::Fmt("store_by", mod.author);
    if (!mod.version.empty()) line += L"   /   " + Loc::Fmt("store_version", mod.version);
    ui.g.Text(line, { inner.x, y + nameH + 2.f, inner.w, 20.f }, Kit::Palette::Body, by);
}

void StoreModOverlay::Description(UI::Ui& ui, const Rect& r) {
    // The numbers
    TextStyle num = Kit::Display(18.f, DWRITE_FONT_WEIGHT_BOLD, false);
    num.valign = VAlign::Center;
    TextStyle cap = Kit::Display(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    cap.valign = VAlign::Center;
    float x = r.x;
    const auto stat = [&](Kit::Glyph glyph, Color color, const std::wstring& value, const std::wstring& caption) {
        Kit::DrawGlyph(ui, glyph, x + 8.f, r.y + 14.f, 15.f, color);
        const float vw = std::ceil(ui.g.Measure(value, num).w);
        ui.g.Text(value, { x + 22.f, r.y, vw + 2.f, 28.f }, Kit::Palette::Text, num);
        const std::wstring c = Kit::Upper(caption);
        const float cw = std::ceil(ui.g.Measure(c, cap).w);
        ui.g.Text(c, { x + 28.f + vw, r.y + 1.f, cw + 2.f, 28.f }, Kit::Palette::TextDim, cap);
        x += 28.f + vw + cw + 22.f;
    };
    stat(Kit::Glyph::Heart, Kit::Palette::Accent, StoreView::Number(mod.endorsements, true), Loc::Get("store_endorsements"));
    stat(Kit::Glyph::Download, Kit::Palette::Info, StoreView::Number(mod.downloads, true), Loc::Get("store_downloads"));
    if (!mod.updatedAt.empty()) stat(Kit::Glyph::Pencil, Kit::Palette::Gold, DateOf(mod.updatedAt), Loc::Get("store_updated"));

    const Rect body{ r.x, r.y + 40.f, r.w, r.h - 40.f };
    if (!details) {
        TextStyle t = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
        if (!error.empty()) {
            ui.g.Text(error, { body.x, body.y, body.w, 60.f }, Kit::Palette::Danger, t);
            if (Kit::Button(ui, ui.MakeId("smod-retry"), { body.x, body.y + 64.f, 120.f, 32.f }, Loc::Get("store_retry"), Kit::Ghost())) Load();
        } else {
            for (int i = 0; i < 6; ++i) {
                const Rect line{ body.x, body.y + i * 22.f, body.w * (i % 3 == 2 ? 0.6f : 0.95f), 12.f };
                ui.g.FillRound(line, 3.f, Kit::Palette::Panel);
                ui.g.PushRoundClip(line, 3.f);
                Kit::Sheen(ui, line, std::fmod(static_cast<float>(ui.Time()) * 0.7f + i * 0.1f, 1.4f), 0.5f);
                ui.g.PopLayer();
            }
            ui.KeepAnimating();
        }
        return;
    }
    // The summary, then the description, scrolling
    UI::ScrollArea scroll(ui, ui.MakeId("smod-desc"), body);
    const float w = scroll.ContentWidth() - 10.f;
    float y = body.y;
    if (!mod.summary.empty()) {
        TextStyle summary = Kit::Style(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
        const float h = ui.g.Measure(mod.summary, summary, w).h;
        ui.g.Text(mod.summary, { body.x, y, w, h }, Kit::Palette::Text, summary);
        y += h + 12.f;
    }
    TextStyle text = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const std::wstring description = details->description.empty() ? Loc::Get("store_no_description") : details->description;
    const float h = ui.g.Measure(description, text, w).h;
    ui.g.Text(description, { body.x, y, w, h }, Kit::Palette::Body, text);
    scroll.SetContentSize(y + h + 8.f - body.y);
}

void StoreModOverlay::Files(UI::Ui& ui, const Rect& r) {
    StoreView& store = main.Store();
    TextStyle head = Kit::Display(13.f, DWRITE_FONT_WEIGHT_BOLD);
    head.valign = VAlign::Center;
    ui.g.Text(Kit::Upper(Loc::Get("store_files")), { r.x, r.y, r.w, 22.f }, Kit::Palette::TextDim, head);
    float y = r.y + 30.f;

    // What the account means for downloading
    const StoreView::Account& account = store.GetAccount();
    std::wstring note;
    Color noteColor = Kit::Palette::Gold;
    if (account.key.empty() || (!account.info && !account.checking)) note = Loc::Get("store_note_no_account");
    else if (account.info && !account.info->premium) note = Loc::Get("store_note_free");
    if (!note.empty()) {
        TextStyle t = Kit::Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
        const float h = ui.g.Measure(note, t, r.w - 34.f).h + 16.f;
        const Rect box{ r.x, y, r.w, h };
        const UI::Id id = ui.MakeId("smod-account");
        const UI::Interaction it = ui.Interact(id, box);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
        Kit::FillSlant(ui, box, noteColor.Fade(0.1f + 0.08f * hover), 8.f);
        Kit::EdgeStripe(ui, box, 4.f, noteColor, 8.f);
        ui.g.Text(note, box.Inset(Edges(20.f, 8.f, 12.f, 8.f)), noteColor, t);
        if (it.clicked) main.StoreAccount().Show(false);
        y += h + 10.f;
    }

    if (!details) return;
    UI::ScrollArea scroll(ui, ui.MakeId("smod-files"), { r.x, y, r.w, r.B() - y });
    const float w = scroll.ContentWidth() - 8.f;
    const Models::Mod* installed = store.Installed(mod.id);
    float fy = y;
    if (details->files.empty()) {
        TextStyle t = Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        ui.g.Text(Loc::Get("store_no_files"), { r.x, fy, w, 40.f }, Kit::Palette::TextFaint, t);
    }
    for (size_t i = 0; i < details->files.size(); ++i) {
        const Store::ModFile& file = details->files[i];
        const float h = FileHeight(ui, file, w);
        const Rect card{ r.x, fy, w, h };
        fy += h + 8.f;
        if (!ui.Visible(card, 20.f)) continue;
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, OpenedAt() + 0.3, static_cast<int>(i), 0.05f, 0.3f), 0.f, 16.f);
        ui.g.FillRound(card, 3.f, Kit::Palette::Panel);
        const auto [category, color] = CategoryOf(file.category);
        ui.g.FillRect({ card.x, card.y, 3.f, card.h }, color);
        const Rect inner = card.Inset(kFilePad);
        float cy = inner.y;
        const float nh = ui.g.Measure(file.name, FileNameStyle(), inner.w).h;
        ui.g.Text(file.name, { inner.x, cy, inner.w, nh }, Kit::Palette::Text, FileNameStyle());
        cy += nh + 4.f;
        // The category tag, then version / size / date
        TextStyle tag = Kit::Display(10.5f, DWRITE_FONT_WEIGHT_BOLD);
        tag.align = HAlign::Center;
        tag.valign = VAlign::Center;
        const std::wstring tagText = Kit::Upper(category);
        const Rect t{ inner.x, cy, std::ceil(ui.g.Measure(tagText, tag).w) + 18.f, 16.f };
        Kit::FillSlant(ui, t, color.Fade(0.2f), 5.f);
        ui.g.Text(tagText, t, color, tag);
        TextStyle meta = Kit::Style(11.f);
        meta.valign = VAlign::Center;
        meta.ellipsis = true;
        std::wstring info;
        if (!file.version.empty()) info += L"v" + file.version + L"   ";
        info += Text::FormatBytes(static_cast<unsigned long long>(file.size));
        if (file.date) info += L"   " + DateOf(file.date);
        ui.g.Text(info, { t.R() + 8.f, cy, inner.R() - t.R() - 8.f, 16.f }, Kit::Palette::TextDim, meta);
        cy += 16.f + 8.f;
        if (!file.description.empty()) {
            const float dh = std::min(ui.g.Measure(file.description, FileTextStyle(), inner.w).h, 46.f);
            ui.g.PushClip({ inner.x, cy, inner.w, dh });
            ui.g.Text(file.description, { inner.x, cy, inner.w, 200.f }, Kit::Palette::TextFaint, FileTextStyle());
            ui.g.PopClip();
            cy += dh + 8.f;
        }
        // Install (and "Installed" for the version here)
        const bool here = installed && !file.version.empty() && installed->nexusVersion == file.version;
        Kit::ButtonStyle style = here ? Kit::Ghost() : Kit::Primary();
        style.sheen = false;
        style.fontSize = 13.f;
        style.padding = Edges(12.f, 4.f);
        const std::wstring label = Loc::Get(here ? "store_reinstall" : "store_install");
        const float bw = std::max(kInstallW, Kit::ButtonWidth(ui, label, style));
        const UI::Id id = ui.MakeId("smod-install", file.id);
        if (Kit::Button(ui, id, { inner.R() - bw, cy, bw, kInstallH }, label, style, !store.Busy())) {
            store.Install(mod, file);
            if (account.info && account.info->premium) Close();
        }
        if (here) {
            TextStyle ok = Kit::Display(11.f, DWRITE_FONT_WEIGHT_BOLD);
            ok.valign = VAlign::Center;
            Kit::DrawGlyph(ui, Kit::Glyph::Check, inner.x + 7.f, cy + kInstallH * 0.5f, 12.f, Kit::Palette::Enabled);
            ui.g.Text(Kit::Upper(Loc::Get("store_installed")), { inner.x + 18.f, cy, 140.f, kInstallH }, Kit::Palette::Enabled, ok);
        }
    }
    scroll.SetContentSize(fy - y);
}

void StoreModOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        // Inside the card's cut corners, under its accent line
        PushCardClip(ui, card);
        Banner(ui, { card.x, card.y + 3.f, card.w, kBannerH });
        ui.g.PopLayer();
    }
    const Rect body{ card.x + kPad, card.y + kBannerH + 14.f, card.w - 2.f * kPad, card.h - kBannerH - 14.f - kPad - kFooterH - 16.f };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        Description(ui, { body.x, body.y, body.w - kFilesW - 24.f, body.h });
    }
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2));
        ui.g.FillRect({ body.R() - kFilesW - 12.f, body.y, 1.f, body.h }, Kit::Palette::Line);
        Files(ui, { body.R() - kFilesW, body.y, kFilesW, body.h });
    }
    // View on Nexus / Close
    M::Motion::Enter enter(ui, Reveal(ui, 3));
    const float fy = card.B() - kPad - kFooterH;
    const Kit::ButtonStyle ghost = Kit::DialogButton(false);
    const std::wstring site = Loc::Get("store_view_site");
    const float sw = Kit::ButtonWidth(ui, site, ghost) + 20.f;
    const Rect siteR{ card.x + kPad, fy, sw, kFooterH };
    if (Kit::Button(ui, ui.MakeId("smod-site"), siteR, L"   " + site, ghost)) Process::OpenUrl(mod.PageUrl());
    Kit::DrawGlyph(ui, Kit::Glyph::External, siteR.x + 22.f, siteR.CY(), 13.f, Kit::Palette::TextDim);
    if (Kit::Button(ui, ui.MakeId("smod-close"), { card.R() - kPad - 110.f, fy, 110.f, kFooterH }, Loc::Get("store_close"), Kit::DialogButton(true)))
        Close();
}

} // namespace RivalsForge::Main

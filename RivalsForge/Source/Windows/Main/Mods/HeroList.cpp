#include <algorithm>
#include "Graphics/Animation.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Mods/ModsView.h"

// The list (was <ScrollViewer x:Name="SectionsScroll" Padding="28,12,28,16">): the hero strip on
// top, then one card per hero, one under the other. The cards follow MainState: a new card grows
// in, a removed one (its last mod gone, filtered out, moved to another hero) shrinks away, and a
// reload brings them all in one after the other.
namespace RivalsForge::Main {

namespace {
constexpr double kAppear = 0.4, kLeave = 0.25;
constexpr float kGap = 14.f;
} // namespace

int64_t ModsView::Key(const std::wstring& id) { return id.empty() ? 0 : static_cast<int64_t>(std::hash<std::wstring>{}(id)); }

float ModsView::Appear(double shownAt, double now) { return Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((now - shownAt) / kAppear))); }

float ModsView::Leave(double hiddenAt, double now) {
    return hiddenAt < 0.0 ? 0.f : Anim::EaseInCubic(Anim::Clamp01(static_cast<float>((now - hiddenAt) / kLeave)));
}

// Brings `rows` in line with MainState: kept cards update, new ones come in where they belong,
// removed ones start leaving where they were
void ModsView::SyncRows(double now) {
    const bool reload = seenRevision != state.revision;
    if (reload) {
        rows.clear();
        seenRevision = state.revision;
    }
    const auto& svc = state.Service();
    const auto groups = state.ShownGroups();

    // A hero's mods: the same merge as the heroes below
    const auto syncMods = [&](HeroRow& row, const MainState::Group& group, double startAt) {
        std::vector<ModRow> next;
        size_t e = 0;
        const auto add = [&](const std::wstring& id, double at) {
            if (const Models::Mod* mod = svc.FindMod(id)) next.push_back({ id, *mod, at, -1.0 });
        };
        for (auto& m : row.mods) {
            const auto from = group.modIds.begin() + static_cast<std::ptrdiff_t>(e);
            const auto it = m.hiddenAt < 0.0 ? std::find(from, group.modIds.end(), m.id) : group.modIds.end();
            if (it == group.modIds.end()) {
                if (m.hiddenAt < 0.0) m.hiddenAt = now;
                next.push_back(m);
                continue;
            }
            const size_t pos = static_cast<size_t>(it - group.modIds.begin());
            for (; e < pos; ++e) add(group.modIds[e], now);
            if (const Models::Mod* mod = svc.FindMod(m.id)) m.mod = *mod;
            next.push_back(m);
            e = pos + 1;
        }
        for (int i = 0; e < group.modIds.size(); ++e, ++i) add(group.modIds[e], reload ? startAt + 0.04 * i : now);
        std::erase_if(next, [&](const ModRow& m) { return m.hiddenAt >= 0.0 && now - m.hiddenAt > kLeave; });
        row.mods = std::move(next);
    };
    const auto make = [&](const MainState::Group& group, double at) {
        HeroRow row;
        row.hero = group.hero;
        row.shownAt = at;
        syncMods(row, group, at + 0.12);
        return row;
    };

    std::vector<HeroRow> next;
    size_t e = 0;
    for (auto& row : rows) {
        size_t pos = groups.size();
        if (row.hiddenAt < 0.0)
            for (size_t i = e; i < groups.size(); ++i)
                if (groups[i].hero == row.hero) {
                    pos = i;
                    break;
                }
        if (pos == groups.size()) {
            // Gone: its mods leave with it
            if (row.hiddenAt < 0.0) row.hiddenAt = now;
            next.push_back(row);
            continue;
        }
        for (; e < pos; ++e) next.push_back(make(groups[e], now));
        syncMods(row, groups[pos], row.shownAt + 0.12);
        next.push_back(row);
        e = pos + 1;
    }
    for (int i = 0; e < groups.size(); ++e, ++i) next.push_back(make(groups[e], reload ? now + 0.07 * i : now));
    std::erase_if(next, [&](const HeroRow& r) { return r.hiddenAt >= 0.0 && now - r.hiddenAt > kLeave; });
    rows = std::move(next);
}

void ModsView::HeroList(UI::Ui& ui, const Rect& r) {
    const double now = ui.Time();
    const float stripH = HeroStrip(ui, r);
    const Rect list{ r.x, r.y + stripH, r.w, r.h - stripH };
    SyncRows(now);

    UI::ScrollArea scroll(ui, ui.MakeId("heroes-scroll"), list);
    const float x = list.x + MainLayout::SideMargin, w = list.w - 2.f * MainLayout::SideMargin;
    float y = list.y + 10.f;
    for (const HeroRow& row : rows) {
        const float appear = Appear(row.shownAt, now), leave = Leave(row.hiddenAt, now);
        if (appear < 1.f || leave > 0.f) ui.KeepAnimating();
        const float open = Anim::EaseInOutCubic(ui.Tween(ui.MakeId("hero-open", Key(row.hero)), state.Expanded(row.hero) ? 1.f : 0.f, 0.3f));
        if (open > 0.f && open < 1.f) ui.KeepAnimating();

        const float full = HeroCardHeight(ui, row, open, now);
        const float k = appear * (1.f - leave);
        const float h = full * k;
        if (h > 0.5f && ui.Visible({ x, y, w, h }, 40.f)) {
            ui.PushClip({ x - 24.f, y - 24.f, w + 48.f, h + 24.f });
            {
                // (the entrance's layer has to close before the clip: D2D pops in order). The card
                // comes in from the left and rises, a leaving one sinks back
                UI::Material::Motion::Enter enter(ui, k, 18.f * (1.f - appear) - 8.f * leave, -36.f * (1.f - appear));
                HeroCard(ui, { x, y, w, full }, row, open, row.hiddenAt < 0.0, now);
            }
            ui.PopClip();
        }
        y += (full + kGap) * k;
    }
    scroll.SetContentSize(y - list.y + 16.f - kGap);
}

} // namespace RivalsForge::Main

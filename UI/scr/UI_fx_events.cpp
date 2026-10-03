// =============================================================================
// [b-effects] FxEventTracker implementation (see UI_fx_events.h)
// =============================================================================
#include "../includes/UI_fx_events.h"

#include <algorithm>
#include <cmath>

namespace {

bool samePos(sf::Vector2f a, sf::Vector2f b) {
    return std::fabs(a.x - b.x) < 0.5f && std::fabs(a.y - b.y) < 0.5f;
}

} // namespace

FxEventTracker::Snapshot FxEventTracker::capture(const GameEngine& engine) {
    Snapshot s;
    for (int p = 0; p < 2; ++p) {
        s.mineCount[p] = engine.getLastMineAction(p + 1).count;
        const PlayerEconomy& econ = engine.getPlayerEconomy(p + 1);
        for (int r = 0; r < 8; ++r) s.mineLevels[p][r] = econ.mineLevels[r];
    }
    const auto& bl = engine.getBuildings();
    s.buildings.reserve(bl.size());
    for (const auto& b : bl) s.buildings.push_back({ b.type, b.playerOwner, b.position });
    for (const auto& plot : engine.getLandPlots()) {
        if (plot.isPurchased) s.purchasedPlotIds.push_back(plot.id);
    }
    s.p1Share = engine.getCityState().p1CityShare;
    s.day = engine.getCurrentDay();
    s.daylight = engine.isDaylight();
    s.season = engine.getSeason();
    s.winner = engine.getCityState().winner;
    return s;
}

void FxEventTracker::reset(const GameEngine& engine) {
    prev = capture(engine);
    valid = true;
}

void FxEventTracker::poll(const GameEngine& engine, std::vector<FxEvent>& out) {
    Snapshot cur = capture(engine);
    if (!valid || cur.day < prev.day || cur.mineCount[0] < prev.mineCount[0] || cur.mineCount[1] < prev.mineCount[1]) {
        // First frame or the match was restarted behind our back: resync silently
        prev = cur;
        valid = true;
        return;
    }

    // Mining (engine hook counts every successful action, human or bot)
    for (int p = 0; p < 2; ++p) {
        if (cur.mineCount[p] > prev.mineCount[p]) {
            const auto& rec = engine.getLastMineAction(p + 1);
            FxEvent e;
            e.type = FxEvent::Type::Mined;
            e.player = p + 1;
            e.resource = rec.type;
            e.amount = rec.amount;
            out.push_back(e);
        }
    }

    // Buildings placed / removed
    for (const auto& b : cur.buildings) {
        bool existed = std::any_of(prev.buildings.begin(), prev.buildings.end(), [&](const BuildingKey& o) {
            return o.owner == b.owner && o.type == b.type && samePos(o.pos, b.pos);
        });
        if (!existed) {
            FxEvent e;
            e.type = FxEvent::Type::Built;
            e.player = b.owner;
            e.building = b.type;
            e.pos = b.pos;
            out.push_back(e);
        }
    }
    for (const auto& b : prev.buildings) {
        bool still = std::any_of(cur.buildings.begin(), cur.buildings.end(), [&](const BuildingKey& o) {
            return o.owner == b.owner && o.type == b.type && samePos(o.pos, b.pos);
        });
        if (!still) {
            FxEvent e;
            e.type = FxEvent::Type::Removed;
            e.player = b.owner;
            e.building = b.type;
            e.pos = b.pos;
            out.push_back(e);
        }
    }

    // Land purchases
    for (int id : cur.purchasedPlotIds) {
        if (std::find(prev.purchasedPlotIds.begin(), prev.purchasedPlotIds.end(), id) != prev.purchasedPlotIds.end()) continue;
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.id != id) continue;
            FxEvent e;
            e.type = FxEvent::Type::LandBought;
            e.player = plot.playerOwner;
            e.area = plot.bounds;
            e.pos = plot.bounds.position + plot.bounds.size * 0.5f;
            out.push_back(e);
        }
    }

    // Mine upgrades
    for (int p = 0; p < 2; ++p) {
        for (int r = 1; r < 8; ++r) {
            if (cur.mineLevels[p][r] > prev.mineLevels[p][r]) {
                FxEvent e;
                e.type = FxEvent::Type::MineUpgraded;
                e.player = p + 1;
                e.resource = static_cast<ResourceType>(r);
                e.amount = cur.mineLevels[p][r];
                out.push_back(e);
            }
        }
    }

    // Day-end settlement: the share only moves at the 06:00 verdict
    if (cur.day > prev.day) {
        float delta = cur.p1Share - prev.p1Share;
        FxEvent e;
        e.type = FxEvent::Type::Settlement;
        if (delta > 0.0005f) {
            e.player = 1;
            e.value = delta;
        } else if (delta < -0.0005f) {
            e.player = 2;
            e.value = -delta;
        }
        out.push_back(e);
    }

    if (cur.winner != 0 && prev.winner == 0) {
        FxEvent e;
        e.type = FxEvent::Type::Victory;
        e.player = cur.winner;
        out.push_back(e);
    }

    if (cur.daylight != prev.daylight) {
        FxEvent e;
        e.type = cur.daylight ? FxEvent::Type::DayBreak : FxEvent::Type::Nightfall;
        out.push_back(e);
    }

    if (cur.season != prev.season) {
        FxEvent e;
        e.type = FxEvent::Type::SeasonChanged;
        e.season = cur.season;
        out.push_back(e);
    }

    prev = std::move(cur);
}

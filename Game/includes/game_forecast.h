#pragma once
#include "game_main.h"

// =============================================================================
// [AI team] Power forecast helpers (read-only, built only on GameEngine's public getters)
//
// The day verdict is the AVERAGE power delivered from 06:00 to 06:00, and the weather of a
// sector only changes at 06:00, so the rest of the day being settled can be projected exactly
// from the buildings standing now: sun arc, wind hour curve, hydro, lamps first, batteries
// (same rules as GameEngine::updateBuildingsEnergy). Used by the bot governor (BAL-01) and by
// the НЕВЪЗМОЖНО bot's "perfect weather foresight" (worst case over every weather type).
// =============================================================================
namespace Forecast {

// City demand of the day after `day` (0 during the grace period, 30 MW on day 3, then +15 MW/day)
int demandForDay(int day);

// Game-hours left until the next 06:00 settlement (0 < result <= 24)
float hoursUntilRollover(const GameEngine& engine);

// Average MW the player will have delivered when the current day is settled, if nothing changes
float projectTodayAverageMW(const GameEngine& engine, int player);

// Average MW over a full 06:00 -> 06:00 day with the given weather and season (batteries start
// empty; season of the 00:00-06:00 part = the next day's season is ignored, it is dark anyway)
float fullDayAverageMW(const GameEngine& engine, int player, WeatherType weather, SeasonType season, int demandMW);

// Lowest fullDayAverageMW over every weather type: output the player can count on tomorrow
float worstCaseDayAverageMW(const GameEngine& engine, int player, SeasonType season, int demandMW);

// Output of one new building under `weather` averaged over a full day (0 for battery / lamp)
float buildingDayAverageMW(BuildingType type, WeatherType weather, SeasonType season);

} // namespace Forecast

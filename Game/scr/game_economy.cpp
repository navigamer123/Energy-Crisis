// =============================================================================
// ENERGY CRISIS - CITY ECONOMY SIMULATION                     [team b-economy]
// Districts, hourly demand curve, proportional verdict, anti-snowball damping,
// grid frequency events and the energy / CO2 ledger. See game_economy.h.
// The GameEngine accessors of this system are defined at the bottom of this file.
// =============================================================================
#include "../includes/game_economy.h"
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Econ {

namespace {

// Raw hourly shapes (hour 0..23). They are normalised to a mean of 1.0 at start-up, so only
// the shape matters; the weights below decide how much of the city each district is.
const float RAW_PROFILE[DISTRICT_COUNT][24] = {
    // Hospital: flat 24/7 critical load
    { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
      1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
    // Industry: steady base load, a little lower during the night shift
    { 0.95f, 0.95f, 0.95f, 0.95f, 0.95f, 0.95f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f,
      1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 0.95f, 0.95f },
    // Business: offices, shops and schools during working hours
    { 0.40f, 0.40f, 0.40f, 0.40f, 0.40f, 0.40f, 0.55f, 0.95f, 1.35f, 1.50f, 1.50f, 1.50f,
      1.50f, 1.50f, 1.50f, 1.50f, 1.45f, 1.30f, 0.95f, 0.70f, 0.55f, 0.48f, 0.44f, 0.42f },
    // Homes: breakfast bump, quiet day, big evening peak (cooking, lights, TV)
    { 0.62f, 0.58f, 0.56f, 0.56f, 0.58f, 0.62f, 0.85f, 1.05f, 0.80f, 0.55f, 0.50f, 0.50f,
      0.52f, 0.52f, 0.52f, 0.55f, 0.75f, 1.75f, 2.30f, 2.50f, 2.45f, 2.30f, 1.35f, 0.85f },
};

const DistrictDef DISTRICTS[DISTRICT_COUNT] = {
    { "БОЛНИЦА",   "24/7 критичен товар",   0.15f },
    { "ИНДУСТРИЯ", "постоянен базов товар", 0.20f },
    { "БИЗНЕС",    "работно време 08-17",   0.30f },
    { "ДОМОВЕ",    "вечерен пик 17-22",     0.35f },
};

struct ProfileTables {
    float district[DISTRICT_COUNT][24];
    float city[24];
    ProfileTables() {
        for (int d = 0; d < DISTRICT_COUNT; ++d) {
            float sum = 0.0f;
            for (int h = 0; h < 24; ++h) sum += RAW_PROFILE[d][h];
            for (int h = 0; h < 24; ++h) district[d][h] = RAW_PROFILE[d][h] * 24.0f / sum;
        }
        float weightSum = 0.0f;
        for (int d = 0; d < DISTRICT_COUNT; ++d) weightSum += DISTRICTS[d].weight;
        for (int h = 0; h < 24; ++h) {
            float v = 0.0f;
            for (int d = 0; d < DISTRICT_COUNT; ++d) v += DISTRICTS[d].weight / weightSum * district[d][h];
            city[h] = v;
        }
    }
};

const ProfileTables& tables() {
    static const ProfileTables t;
    return t;
}

int hourIndex(float hour24) {
    int h = static_cast<int>(std::floor(hour24));
    h %= 24;
    if (h < 0) h += 24;
    return h;
}

int clampDistrict(int d) { return std::max(0, std::min(DISTRICT_COUNT - 1, d)); }

float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

float playerShareOf(const DistrictState& ds, int player) { return (player == 1) ? ds.p1Share : 1.0f - ds.p1Share; }

const float GAME_SECONDS_PER_HOUR = Balance::SECONDS_PER_DAY / 24.0f;

std::string pct(float fraction) {
    return std::to_string(static_cast<int>(std::lround(clamp01(fraction) * 100.0f))) + "%";
}

} // namespace

// -----------------------------------------------------------------------------
// Profiles
// -----------------------------------------------------------------------------
const DistrictDef& getDistrictDef(int district) { return DISTRICTS[clampDistrict(district)]; }

float getDistrictProfile(int district, int hour) {
    return tables().district[clampDistrict(district)][((hour % 24) + 24) % 24];
}

float getDistrictProfileAt(int district, float hour24) { return getDistrictProfile(district, hourIndex(hour24)); }

float getCityDemandProfile(int hour) { return tables().city[((hour % 24) + 24) % 24]; }

float getCityDemandProfileAt(float hour24) { return getCityDemandProfile(hourIndex(hour24)); }

float getPeakPriceFactor(float hour24) { return getCityDemandProfileAt(hour24); }

bool isPeakHour(float hour24) { return getCityDemandProfileAt(hour24) >= PEAK_PROFILE_THRESHOLD; }

const char* getSourceNameBg(int source) {
    switch (source) {
        case SRC_SOLAR: return "Слънце";
        case SRC_WIND: return "Вятър";
        case SRC_HYDRO: return "ВЕЦ";
        case SRC_BATTERY: return "Батерии";
        default: return "";
    }
}

// -----------------------------------------------------------------------------
// Verdict math
// -----------------------------------------------------------------------------
float quotaFactor(float playerShare) { return QUOTA_BASE + clamp01(playerShare); }

float computeVerdictShift(float served1, float served2, float supplyShare1) {
    float shift = VERDICT_SERVED_WEIGHT * (clamp01(served1) - clamp01(served2)) +
                  VERDICT_SUPPLY_WEIGHT * (2.0f * clamp01(supplyShare1) - 1.0f);
    return std::max(-VERDICT_MAX_SHIFT, std::min(VERDICT_MAX_SHIFT, shift));
}

float applyLeaderDamping(float p1Share, float p1Shift) {
    if (p1Shift == 0.0f) return 0.0f;
    const float sign = (p1Shift > 0.0f) ? 1.0f : -1.0f;
    const float gainerShare = (p1Shift > 0.0f) ? p1Share : 1.0f - p1Share;
    const float gain = std::abs(p1Shift);
    if (gainerShare + gain <= DAMPING_START_SHARE) return p1Shift;
    const float below = std::max(0.0f, DAMPING_START_SHARE - gainerShare); // part of the gain below 70 %
    const float above = gain - below;
    return sign * (below + above * DAMPING_FACTOR);
}

// -----------------------------------------------------------------------------
// Districts and quotas
// -----------------------------------------------------------------------------
float cityShareFromDistricts(const CityEconomy& ce) {
    float share = 0.0f;
    for (int d = 0; d < DISTRICT_COUNT; ++d) share += DISTRICTS[d].weight * ce.districts[d].p1Share;
    return clamp01(share);
}

void syncDistrictsToCityShare(CityEconomy& ce, float p1CityShare) {
    // A few passes absorb the clamping of districts that are already at 0 % or 100 %
    for (int pass = 0; pass < 4; ++pass) {
        float diff = p1CityShare - cityShareFromDistricts(ce);
        if (std::abs(diff) < 1e-5f) return;
        float movableWeight = 0.0f;
        for (int d = 0; d < DISTRICT_COUNT; ++d) {
            float s = ce.districts[d].p1Share;
            if ((diff > 0.0f && s < 1.0f) || (diff < 0.0f && s > 0.0f)) movableWeight += DISTRICTS[d].weight;
        }
        if (movableWeight <= 0.0f) return;
        for (int d = 0; d < DISTRICT_COUNT; ++d) {
            float& s = ce.districts[d].p1Share;
            if ((diff > 0.0f && s < 1.0f) || (diff < 0.0f && s > 0.0f)) s = clamp01(s + diff / movableWeight);
        }
    }
}

float playerQuotaMW(const CityEconomy& ce, int player, int demandMW, float hour24) {
    if (demandMW <= 0) return 0.0f;
    const int h = hourIndex(hour24);
    float quota = 0.0f;
    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        float districtDemand = demandMW * DISTRICTS[d].weight * getDistrictProfile(d, h);
        quota += districtDemand * quotaFactor(playerShareOf(ce.districts[d], player));
    }
    return quota;
}

float gridOutputFactor(const CityEconomy& ce, int player) {
    const PlayerGrid& g = ce.grid[(player == 1) ? 0 : 1];
    if (g.state == GRID_BLACKOUT) return 0.0f;
    if (g.state == GRID_BROWNOUT) return BROWNOUT_FACTOR;
    return 1.0f;
}

// -----------------------------------------------------------------------------
// Per-step integration
// -----------------------------------------------------------------------------
void onSimStep(CityEconomy& ce, const GridSample samples[2], int demandMW, float hour24, float gameSeconds, float dt) {
    for (int i = 0; i < 2; ++i) ce.lastDeliveredMW[i] = samples[i].deliveredMW;
    if (dt <= 0.0f) return;

    const float hours = dt / GAME_SECONDS_PER_HOUR;
    const int h = hourIndex(hour24);
    const int demand = std::max(0, demandMW);

    // City demand of the hour, per district
    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        ce.districts[d].demandNowMW = demand * DISTRICTS[d].weight * getDistrictProfile(d, h);
    }
    ce.demandEnergyToday += demand * getCityDemandProfile(h) * dt;

    for (int i = 0; i < 2; ++i) {
        const GridSample& s = samples[i];
        const int player = i + 1;
        PlayerLedger& led = ce.ledger[i];

        // (F-11) Ledger: generation by source, battery discharge, delivered energy
        led.generatedMWh[SRC_SOLAR] += s.solarMW * hours;
        led.generatedMWh[SRC_WIND] += s.windMW * hours;
        led.generatedMWh[SRC_HYDRO] += s.hydroMW * hours;
        led.generatedMWh[SRC_BATTERY] += s.batteryOutMW * hours;
        led.deliveredMWh += s.deliveredMW * hours;
        led.deliveredTodayMWh += s.deliveredMW * hours;
        ce.deliveredEnergyToday[i] += s.deliveredMW * dt;

        // (F-36) Allocate the delivered power to the districts in the fixed order
        float remaining = std::max(0.0f, s.deliveredMW);
        for (int d = 0; d < DISTRICT_COUNT; ++d) {
            DistrictState& ds = ce.districts[d];
            float quota = ds.demandNowMW * quotaFactor(playerShareOf(ds, player));
            float alloc = std::min(remaining, quota);
            remaining -= alloc;
            ds.quotaNowMW[i] = quota;
            ds.allocNowMW[i] = alloc;
            ds.quotaEnergy[i] += quota * dt;
            ds.servedEnergy[i] += alloc * dt;
            led.servedMWh += alloc * hours;
            led.servedTodayMWh += alloc * hours;
        }

        // (F-37) Grid frequency
        PlayerGrid& g = ce.grid[i];
        const float generation = s.solarMW + s.windMW + s.hydroMW;
        const float load = s.loadTargetMW;

        if (g.state != GRID_NORMAL) {
            g.stateHoursLeft -= hours;
            if (g.stateHoursLeft <= 0.0f) {
                g.state = GRID_NORMAL;
                g.stateHoursLeft = 0.0f;
            }
        }

        const long hourNow = static_cast<long>(std::floor(gameSeconds / GAME_SECONDS_PER_HOUR));
        if (hourNow != g.lastCheckHour) {
            if (g.lastCheckGenMW >= 0.0f && demand > 0 && g.state == GRID_NORMAL && load > 0.0f) {
                const float drop = std::max(0.0f, g.lastCheckGenMW - generation);
                const float shortfall = std::max(0.0f, load - generation);
                const float reserve = s.batteryOutMW + s.batteryReadyMW;
                const float uncovered = std::max(0.0f, std::min(drop, shortfall) - reserve);
                const float inertia = (generation > 0.0f) ? s.hydroMW / generation : 0.0f;
                const float deltaHz = FREQ_DROP_SCALE_HZ * (uncovered / std::max(load, 1.0f)) /
                                      (1.0f + HYDRO_INERTIA_BONUS * inertia);
                const float eventHz = NOMINAL_HZ - deltaHz;
                g.lastDropMW = drop;
                g.lastUncoveredMW = uncovered;
                if (eventHz < g.frequencyHz) g.frequencyHz = eventHz;
                if (eventHz < BLACKOUT_HZ) {
                    g.state = GRID_BLACKOUT;
                    g.stateHoursLeft = GRID_EVENT_HOURS;
                    ++g.blackoutsToday;
                    ++g.blackoutsTotal;
                } else if (eventHz < BROWNOUT_HZ) {
                    g.state = GRID_BROWNOUT;
                    g.stateHoursLeft = GRID_EVENT_HOURS;
                    ++g.brownoutsToday;
                    ++g.brownoutsTotal;
                }
            }
            g.lastCheckGenMW = generation;
            g.lastCheckHour = hourNow;
        }

        // Between checks the frequency drifts back towards its steady value
        float steady = NOMINAL_HZ;
        if (demand > 0 && load > 0.0f) {
            const float shortRatio = clamp01((load - s.deliveredMW) / load);
            const float surplusRatio = clamp01((s.deliveredMW - load) / load);
            steady = NOMINAL_HZ - FREQ_SAG_HZ * shortRatio + FREQ_RISE_HZ * surplusRatio;
        }
        if (g.state == GRID_BROWNOUT) steady = std::min(steady, 49.45f);
        if (g.state == GRID_BLACKOUT) steady = std::min(steady, 48.6f);
        const float maxMove = FREQ_RECOVERY_HZ_PER_HOUR * hours;
        const float diff = steady - g.frequencyHz;
        g.frequencyHz += std::max(-maxMove, std::min(maxMove, diff));
        g.lowestHzToday = std::min(g.lowestHzToday, g.frequencyHz);
    }
}

// -----------------------------------------------------------------------------
// Settlement
// -----------------------------------------------------------------------------
Settlement evaluateDay(const CityEconomy& ce, float p1CityShare) {
    Settlement st;
    float quotaSum[2] = { 0.0f, 0.0f };
    float servedSum[2] = { 0.0f, 0.0f };
    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        const DistrictState& ds = ce.districts[d];
        for (int i = 0; i < 2; ++i) {
            st.districtServed[d][i] = (ds.quotaEnergy[i] > 0.0f) ? clamp01(ds.servedEnergy[i] / ds.quotaEnergy[i]) : 0.0f;
            quotaSum[i] += ds.quotaEnergy[i];
            servedSum[i] += ds.servedEnergy[i];
        }
    }
    for (int i = 0; i < 2; ++i) st.served[i] = (quotaSum[i] > 0.0f) ? clamp01(servedSum[i] / quotaSum[i]) : 0.0f;

    const float delivered = ce.deliveredEnergyToday[0] + ce.deliveredEnergyToday[1];
    st.supplyShare1 = (delivered > 0.0f) ? ce.deliveredEnergyToday[0] / delivered : 0.5f;
    // The supply term only counts in full once the two players together delivered at least the
    // city's demand; a few MWh against nothing must not be worth the whole +-5 %.
    const float supplyWeight = (ce.demandEnergyToday > 0.0f) ? clamp01(delivered / ce.demandEnergyToday) : 0.0f;
    const float effectiveSupply1 = 0.5f + (st.supplyShare1 - 0.5f) * supplyWeight;

    // Raw shift per district, then the city-level damping of the leader's net gain
    float raw[DISTRICT_COUNT];
    float rawCity = 0.0f;
    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        raw[d] = (quotaSum[0] > 0.0f || quotaSum[1] > 0.0f)
                     ? computeVerdictShift(st.districtServed[d][0], st.districtServed[d][1], effectiveSupply1)
                     : 0.0f;
        rawCity += DISTRICTS[d].weight * raw[d];
    }
    st.rawShift = rawCity;
    const float dampedCity = applyLeaderDamping(p1CityShare, rawCity);
    float scaleGainer = 1.0f;
    if (std::abs(dampedCity - rawCity) > 1e-6f) {
        st.damped = true;
        // Shrink only the districts that move in the gainer's direction
        const float sign = (rawCity > 0.0f) ? 1.0f : -1.0f;
        float gainerPart = 0.0f;
        for (int d = 0; d < DISTRICT_COUNT; ++d) gainerPart += DISTRICTS[d].weight * std::max(0.0f, sign * raw[d]);
        const float reduce = std::abs(rawCity - dampedCity);
        scaleGainer = (gainerPart > 0.0f) ? std::max(0.0f, (gainerPart - reduce) / gainerPart) : 1.0f;
    }

    float applied = 0.0f;
    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        float shift = raw[d];
        if (st.damped && ((rawCity > 0.0f && shift > 0.0f) || (rawCity < 0.0f && shift < 0.0f))) shift *= scaleGainer;
        const float before = ce.districts[d].p1Share;
        const float after = clamp01(before + shift);
        st.districtShift[d] = after - before;
        applied += DISTRICTS[d].weight * st.districtShift[d];
    }
    st.appliedShift = applied;
    return st;
}

void resetDay(CityEconomy& ce) {
    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        DistrictState& ds = ce.districts[d];
        for (int i = 0; i < 2; ++i) {
            ds.quotaEnergy[i] = 0.0f;
            ds.servedEnergy[i] = 0.0f;
        }
    }
    for (int i = 0; i < 2; ++i) {
        ce.deliveredEnergyToday[i] = 0.0f;
        ce.ledger[i].deliveredTodayMWh = 0.0;
        ce.ledger[i].servedTodayMWh = 0.0;
        ce.grid[i].brownoutsToday = 0;
        ce.grid[i].blackoutsToday = 0;
        ce.grid[i].lowestHzToday = ce.grid[i].frequencyHz;
    }
    ce.demandEnergyToday = 0.0f;
}

float settleDay(CityEconomy& ce, int endedDay, int demandMW, float& p1CityShare, std::string& outMessage) {
    syncDistrictsToCityShare(ce, p1CityShare);
    const Settlement st = evaluateDay(ce, p1CityShare);

    DayReport& r = ce.lastReport;
    r = DayReport();
    r.valid = true;
    r.day = endedDay;
    r.demandMW = demandMW;
    r.shareBefore = p1CityShare;
    r.rawShift = st.rawShift;
    r.damped = st.damped;
    r.supplyShare1 = st.supplyShare1;

    for (int d = 0; d < DISTRICT_COUNT; ++d) {
        ce.districts[d].p1Share = clamp01(ce.districts[d].p1Share + st.districtShift[d]);
        ce.districts[d].lastShift = st.districtShift[d];
        r.districtShift[d] = st.districtShift[d];
        r.districtServed[d][0] = st.districtServed[d][0];
        r.districtServed[d][1] = st.districtServed[d][1];
    }
    p1CityShare = cityShareFromDistricts(ce);
    r.shareAfter = p1CityShare;
    r.appliedShift = r.shareAfter - r.shareBefore;
    for (int i = 0; i < 2; ++i) {
        r.served[i] = st.served[i];
        r.deliveredMWh[i] = static_cast<float>(ce.ledger[i].deliveredTodayMWh);
        r.co2AvoidedT[i] = static_cast<float>(ce.ledger[i].deliveredTodayMWh * GRID_CO2_T_PER_MWH);
        r.brownouts[i] = ce.grid[i].brownoutsToday;
        r.blackouts[i] = ce.grid[i].blackoutsToday;
    }

    // Day message (Bulgarian, upper case like the rest of the day-end messages)
    const std::string head = "ДЕН " + std::to_string(endedDay) + ": ОБСЛУЖЕНИ P1 " + pct(r.served[0]) + " / P2 " +
                             pct(r.served[1]) + " ОТ НУЖДАТА";
    const int movedPct = static_cast<int>(std::lround(std::abs(r.appliedShift) * 100.0f));
    if (movedPct == 0) {
        outMessage = head + " - РАВЕН ДЕН, ТЕРИТОРИЯТА НЕ СЕ ПРОМЕНИ.";
    } else {
        const int gainer = (r.appliedShift > 0.0f) ? 1 : 2;
        outMessage = head + " - ИГРАЧ " + std::to_string(gainer) + " ВЗЕ +" + std::to_string(movedPct) + "% ТЕРИТОРИЯ";
        outMessage += r.damped ? " (ЗАБАВЕНО НАД 70%)!" : "!";
    }

    resetDay(ce);
    return r.appliedShift;
}

int tieBreakWinner(const CityEconomy& ce) {
    const double a = ce.ledger[0].servedMWh;
    const double b = ce.ledger[1].servedMWh;
    if (std::abs(a - b) < 0.5) return 0;
    return (a > b) ? 1 : 2;
}

// -----------------------------------------------------------------------------
// Optional dominance levy / underdog subsidy
// -----------------------------------------------------------------------------
bool hasDominanceLevy(const CityEconomy& ce, int player, float p1CityShare) {
    if (!ce.rules.levyAndSubsidy) return false;
    const float share = (player == 1) ? p1CityShare : 1.0f - p1CityShare;
    return share > LEVY_ABOVE_SHARE;
}

bool hasUnderdogSubsidy(const CityEconomy& ce, int player, float p1CityShare) {
    if (!ce.rules.levyAndSubsidy) return false;
    const float share = (player == 1) ? p1CityShare : 1.0f - p1CityShare;
    return share < SUBSIDY_BELOW_SHARE;
}

float payoutFactor(const CityEconomy& ce, int player, float p1CityShare) {
    return hasDominanceLevy(ce, player, p1CityShare) ? LEVY_PAYOUT_FACTOR : 1.0f;
}

// -----------------------------------------------------------------------------
// Formatting
// -----------------------------------------------------------------------------
std::string formatPercentSigned(float fraction) {
    int v = static_cast<int>(std::lround(fraction * 100.0f));
    if (v > 0) return "+" + std::to_string(v) + "%";
    return std::to_string(v) + "%";
}

double co2AvoidedT(const PlayerLedger& ledger) { return ledger.deliveredMWh * GRID_CO2_T_PER_MWH; }

} // namespace Econ

// =============================================================================
// GameEngine accessors and hooks of the city economy           [team b-economy]
// =============================================================================
float GameEngine::getCurrentDemandMW() const {
    return static_cast<float>(std::max(0, city.cityEnergyDemand)) * Econ::getCityDemandProfileAt(hour24);
}

float GameEngine::getPlayerLoadTargetMW(int player) const {
    return Econ::playerQuotaMW(cityEcon, player, city.cityEnergyDemand, hour24);
}

float GameEngine::getGridFrequencyHz(int player) const { return cityEcon.grid[(player == 1) ? 0 : 1].frequencyHz; }

int GameEngine::getGridState(int player) const { return cityEcon.grid[(player == 1) ? 0 : 1].state; }

float GameEngine::getSecondsToSettlement() const {
    return std::max(0.0f, static_cast<float>(currentDay) * Balance::SECONDS_PER_DAY - gameSeconds);
}

Econ::Forecast GameEngine::projectPlayerOutput() const {
    Econ::Forecast f;
    f.secondsToSettlement = getSecondsToSettlement();
    f.active = (city.cityEnergyDemand > 0) && !isGracePeriod();
    const Econ::Settlement st = Econ::evaluateDay(cityEcon, city.p1CityShare);
    for (int i = 0; i < 2; ++i) {
        f.served[i] = st.served[i];
        f.nowMW[i] = cityEcon.lastDeliveredMW[i];
        f.quotaNowMW[i] = getPlayerLoadTargetMW(i + 1);
    }
    f.projectedShift = f.active ? st.appliedShift : 0.0f;
    return f;
}

bool GameEngine::consumeDayCut() {
    const bool occurred = city.dayCutOccurred;
    city.dayCutOccurred = false;
    return occurred;
}

void GameEngine::setEconomyRules(const Econ::Rules& rules) { cityEcon.rules = rules; }

void GameEngine::applyUnderdogRebate(int player, const BuildingCost& cost) {
    // BAL-04 optional subsidy: an underdog's building effectively costs x0.85 (15 % of the
    // recipe comes back right after the purchase). Integrator: map onto the Wave A cost accessor.
    if (!Econ::hasUnderdogSubsidy(cityEcon, player, city.p1CityShare)) return;
    auto& econ = (player == 1) ? p1 : p2;
    const float back = 1.0f - Econ::SUBSIDY_COST_FACTOR;
    econ.wood += static_cast<int>(std::lround(cost.woodCost * back));
    econ.iron += static_cast<int>(std::lround(cost.ironCost * back));
    econ.copper += static_cast<int>(std::lround(cost.copperCost * back));
    econ.coal += static_cast<int>(std::lround(cost.coalCost * back));
    econ.silicon += static_cast<int>(std::lround(cost.siliconCost * back));
    econ.silver += static_cast<int>(std::lround(cost.silverCost * back));
    econ.data.wood = econ.wood;
    econ.data.iron = econ.iron;
    econ.data.copper = econ.copper;
    econ.data.coal = econ.coal;
    econ.data.silicon = econ.silicon;
    econ.data.silver = econ.silver;
}

bool GameEngine::readUnderdogAidEnv() {
    const char* env = std::getenv("EC_UNDERDOG_AID");
    return env != nullptr && env[0] == '1';
}

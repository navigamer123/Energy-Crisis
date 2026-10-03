// =============================================================================
// ENERGY CRISIS - MATCH OPTIONS: rules presets, mutators, charters, perks   [team b-options]
// Pure data + formulas (no GameEngine state); GameEngine glue lives in game_match_engine.cpp.
// =============================================================================
#include "../includes/game_match.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {

// BuildingType indices used by PlayerPerks::buildCostMult (see BuildingType in game_main.h)
constexpr int BT_SOLAR = 1;
constexpr int BT_WIND = 2;
constexpr int BT_HYDRO = 3;
constexpr int BT_BATTERY = 4;
constexpr int BT_LAMP = 5;

template <typename T>
T clampValue(T v, T lo, T hi) { return (v < lo) ? lo : ((hi < v) ? hi : v); }

} // namespace

// -----------------------------------------------------------------------------
// MatchRules
// -----------------------------------------------------------------------------
MatchRules MatchRules::fromPreset(MatchPreset p) {
    MatchRules r;
    r.applyPreset(p);
    return r;
}

void MatchRules::applyPreset(MatchPreset p) {
    preset = p;
    switch (p) {
        case MatchPreset::BLITZ:
            daySeconds = 45.0f; graceDays = 1; finalDay = 10;
            startDemandMW = 30; demandGrowthMW = 20; victoryShare = 0.75f; miningMult = 1.5f;
            break;
        case MatchPreset::STANDARD:
            daySeconds = 90.0f; graceDays = 2; finalDay = 20;
            startDemandMW = 30; demandGrowthMW = 15; victoryShare = 0.85f; miningMult = 1.0f;
            break;
        case MatchPreset::MARATHON:
            daySeconds = 120.0f; graceDays = 3; finalDay = 35;
            startDemandMW = 30; demandGrowthMW = 10; victoryShare = 0.90f; miningMult = 1.0f;
            break;
        case MatchPreset::ENDLESS:
            daySeconds = 90.0f; graceDays = 2; finalDay = 0;
            startDemandMW = 30; demandGrowthMW = 15; victoryShare = 0.85f; miningMult = 1.0f;
            break;
        case MatchPreset::CUSTOM:
        case MatchPreset::COUNT:
        default:
            preset = MatchPreset::CUSTOM; // keep the current values; they are edited by hand
            break;
    }
}

void MatchRules::validate() {
    using namespace MatchLimits;
    if (!(daySeconds == daySeconds)) daySeconds = 90.0f; // NaN guard
    daySeconds = clampValue(daySeconds, DAY_SECONDS_MIN, DAY_SECONDS_MAX);
    graceDays = clampValue(graceDays, GRACE_MIN, GRACE_MAX);
    if (finalDay != 0) finalDay = clampValue(finalDay, FINAL_DAY_MIN, FINAL_DAY_MAX);
    // The match must last past the grace period, otherwise it would end before anyone plays
    if (finalDay != 0 && finalDay <= graceDays) finalDay = graceDays + 1;
    startDemandMW = clampValue(startDemandMW, DEMAND_START_MIN, DEMAND_START_MAX);
    demandGrowthMW = clampValue(demandGrowthMW, DEMAND_GROWTH_MIN, DEMAND_GROWTH_MAX);
    if (!(victoryShare == victoryShare)) victoryShare = 0.85f;
    victoryShare = clampValue(victoryShare, VICTORY_MIN, VICTORY_MAX);
    if (!(miningMult == miningMult)) miningMult = 1.0f;
    miningMult = clampValue(miningMult, MINING_MIN, MINING_MAX);
    mutators &= (1u << MUTATOR_COUNT) - 1u;
    while (activeMutatorCount() > MAX_ACTIVE_MUTATORS) {
        // Drop the highest flags until the limit holds
        for (int i = MUTATOR_COUNT - 1; i >= 0; --i) {
            if (mutators & (1u << i)) { mutators &= ~(1u << i); break; }
        }
    }
    for (auto& c : charter) {
        int ci = static_cast<int>(c);
        if (ci < 0 || ci >= static_cast<int>(CharterType::COUNT)) c = CharterType::NONE;
    }
    if (static_cast<int>(preset) < 0 || static_cast<int>(preset) >= static_cast<int>(MatchPreset::COUNT)) {
        preset = MatchPreset::CUSTOM;
    }
}

int MatchRules::activeMutatorCount() const {
    int n = 0;
    for (int i = 0; i < MUTATOR_COUNT; ++i) {
        if (mutators & (1u << i)) ++n;
    }
    return n;
}

bool MatchRules::toggleMutator(std::uint32_t flag) {
    if (hasMutator(flag)) {
        mutators &= ~flag;
        return true;
    }
    if (activeMutatorCount() >= MAX_ACTIVE_MUTATORS) return false;
    mutators |= flag;
    return true;
}

int MatchRules::effectiveGraceDays() const {
    return hasMutator(MUT_NO_GRACE) ? 0 : graceDays;
}

int MatchRules::effectiveDemandGrowthMW() const {
    return hasMutator(MUT_HUNGRY_CITY) ? demandGrowthMW * 2 : demandGrowthMW;
}

float MatchRules::effectiveShiftMult() const {
    return hasMutator(MUT_VOLATILE_CITY) ? 2.0f : 1.0f;
}

// -----------------------------------------------------------------------------
// Player-facing names (Bulgarian)
// -----------------------------------------------------------------------------
namespace MatchInfo {

const char* presetName(MatchPreset p) {
    switch (p) {
        case MatchPreset::BLITZ:    return "БЛИЦ";
        case MatchPreset::STANDARD: return "СТАНДАРТ";
        case MatchPreset::MARATHON: return "МАРАТОН";
        case MatchPreset::ENDLESS:  return "БЕЗКРАЙНА";
        case MatchPreset::CUSTOM:   return "СВОЯ";
        default:                    return "?";
    }
}

const char* presetDescription(MatchPreset p) {
    switch (p) {
        case MatchPreset::BLITZ:    return "Около 10 минути: 45 с ден, 10 дни, 1 гратисен ден, победа при 75%, +50% добив.";
        case MatchPreset::STANDARD: return "Правилата от README: 90 с ден, 20 дни, 2 гратисни дни, победа при 85%.";
        case MatchPreset::MARATHON: return "Дълга игра: 120 с ден, 35 дни, 3 гратисни дни, победа при 90%.";
        case MatchPreset::ENDLESS:  return "Без краен ден: мачът свършва само когато някой превземе града.";
        case MatchPreset::CUSTOM:   return "Ваши стойности: променете всеки ред с [A]/[D] или със стрелките.";
        default:                    return "";
    }
}

std::uint32_t mutatorFlag(int index) {
    if (index < 0 || index >= MUTATOR_COUNT) return MUT_NONE;
    return 1u << index;
}

const char* mutatorName(std::uint32_t flag) {
    switch (flag) {
        case MUT_ETERNAL_WINTER: return "ВЕЧНА ЗИМА";
        case MUT_MIRROR_WEATHER: return "ОГЛЕДАЛНО ВРЕМЕ";
        case MUT_RICH_VEINS:     return "БОГАТИ ЖИЛИ";
        case MUT_NO_GRACE:       return "БЕЗ ГРАТИС";
        case MUT_BUILDING_BOOM:  return "СТРОИТЕЛЕН БУМ";
        case MUT_VOLATILE_CITY:  return "НЕПОСТОЯНЕН ГРАД";
        case MUT_HUNGRY_CITY:    return "НЕНАСИТЕН ГРАД";
        case MUT_HEAD_START:     return "СТАРТОВ КАПИТАЛ";
        default:                 return "?";
    }
}

const char* mutatorDescription(std::uint32_t flag) {
    switch (flag) {
        case MUT_ETERNAL_WINTER: return "Сезонът е винаги зима: къси дни и сняг.";
        case MUT_MIRROR_WEATHER: return "Двата сектора имат еднакво време (без късмет).";
        case MUT_RICH_VEINS:     return "Всеки добив дава двойно повече ресурси.";
        case MUT_NO_GRACE:       return "Градът иска ток още от ден 1.";
        case MUT_BUILDING_BOOM:  return "Всички сгради струват 40% по-малко.";
        case MUT_VOLATILE_CITY:  return "Дневната промяна на територията е двойна.";
        case MUT_HUNGRY_CITY:    return "Нуждата на града расте два пъти по-бързо.";
        case MUT_HEAD_START:     return "Старт с 40 от всеки ресурс и 300 злато.";
        default:                 return "";
    }
}

const char* charterName(CharterType c) {
    switch (c) {
        case CharterType::NONE:             return "БЕЗ ХАРТА";
        case CharterType::SOLAR_COOP:       return "СОЛАРЕН КООПЕРАТИВ";
        case CharterType::HYDRO_HOLDING:    return "ХИДРО ХОЛДИНГ";
        case CharterType::MINING_SYNDICATE: return "МИНЕН СИНДИКАТ";
        case CharterType::CITY_INSIDER:     return "ГРАДСКИ ИНСАЙДЕР";
        case CharterType::NIGHT_SHIFT:      return "НОЩНА СМЯНА";
        default:                            return "?";
    }
}

const char* charterShortName(CharterType c) {
    switch (c) {
        case CharterType::NONE:             return "Без харта";
        case CharterType::SOLAR_COOP:       return "Соларен";
        case CharterType::HYDRO_HOLDING:    return "Хидро";
        case CharterType::MINING_SYNDICATE: return "Минен";
        case CharterType::CITY_INSIDER:     return "Инсайдер";
        case CharterType::NIGHT_SHIFT:      return "Нощна смяна";
        default:                            return "?";
    }
}

// F-35 charter table: the only place where charter numbers live (see CharterDef)
const CharterDef& charterDef(CharterType c) {
    static const CharterDef* const kDefs = [] {
        static CharterDef d[static_cast<int>(CharterType::COUNT)];
        CharterDef& solar = d[static_cast<int>(CharterType::SOLAR_COOP)];
        solar.solarOutput = 1.40f;
        solar.solarCost = 0.60f;
        solar.hydroOutput = 0.90f;
        CharterDef& hydro = d[static_cast<int>(CharterType::HYDRO_HOLDING)];
        hydro.hydroOutput = 1.30f;
        hydro.hydroCost = 0.70f;
        hydro.solarOutput = 0.90f;
        CharterDef& mining = d[static_cast<int>(CharterType::MINING_SYNDICATE)];
        mining.miningYield = 1.20f;
        mining.mineUpgradeCost = 0.80f;
        mining.windOutput = 0.90f;
        mining.solarOutput = 0.90f;
        mining.hydroOutput = 0.90f;
        CharterDef& insider = d[static_cast<int>(CharterType::CITY_INSIDER)];
        insider.income = 1.30f;
        insider.landCost = 0.85f;
        insider.miningYield = 0.90f;
        CharterDef& night = d[static_cast<int>(CharterType::NIGHT_SHIFT)];
        night.batteryCapacity = 1.50f;
        night.batteryCost = 0.85f;
        night.lampDraw = 0.50f;
        night.solarOutput = 0.90f;
        return d;
    }();
    int i = static_cast<int>(c);
    if (i < 0 || i >= static_cast<int>(CharterType::COUNT)) i = 0;
    return kDefs[i];
}

namespace {

// One effect of a charter for the generated bonus / drawback lines. `higherIsBetter` is true
// for outputs, yields and income; false for prices and lamp draw.
struct CharterEffect {
    float CharterDef::*field;
    bool higherIsBetter;
    const char* before; // text before the percentage ("" when the percentage comes first)
    const char* after;  // text after the percentage
};

const CharterEffect kCharterEffects[] = {
    { &CharterDef::solarOutput, true, "", " слънчева мощност" },
    { &CharterDef::windOutput, true, "", " вятърна мощност" },
    { &CharterDef::hydroOutput, true, "", " мощност на ВЕЦ" },
    { &CharterDef::batteryCapacity, true, "", " капацитет на батерии" },
    { &CharterDef::miningYield, true, "", " добив" },
    { &CharterDef::income, true, "", " пари от града" },
    { &CharterDef::solarCost, false, "панели ", " цена" },
    { &CharterDef::windCost, false, "мелници ", " цена" },
    { &CharterDef::hydroCost, false, "ВЕЦ ", " цена" },
    { &CharterDef::batteryCost, false, "батерии ", " цена" },
    { &CharterDef::lampDraw, false, "лампи ", " ток" },
    { &CharterDef::mineUpgradeCost, false, "ъпгрейди на мини ", " злато" },
    { &CharterDef::landCost, false, "земя ", " злато" },
};

// "+35%" / "-10%" for a multiplier
std::string percentText(float mult) {
    int pct = static_cast<int>(std::lround((mult - 1.0f) * 100.0f));
    return (pct > 0 ? "+" : "") + std::to_string(pct) + "%";
}

bool isOutputField(float CharterDef::*field) {
    return field == &CharterDef::solarOutput || field == &CharterDef::windOutput || field == &CharterDef::hydroOutput;
}

// wantGood = true: the bonus line; false: the drawback line (without the "Минус: " prefix)
std::string charterEffectLine(const CharterDef& d, bool wantGood) {
    std::string line;
    auto add = [&line](const std::string& part) {
        if (!line.empty()) line += ", ";
        line += part;
    };
    // The same change on all three generators reads as one effect
    const bool sameOutputs = std::fabs(d.solarOutput - d.windOutput) < 0.005f && std::fabs(d.solarOutput - d.hydroOutput) < 0.005f;
    for (const auto& e : kCharterEffects) {
        float v = d.*(e.field);
        if (std::fabs(v - 1.0f) < 0.005f) continue;
        bool good = e.higherIsBetter ? (v > 1.0f) : (v < 1.0f);
        if (good != wantGood) continue;
        if (sameOutputs && isOutputField(e.field)) {
            if (e.field == &CharterDef::solarOutput) add(percentText(v) + " мощност на всички централи");
            continue;
        }
        add(e.before + percentText(v) + e.after);
    }
    return line;
}

} // namespace

const char* charterDescription(CharterType c) {
    static const std::string* const kLines = [] {
        static std::string s[static_cast<int>(CharterType::COUNT)];
        for (int i = 0; i < static_cast<int>(CharterType::COUNT); ++i) {
            s[i] = charterEffectLine(charterDef(static_cast<CharterType>(i)), true);
            if (s[i].empty()) s[i] = "Еднакъв старт, без бонуси и минуси.";
        }
        return s;
    }();
    int i = static_cast<int>(c);
    if (i < 0 || i >= static_cast<int>(CharterType::COUNT)) return "";
    return kLines[i].c_str();
}

const char* charterDrawback(CharterType c) {
    static const std::string* const kLines = [] {
        static std::string s[static_cast<int>(CharterType::COUNT)];
        for (int i = 0; i < static_cast<int>(CharterType::COUNT); ++i) {
            std::string minus = charterEffectLine(charterDef(static_cast<CharterType>(i)), false);
            s[i] = minus.empty() ? std::string() : "Минус: " + minus;
        }
        return s;
    }();
    int i = static_cast<int>(c);
    if (i < 0 || i >= static_cast<int>(CharterType::COUNT)) return "";
    return kLines[i].c_str();
}

const char* techBranchName(int branch) {
    switch (branch) {
        case 0:  return "ГЕНЕРАЦИЯ";
        case 1:  return "СЪХРАНЕНИЕ";
        case 2:  return "ДОБИВ";
        default: return "?";
    }
}

const TechInfo& techInfo(int branch, int tier, int option) {
    static const TechInfo kTree[TECH_BRANCHES][TECH_TIERS][TECH_OPTIONS] = {
        { // 0 ГЕНЕРАЦИЯ
            { { "Фотоволтаици 2.0", "+15% слънчева мощност" },  { "Аеродинамични перки", "+15% вятърна мощност" } },
            { { "Турбини Каплан", "+20% мощност на ВЕЦ" },        { "Умни инвертори", "+8% мощност на всички" } },
            { { "Перовскитни клетки", "+30% слънчева мощност" }, { "Офшорни ротори", "+30% вятърна мощност" } },
        },
        { // 1 СЪХРАНЕНИЕ
            { { "Литиеви клетки", "+50% капацитет на батерии" }, { "LED осветление", "Лампите харчат 50% по-малко" } },
            { { "Бърз заряд", "+50% мощност на батерии" },        { "Прожектори", "+50% радиус на лампите" } },
            { { "Твърдотелни батерии", "+100% капацитет" },       { "Умна мрежа", "+25% пари от града" } },
        },
        { // 2 ДОБИВ
            { { "Хидравлични сонди", "+25% добив" },              { "Автоматизация", "-25% време за добив" } },
            { { "Модулно строителство", "Сградите -15% цена" },   { "Геоложки карти", "Ъпгрейди на мини -30%" } },
            { { "Дълбок добив", "+40% добив" },                   { "Земна борса", "Земята -30% злато" } },
        },
    };
    static const TechInfo kNone = { "?", "" };
    if (branch < 0 || branch >= TECH_BRANCHES || tier < 0 || tier >= TECH_TIERS || option < 0 || option >= TECH_OPTIONS) {
        return kNone;
    }
    return kTree[branch][tier][option];
}

int techTierCost(int tier) {
    switch (tier) {
        case 0:  return 4000;
        case 1:  return 15000;
        case 2:  return 40000;
        default: return 0;
    }
}

// -----------------------------------------------------------------------------
// Perks: charter x research x mutators (multiplicative)
// -----------------------------------------------------------------------------
PlayerPerks computePlayerPerks(const MatchRules& rules, int player, const TechState& tech) {
    PlayerPerks p;

    // F-35 charter (multipliers from the charter table)
    const CharterDef& ch = charterDef(rules.charterOf(player));
    p.solarOutputMult *= ch.solarOutput;
    p.windOutputMult *= ch.windOutput;
    p.hydroOutputMult *= ch.hydroOutput;
    p.buildCostMult[BT_SOLAR] *= ch.solarCost;
    p.buildCostMult[BT_WIND] *= ch.windCost;
    p.buildCostMult[BT_HYDRO] *= ch.hydroCost;
    p.buildCostMult[BT_BATTERY] *= ch.batteryCost;
    p.batteryCapacityMult *= ch.batteryCapacity;
    p.lampDrawMult *= ch.lampDraw;
    p.miningYieldMult *= ch.miningYield;
    p.mineUpgradeCostMult *= ch.mineUpgradeCost;
    p.incomeMult *= ch.income;
    p.landCostMult *= ch.landCost;

    // F-33 research (choice -1 = not researched)
    auto has = [&](int branch, int tier, int option) {
        return tech.choice[branch][tier] == option;
    };
    // Generation
    if (has(0, 0, 0)) p.solarOutputMult *= 1.15f;
    if (has(0, 0, 1)) p.windOutputMult *= 1.15f;
    if (has(0, 1, 0)) p.hydroOutputMult *= 1.20f;
    if (has(0, 1, 1)) { p.solarOutputMult *= 1.08f; p.windOutputMult *= 1.08f; p.hydroOutputMult *= 1.08f; }
    if (has(0, 2, 0)) p.solarOutputMult *= 1.30f;
    if (has(0, 2, 1)) p.windOutputMult *= 1.30f;
    // Storage & night
    if (has(1, 0, 0)) p.batteryCapacityMult *= 1.50f;
    if (has(1, 0, 1)) p.lampDrawMult *= 0.50f;
    if (has(1, 1, 0)) p.batteryPowerMult *= 1.50f;
    if (has(1, 1, 1)) p.lampRadiusMult *= 1.50f;
    if (has(1, 2, 0)) p.batteryCapacityMult *= 2.00f;
    if (has(1, 2, 1)) p.incomeMult *= 1.25f;
    // Extraction
    if (has(2, 0, 0)) p.miningYieldMult *= 1.25f;
    if (has(2, 0, 1)) p.miningCooldownMult *= 0.75f;
    if (has(2, 1, 0)) { for (int i = BT_SOLAR; i <= BT_LAMP; ++i) p.buildCostMult[i] *= 0.85f; }
    if (has(2, 1, 1)) p.mineUpgradeCostMult *= 0.70f;
    if (has(2, 2, 0)) p.miningYieldMult *= 1.40f;
    if (has(2, 2, 1)) p.landCostMult *= 0.70f;

    // F-24 mutators and the F-03 pacing multiplier
    if (rules.hasMutator(MUT_RICH_VEINS)) p.miningYieldMult *= 2.0f;
    if (rules.hasMutator(MUT_BUILDING_BOOM)) {
        for (int i = BT_SOLAR; i <= BT_LAMP; ++i) p.buildCostMult[i] *= 0.60f;
    }
    p.miningYieldMult *= rules.miningMult;

    return p;
}

} // namespace MatchInfo

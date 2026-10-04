// =============================================================================
// ENERGY CRISIS - F-33 RESEARCH LAB                                         [team b-options]
// Three branches (Generation / Storage & night / Extraction) x three tiers. Each tier offers
// two options and the player keeps exactly one ("pick 1 of 2"). Paid with city money, which
// was never spent before. Effects are applied through PlayerPerks (game_match.cpp).
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cstdlib>

namespace {

bool validTech(int branch, int tier, int option) {
    return branch >= 0 && branch < TECH_BRANCHES && tier >= 0 && tier < TECH_TIERS &&
           option >= 0 && option < TECH_OPTIONS;
}

// "40000" -> "40 000"
std::string groupThousands(int value) {
    std::string digits = std::to_string(std::abs(value));
    std::string out;
    int n = static_cast<int>(digits.size());
    for (int i = 0; i < n; ++i) {
        out += digits[static_cast<size_t>(i)];
        int left = n - 1 - i;
        if (left > 0 && left % 3 == 0) out += ' ';
    }
    return (value < 0 ? "-" : "") + out;
}

} // namespace

int GameEngine::getTechChoice(int player, int branch, int tier) const {
    if (branch < 0 || branch >= TECH_BRANCHES || tier < 0 || tier >= TECH_TIERS) return -1;
    const PlayerEconomy& econ = (player == 1) ? p1 : p2;
    return econ.tech.choice[branch][tier];
}

int GameEngine::getResearchedTierCount(int player, int branch) const {
    int n = 0;
    for (int t = 0; t < TECH_TIERS; ++t) {
        if (getTechChoice(player, branch, t) < 0) break;
        ++n;
    }
    return n;
}

TechStatus GameEngine::getTechStatus(int player, int branch, int tier, int option) const {
    if (!validTech(branch, tier, option)) return TechStatus::LOCKED;
    int choice = getTechChoice(player, branch, tier);
    if (choice == option) return TechStatus::RESEARCHED;
    if (choice >= 0) return TechStatus::EXCLUDED;
    if (tier > 0 && getTechChoice(player, branch, tier - 1) < 0) return TechStatus::LOCKED;
    const PlayerEconomy& econ = (player == 1) ? p1 : p2;
    return (econ.money >= MatchInfo::techTierCost(tier)) ? TechStatus::AVAILABLE : TechStatus::UNAFFORDABLE;
}

bool GameEngine::researchTech(int player, int branch, int tier, int option, std::string& outMsg) {
    if (player != 1 && player != 2) {
        outMsg = "НЕВАЛИДЕН ИГРАЧ!";
        return false;
    }
    if (!validTech(branch, tier, option)) {
        outMsg = "НЕВАЛИДНО ИЗСЛЕДВАНЕ!";
        return false;
    }
    const TechInfo& info = MatchInfo::techInfo(branch, tier, option);
    const int cost = MatchInfo::techTierCost(tier);
    PlayerEconomy& econ = (player == 1) ? p1 : p2;

    switch (getTechStatus(player, branch, tier, option)) {
        case TechStatus::RESEARCHED:
            outMsg = std::string("ВЕЧЕ Е ИЗСЛЕДВАНО: ") + info.name + "!";
            return false;
        case TechStatus::EXCLUDED:
            outMsg = "В ТОВА НИВО ВЕЧЕ ИЗБРАХТЕ: " +
                     std::string(MatchInfo::techInfo(branch, tier, econ.tech.choice[branch][tier]).name) + "!";
            return false;
        case TechStatus::LOCKED:
            outMsg = "ЗАКЛЮЧЕНО! ПЪРВО ИЗСЛЕДВАЙТЕ НИВО " + std::to_string(tier) + " В КЛОН " +
                     MatchInfo::techBranchName(branch) + ".";
            return false;
        case TechStatus::UNAFFORDABLE:
            outMsg = "НЕДОСТИГ НА ПАРИ! НУЖНИ: " + groupThousands(cost) + " $ (ИМАТЕ " + groupThousands(econ.money) + " $)";
            return false;
        case TechStatus::AVAILABLE:
        default:
            break;
    }

    econ.money -= cost;
    econ.tech.choice[branch][tier] = static_cast<signed char>(option);
    refreshPerkDependentState(player);

    outMsg = std::string("ИЗСЛЕДВАНО: ") + info.name + " (" + info.effect + ")";
    return true;
}

bool GameEngine::autoResearch(int player, std::string& outMsg) {
    // Bot helper: buy the cheapest unlocked tier it can afford (branches in the order
    // Generation, Extraction, Storage) and pick the option that suits its buildings.
    int solar = 0, wind = 0, hydro = 0, batteries = 0;
    for (const auto& b : buildings) {
        if (b.playerOwner != player) continue;
        if (b.type == BuildingType::SOLAR_PANEL) ++solar;
        else if (b.type == BuildingType::WIND_TURBINE) ++wind;
        else if (b.type == BuildingType::HYDRO_PLANT) ++hydro;
        else if (b.type == BuildingType::BATTERY) ++batteries;
    }
    const int order[TECH_BRANCHES] = { 0, 2, 1 };
    for (int tier = 0; tier < TECH_TIERS; ++tier) {
        for (int bi = 0; bi < TECH_BRANCHES; ++bi) {
            int branch = order[bi];
            if (getTechChoice(player, branch, tier) >= 0) continue;
            if (tier > 0 && getTechChoice(player, branch, tier - 1) < 0) continue;

            int option = 0;
            if (branch == 0) {
                if (tier == 0 || tier == 2) option = (wind > solar) ? 1 : 0;
                else option = (hydro > 0) ? 0 : 1;
            } else if (branch == 1) {
                if (tier == 0 || tier == 1) option = (batteries > 0) ? 0 : 1;
                else option = (batteries >= 3) ? 0 : 1;
            } else {
                option = 0;
            }
            if (getTechStatus(player, branch, tier, option) != TechStatus::AVAILABLE) {
                outMsg.clear();
                return false; // cheapest tier not affordable yet: save up for it
            }
            return researchTech(player, branch, tier, option, outMsg);
        }
    }
    outMsg.clear();
    return false;
}

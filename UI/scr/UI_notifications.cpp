#include "../includes/UI_notifications.h"
#include "../includes/UI_infoText.h"
#include "../includes/UI_matchStats.h"
#include <iterator>
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// =============================================================================
// team info: notification system (toasts, proactive alerts, event log)
// =============================================================================

namespace {

// Layout: each player's toast stack sits in the old popup slot, between the building
// panel (ends at y = 510) and the resource quarter-circle (starts near y = 651 at x = 20)
constexpr float TOAST_X_P1 = 20.0f;
constexpr float TOAST_W = 218.0f; // leaves a gap to the mine cards (x 245 / 1355)
constexpr float TOAST_X_P2 = 1600.0f - 20.0f - TOAST_W;
constexpr float TOAST_TOP = 516.0f;
constexpr float TOAST_BOTTOM = 649.0f;
constexpr float TOAST_PAD = 8.0f;
constexpr float COMPACT_H = 21.0f;
constexpr float TOAST_GAP = 3.0f;

// Colours (named here so a theme pass can map them)
const sf::Color COL_PANEL(16, 22, 34);
const sf::Color COL_TITLE(240, 246, 255);
const sf::Color COL_DETAIL(195, 225, 255);
const sf::Color COL_ACTION(255, 215, 80);
const sf::Color COL_SEPARATOR(60, 85, 120);
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 120, 200);
const sf::Color COL_GOOD(0, 230, 160);
const sf::Color COL_WARN(255, 180, 50);
const sf::Color COL_CRIT(255, 80, 80);
const sf::Color COL_CITY(255, 215, 0);
const sf::Color COL_LOG_BG(14, 22, 36);
const sf::Color COL_LOG_ROW(22, 32, 50);
const sf::Color COL_LOG_DIM(140, 165, 200);

float durationFor(ToastPriority p) {
    switch (p) {
        case ToastPriority::CRITICAL: return 7.0f;
        case ToastPriority::WARNING:  return 5.5f;
        case ToastPriority::INFO:     break;
    }
    return 4.0f;
}

sf::Color withAlpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(c.a));
    return c;
}

std::string playerName(int p) { return (p == 1) ? "Играч 1" : "Играч 2"; }

std::string fmtMult(float v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
    std::string s(buf);
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

const char* weatherBg(WeatherType w) {
    switch (w) {
        case WeatherType::SUNNY:  return "Слънчево";
        case WeatherType::WINDY:  return "Ветровито";
        case WeatherType::RAINY:  return "Дъждовно";
        case WeatherType::STORMY: return "Буря";
        case WeatherType::SNOWY:  return "Снеговалеж";
        case WeatherType::CLOUDY: return "Облачно";
    }
    return "Слънчево";
}

const char* seasonBg(SeasonType s) {
    switch (s) {
        case SeasonType::SPRING: return "пролетта";
        case SeasonType::SUMMER: return "лятото";
        case SeasonType::AUTUMN: return "есента";
        case SeasonType::WINTER: return "зимата";
    }
    return "пролетта";
}

const char* mineBg(ResourceType r) {
    switch (r) {
        case ResourceType::WOOD:    return "дърводобива";
        case ResourceType::IRON:    return "желязната мина";
        case ResourceType::COPPER:  return "медната мина";
        case ResourceType::COAL:    return "въглищната мина";
        case ResourceType::SILICON: return "силициевата кариера";
        case ResourceType::SILVER:  return "сребърната мина";
        case ResourceType::GOLD:    return "златната жила";
        default:                    return "мината";
    }
}

std::string sunTimes(SeasonType s) {
    return "Изгрев " + Balance::formatHourMinute(Balance::getSunriseHour(s)) + ", залез " +
           Balance::formatHourMinute(Balance::getSunsetHour(s)) + ".";
}

} // namespace

// -----------------------------------------------------------------------------
// Queue
// -----------------------------------------------------------------------------

void UI_notifications::reset() {
    toasts[0].clear();
    toasts[1].clear();
    entries.clear();
    logScroll = 0;
    for (auto& row : alertDay) std::fill(std::begin(row), std::end(row), 0);
    for (auto& row : opponentDay) std::fill(std::begin(row), std::end(row), 0);
    curDay = 1;
    curHour = Balance::MATCH_START_HOUR;
}

void UI_notifications::log(int player, ToastPriority priority, const std::string& text) {
    LogEntry e;
    e.day = curDay;
    e.hour = curHour;
    e.player = player;
    e.priority = priority;
    e.text = text;
    entries.push_back(e);
    while (entries.size() > MAX_LOG) entries.pop_front();
    if (logScroll > 0) ++logScroll; // keep the rows the reader is looking at in place
}

void UI_notifications::push(int player, ToastPriority priority, const std::string& channel, const std::string& badge,
                            const std::string& title, const std::string& detail, const std::string& action,
                            sf::Color accent, bool addToLog) {
    if (player != 1 && player != 2) return;
    std::deque<Toast>& q = toasts[player - 1];
    if (!channel.empty()) {
        q.erase(std::remove_if(q.begin(), q.end(), [&](const Toast& t) { return t.channel == channel; }), q.end());
    }
    Toast t;
    t.priority = priority;
    t.channel = channel;
    t.badge = badge;
    t.title = title;
    t.detail = detail;
    t.action = action;
    t.accent = accent;
    t.timer = t.maxTimer = durationFor(priority);
    t.serial = ++serialCounter;
    q.push_back(t);

    // Too many: drop the least important, oldest toast (a critical one only when all are critical)
    while (static_cast<int>(q.size()) > MAX_TOASTS) {
        auto victim = q.begin();
        for (auto it = q.begin(); it != q.end(); ++it) {
            if (it->priority < victim->priority ||
                (it->priority == victim->priority && it->serial < victim->serial)) {
                victim = it;
            }
        }
        q.erase(victim);
    }

    if (addToLog) log(player, priority, title + (detail.empty() ? "" : ": " + detail));
}

void UI_notifications::update(float dt, const GameEngine& engine, bool botActive) {
    curDay = engine.getCurrentDay();
    curHour = engine.getHour24();
    for (auto& q : toasts) {
        for (auto& t : q) {
            t.timer -= dt;
            t.age += dt;
        }
        q.erase(std::remove_if(q.begin(), q.end(), [](const Toast& t) { return t.timer <= 0.0f; }), q.end());
    }
    if (dt > 0.0f && engine.getCityState().winner == 0) evaluateAlerts(engine, botActive);
}

// -----------------------------------------------------------------------------
// Proactive alerts (each at most once per in-game day and player)
// -----------------------------------------------------------------------------

bool UI_notifications::alertOnce(int player, Alert a, int day) {
    int& last = alertDay[player - 1][a];
    if (last == day) return false;
    last = day;
    return true;
}

void UI_notifications::evaluateAlerts(const GameEngine& engine, bool botActive) {
    const float hour = engine.getHour24();
    const int day = engine.getCurrentDay();
    const auto& city = engine.getCityState();
    const float sunset = engine.getSunsetHour();

    for (int player = 1; player <= 2; ++player) {
        if (!isHuman(player, botActive)) continue;
        const sf::Color own = (player == 1) ? COL_P1 : COL_P2;

        int lamps = 0, others = 0, batteries = 0;
        float stored = 0.0f, capacity = 0.0f;
        for (const auto& b : engine.getBuildings()) {
            if (b.playerOwner != player) continue;
            if (b.type == BuildingType::LAMP) ++lamps;
            else ++others;
            if (b.type == BuildingType::BATTERY) {
                ++batteries;
                stored += b.energyStored;
                capacity += b.maxCapacity;
            }
        }

        // 1. Nightfall soon and no lamp: building at night will be impossible
        if (others > 0 && lamps == 0 && hour >= sunset - 1.5f && hour < sunset &&
            alertOnce(player, NIGHT_SOON, day)) {
            push(player, ToastPriority::WARNING, "alert-night", "НОЩ", "Нощта идва след около 1 час",
                 "Без захранена лампа строежът нощем е забранен. Поставете Осветителна лампа.",
                 player == 1 ? "[5] или [E]: Осветителна лампа" : "[PgDn]: Осветителна лампа", COL_WARN, true);
        }

        // 2. Settlement risk in the last 4 game hours before the 06:00 settlement
        if (city.cityEnergyDemand > 0 && hour >= 2.0f && hour < 6.0f) {
            const float total = Balance::SECONDS_PER_DAY;
            const float elapsed = std::min(total, city.dailySeconds);
            const float avgSoFar = engine.getTodayAverageMW(player);
            const float mwNow = static_cast<float>(engine.getPlayerEconomy(player).energyMW);
            const float projected = (avgSoFar * elapsed + mwNow * (total - elapsed)) / total;
            const float demand = static_cast<float>(city.cityEnergyDemand);
            if (projected < demand && alertOnce(player, SETTLEMENT_RISK, day)) {
                bool severe = projected < 0.8f * demand;
                push(player, severe ? ToastPriority::CRITICAL : ToastPriority::WARNING, "alert-risk", "РИСК",
                     "~" + std::to_string(static_cast<int>(projected)) + " от " +
                         std::to_string(city.cityEnergyDemand) + " MW",
                     "Отчетът е в 06:00, а средната доставка за деня е под нуждата. Вятър, ВЕЦ и батерии помагат нощем.", "",
                     severe ? COL_CRIT : COL_WARN, true);
            }
        }

        // 3. Battery bank below 20% while it is dark
        if (batteries > 0 && !engine.isDaylight() && capacity > 0.0f && stored < 0.2f * capacity &&
            alertOnce(player, BATTERY_LOW, day)) {
            push(player, ToastPriority::INFO, "alert-battery", "БАТЕРИИ", "Батериите са под 20%",
                 "Заредени " + std::to_string(static_cast<int>(stored)) + " от " +
                     std::to_string(static_cast<int>(capacity)) + " MWh. Изгрев в " +
                     Balance::formatHourMinute(engine.getSunriseHour()) + ".",
                 "", own, true);
        }

        // 4. The season changes at the coming midnight
        SeasonType tomorrow = Balance::getSeasonForDay(day + 1);
        if (tomorrow != engine.getSeason() && hour >= 17.0f && hour < 18.0f &&
            alertOnce(player, SEASON_TOMORROW, day)) {
            push(player, ToastPriority::INFO, "alert-season", "УТРЕ", std::string("Утре започва ") + seasonBg(tomorrow),
                 sunTimes(tomorrow), "", COL_CITY, false);
            if (player == 1) log(0, ToastPriority::INFO, std::string("Утре започва ") + seasonBg(tomorrow) + ". " + sunTimes(tomorrow));
        }
    }
}

// -----------------------------------------------------------------------------
// Info events -> toasts and log lines
// -----------------------------------------------------------------------------

void UI_notifications::onInfoEvent(const InfoEvent& ev, const GameEngine& engine, bool botActive) {
    curDay = engine.getCurrentDay();
    curHour = engine.getHour24();
    const int p = ev.player;
    const int other = (p == 1) ? 2 : 1;

    // Opponent alerts: once per in-game day and kind, only to a human opponent
    auto opponentAlert = [&](int kind, const std::string& title, const std::string& detail) {
        if (!(p == 1 || p == 2) || !isHuman(other, botActive)) return;
        int& last = opponentDay[other - 1][kind];
        if (last == curDay) return;
        last = curDay;
        push(other, ToastPriority::INFO, "opponent", "СЪПЕРНИК", title, detail, "", (other == 1) ? COL_P2 : COL_P1, false);
    };

    switch (ev.type) {
        case InfoEventType::BUILT: {
            std::string name = engine.getBuildingCost(ev.building).nameBg;
            log(p, ToastPriority::INFO, playerName(p) + " построи " + name +
                                            (ev.value > 1 ? " (x" + std::to_string(ev.value) + ")" : ""));
            if (ev.building == BuildingType::HYDRO_PLANT) {
                opponentAlert(1, "Съперникът построи ВЕЦ", "ВЕЦ дава ток денем и нощем и е най-силен при дъжд.");
            }
            break;
        }
        case InfoEventType::DEMOLISHED:
            log(p, ToastPriority::INFO, playerName(p) + " премахна " + std::to_string(ev.value) +
                                            (ev.value == 1 ? " сграда" : " сгради"));
            break;
        case InfoEventType::LOST_LIGHTNING: {
            std::string name = engine.getBuildingCost(ev.building).nameBg;
            push(p, ToastPriority::CRITICAL, "", "МЪЛНИЯ!", "Загубена сграда",
                 "Мълния унищожи " + name + ". Клетката е свободна за нов строеж.",
                 p == 1 ? "[SPACE]: Постройте отново" : "[ENTER]: Постройте отново", COL_CRIT, false);
            log(p, ToastPriority::CRITICAL, "Мълния унищожи " + name + " (" + playerName(p) + ")");
            break;
        }
        case InfoEventType::PLOT_BOUGHT:
            log(p, ToastPriority::INFO, playerName(p) + " купи парцел (" + std::to_string(ev.value) + " от " +
                                            std::to_string(Balance::PLOTS_PER_PLAYER) + ")");
            opponentAlert(0, "Съперникът купи земя",
                          "Вече има " + std::to_string(ev.value) + " от " + std::to_string(Balance::PLOTS_PER_PLAYER) +
                              " парцела.");
            break;
        case InfoEventType::MINE_UPGRADED:
            log(p, ToastPriority::INFO, playerName(p) + " надгради " + mineBg(ev.resource) + " до ниво " +
                                            std::to_string(ev.value));
            if (ev.value >= 4) {
                opponentAlert(2, "Съперникът надгради мина", std::string("Ниво ") + std::to_string(ev.value) +
                                                                  " на " + mineBg(ev.resource) + ".");
            }
            break;
        case InfoEventType::DAY_SETTLED: {
            const auto outcome = static_cast<UI_matchStats::DayOutcome>(ev.value2);
            const std::string dayTag = "ДЕН " + std::to_string(ev.value);
            const int p1Pct = static_cast<int>(std::lround(ev.share * 100.0f));
            const std::string demandStr = std::to_string(std::max(0, ev.demand));
            log(0, outcome == UI_matchStats::DayOutcome::NONE_MET ? ToastPriority::WARNING : ToastPriority::INFO,
                engine.getCityState().lastCutMessage);

            if (outcome == UI_matchStats::DayOutcome::GRACE) {
                for (int q = 1; q <= 2; ++q) {
                    if (!isHuman(q, botActive)) continue;
                    push(q, ToastPriority::INFO, "day", dayTag, "Денят приключи",
                         "Гратисен период: градът още не иска ток. Строете и купувайте земя.", "", COL_GOOD, false);
                }
            } else if (outcome == UI_matchStats::DayOutcome::P1_TOOK || outcome == UI_matchStats::DayOutcome::P2_TOOK) {
                const int winner = (outcome == UI_matchStats::DayOutcome::P1_TOOK) ? 1 : 2;
                const int loser = (winner == 1) ? 2 : 1;
                const int winPct = (winner == 1) ? p1Pct : 100 - p1Pct;
                if (isHuman(winner, botActive)) {
                    push(winner, ToastPriority::INFO, "day", dayTag, "Градът: " + std::to_string(winPct) + "% ваш",
                         "Доставихте средно " + std::to_string(std::max(0, ev.avgMW[winner - 1])) + " от " + demandStr +
                             " MW. Съперникът не успя и загуби територия.",
                         "", COL_GOOD, false);
                }
                if (isHuman(loser, botActive)) {
                    push(loser, ToastPriority::CRITICAL, "day", dayTag, "Остават ви " + std::to_string(100 - winPct) + "%",
                         "Доставихте средно " + std::to_string(std::max(0, ev.avgMW[loser - 1])) + " от " + demandStr +
                             " MW. Нужни са още централи или батерии.",
                         "", COL_CRIT, false);
                }
            } else if (outcome == UI_matchStats::DayOutcome::BOTH_MET) {
                for (int q = 1; q <= 2; ++q) {
                    if (!isHuman(q, botActive)) continue;
                    push(q, ToastPriority::INFO, "day", dayTag, "Градът е захранен",
                         "И двамата покрихте " + demandStr + " MW. Територията не се променя.", "", COL_GOOD, false);
                }
            } else {
                for (int q = 1; q <= 2; ++q) {
                    if (!isHuman(q, botActive)) continue;
                    push(q, ToastPriority::WARNING, "day", dayTag, "Градът остана без ток",
                         "Никой не покри средно " + demandStr + " MW. Територията не се променя.", "", COL_WARN, false);
                }
            }
            break;
        }
        case InfoEventType::GRACE_ENDED:
            for (int q = 1; q <= 2; ++q) {
                if (!isHuman(q, botActive)) continue;
                push(q, ToastPriority::WARNING, "grace", "ВНИМАНИЕ", "Край на гратисния период",
                     "От днес градът иска средно " + std::to_string(Balance::STARTING_CITY_DEMAND_MW) +
                         " MW за деня. Отчет всяка сутрин в 06:00.",
                     "Задръжте [Tab] за енергийното табло", COL_WARN, false);
            }
            break;
        case InfoEventType::SEASON_CHANGED: {
            const auto season = static_cast<SeasonType>(ev.value);
            std::string title = std::string("Започна ") + seasonBg(season);
            for (int q = 1; q <= 2; ++q) {
                if (!isHuman(q, botActive)) continue;
                push(q, ToastPriority::INFO, "alert-season", "СЕЗОН", title, sunTimes(season), "", COL_CITY, false);
            }
            log(0, ToastPriority::INFO, title + ". " + sunTimes(season));
            break;
        }
        case InfoEventType::WEATHER_CHANGED: {
            const auto w = static_cast<WeatherType>(ev.value);
            const SeasonType s = engine.getSeason();
            const float noon = 0.5f * (Balance::getSunriseHour(s) + Balance::getSunsetHour(s));
            std::string mults = "Слънце x" + fmtMult(WeatherSystem::getSolarMultiplier(w, noon, s)) + " · Вятър x" +
                                fmtMult(WeatherSystem::getWindMultiplier(w, 12.0f)) + " · ВЕЦ x" +
                                fmtMult(WeatherSystem::getHydroMultiplier(w));
            log(p, ToastPriority::INFO, playerName(p) + ": времето е " + weatherBg(w) + " (" + mults + ")");
            if (w == WeatherType::STORMY && isHuman(p, botActive)) {
                push(p, ToastPriority::WARNING, "weather", "ВРЕМЕ", "Буря в сектора ви",
                     "Мълнии могат да унищожат сгради. " + mults + ".", "", COL_WARN, false);
            }
            break;
        }
        case InfoEventType::MATCH_ENDED:
            log(0, ToastPriority::CRITICAL, engine.getCityState().lastCutMessage);
            break;
    }
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

void UI_notifications::draw(sf::RenderTarget& target, const sf::Font& font) const {
    drawPlayerStack(target, font, 1);
    drawPlayerStack(target, font, 2);
}

void UI_notifications::drawPlayerStack(sf::RenderTarget& target, const sf::Font& font, int player) const {
    const std::deque<Toast>& q = toasts[player - 1];
    if (q.empty()) return;

    // Most important first, newest first within the same priority
    std::vector<const Toast*> order;
    for (const auto& t : q) order.push_back(&t);
    std::sort(order.begin(), order.end(), [](const Toast* a, const Toast* b) {
        if (a->priority != b->priority) return a->priority > b->priority;
        return a->serial > b->serial;
    });

    const float baseX = (player == 1) ? TOAST_X_P1 : TOAST_X_P2;
    const float inner = TOAST_W - 2.0f * TOAST_PAD;
    float y = TOAST_TOP;

    for (std::size_t i = 0; i < order.size(); ++i) {
        const Toast& t = *order[i];
        const bool expanded = (i == 0);
        const float fade = std::min(1.0f, t.timer / 0.6f);
        const float slide = (1.0f - std::min(1.0f, t.age / 0.18f)) * 26.0f;
        const float x = baseX + ((player == 1) ? -slide : slide);
        const bool critical = (t.priority == ToastPriority::CRITICAL);

        // Badge pill
        const unsigned badgeSize = expanded ? 10u : 9u;
        const float badgeW = std::clamp(infoText::advance(font, toUtf8(t.badge), badgeSize, true) + 12.0f, 30.0f, 96.0f);

        if (!t.wrapped) {
            t.detailLines = infoText::wrap(font, t.detail, 11, inner, 3);
            t.actionLines = t.action.empty() ? std::vector<sf::String>() : infoText::wrap(font, t.action, 10, inner, 2);
            t.wrapped = true;
        }

        float h = COMPACT_H;
        if (expanded) {
            h = 33.0f + 14.0f * static_cast<float>(t.detailLines.size()) +
                (t.actionLines.empty() ? 0.0f : 3.0f + 13.0f * static_cast<float>(t.actionLines.size())) + 8.0f;
        }
        if (y + h > TOAST_BOTTOM) break; // no room left for this toast (it stays queued)

        // Card
        sf::RectangleShape box({ TOAST_W, h });
        box.setPosition({ x, y });
        box.setFillColor(withAlpha(sf::Color(COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 245), fade));
        float pulse = critical ? 0.65f + 0.35f * std::sin(t.age * 7.0f) : 1.0f;
        box.setOutlineThickness(critical ? 2.0f : 1.5f);
        box.setOutlineColor(withAlpha(t.accent, fade * pulse));
        target.draw(box);

        // Accent strip on the outer edge
        sf::RectangleShape strip({ 3.0f, h });
        strip.setPosition({ (player == 1) ? x : x + TOAST_W - 3.0f, y });
        strip.setFillColor(withAlpha(t.accent, fade));
        target.draw(strip);

        const float bx = x + TOAST_PAD;
        const float by = expanded ? y + 7.0f : y + 3.0f;
        const float bh = expanded ? 16.0f : 15.0f;
        sf::RectangleShape pill({ badgeW, bh });
        pill.setPosition({ bx, by });
        sf::Color pillCol = t.accent;
        pillCol.a = 235; // near-opaque so the dark badge text keeps its contrast
        pill.setFillColor(withAlpha(pillCol, fade));
        target.draw(pill);

        sf::Text tBadge(font, toUtf8(t.badge), badgeSize);
        tBadge.setStyle(sf::Text::Bold);
        tBadge.setString(infoText::ellipsize(font, toUtf8(t.badge), badgeSize, badgeW - 8.0f, true));
        tBadge.setFillColor(withAlpha(sf::Color(12, 16, 24), fade)); // dark text on the bright pill (contrast)
        float bw = infoText::width(tBadge);
        tBadge.setPosition({ bx + (badgeW - bw) / 2.0f, by + (expanded ? 1.0f : 1.0f) });
        target.draw(tBadge);

        const unsigned titleSize = expanded ? 12u : 11u;
        const float titleX = bx + badgeW + 6.0f;
        const float titleW = x + TOAST_W - TOAST_PAD - titleX;
        sf::Text tTitle(font, infoText::ellipsize(font, toUtf8(t.title), titleSize, titleW), titleSize);
        tTitle.setFillColor(withAlpha(COL_TITLE, fade));
        tTitle.setPosition({ titleX, expanded ? y + 7.0f : y + 3.0f });
        target.draw(tTitle);

        if (expanded) {
            sf::RectangleShape sep({ inner, 1.0f });
            sep.setPosition({ bx, y + 28.0f });
            sep.setFillColor(withAlpha(COL_SEPARATOR, fade));
            target.draw(sep);

            float ly = y + 32.0f;
            for (const sf::String& line : t.detailLines) {
                sf::Text tl(font, line, 11);
                tl.setFillColor(withAlpha(COL_DETAIL, fade));
                tl.setPosition({ bx, ly });
                target.draw(tl);
                ly += 14.0f;
            }
            if (!t.actionLines.empty()) {
                ly += 3.0f;
                for (const sf::String& line : t.actionLines) {
                    sf::Text tl(font, line, 10);
                    tl.setFillColor(withAlpha(COL_ACTION, fade));
                    tl.setPosition({ bx, ly });
                    target.draw(tl);
                    ly += 13.0f;
                }
            }

            // Remaining time
            float w = inner * std::max(0.0f, t.timer / t.maxTimer);
            sf::RectangleShape prog({ w, 2.0f });
            prog.setPosition({ bx, y + h - 4.0f });
            prog.setFillColor(withAlpha(t.accent, fade));
            target.draw(prog);
        }
        y += h + TOAST_GAP;
    }
}

void UI_notifications::scrollLog(int rows) {
    int maxScroll = std::max(0, static_cast<int>(entries.size()) - 1);
    logScroll = std::clamp(logScroll + rows, 0, maxScroll);
}

void UI_notifications::drawLog(sf::RenderTarget& target, const sf::Font& font) const {
    const sf::Vector2f pos(300.0f, 120.0f);
    const sf::Vector2f size(1000.0f, 660.0f);
    const int visibleRows = 25;
    const float rowH = 21.0f;

    sf::RectangleShape dim({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    dim.setFillColor(sf::Color(5, 8, 14, 150));
    target.draw(dim);

    sf::RectangleShape card(size);
    card.setPosition(pos);
    card.setFillColor(sf::Color(COL_LOG_BG.r, COL_LOG_BG.g, COL_LOG_BG.b, 252));
    card.setOutlineThickness(2.5f);
    card.setOutlineColor(COL_P1);
    target.draw(card);

    sf::RectangleShape header({ size.x, 52.0f });
    header.setPosition(pos);
    header.setFillColor(sf::Color(22, 35, 58));
    target.draw(header);

    sf::Text title(font, toUtf8("ДНЕВНИК НА СЪБИТИЯТА"), 20);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(COL_P1);
    title.setPosition({ pos.x + 24.0f, pos.y + 13.0f });
    target.draw(title);

    std::string countStr = std::to_string(entries.size()) + (entries.size() == 1 ? " запис" : " записа") +
                           " · най-новите са най-отгоре";
    sf::Text count(font, toUtf8(countStr), 12);
    count.setFillColor(COL_LOG_DIM);
    float cw = infoText::width(count);
    count.setPosition({ pos.x + size.x - 24.0f - cw, pos.y + 19.0f });
    target.draw(count);

    const float listTop = pos.y + 64.0f;
    if (entries.empty()) {
        sf::Text empty(font, toUtf8("Още няма събития. Те се появяват тук по време на мача."), 14);
        empty.setFillColor(COL_LOG_DIM);
        empty.setPosition({ pos.x + 30.0f, listTop + 10.0f });
        target.draw(empty);
    }

    const int total = static_cast<int>(entries.size());
    for (int row = 0; row < visibleRows; ++row) {
        int idx = total - 1 - logScroll - row; // newest first
        if (idx < 0) break;
        const LogEntry& e = entries[static_cast<std::size_t>(idx)];
        const float ry = listTop + static_cast<float>(row) * rowH;

        if (row % 2 == 0) {
            sf::RectangleShape zebra({ size.x - 32.0f, rowH });
            zebra.setPosition({ pos.x + 16.0f, ry });
            zebra.setFillColor(COL_LOG_ROW);
            target.draw(zebra);
        }

        std::string stamp = "Д" + std::to_string(e.day) + "  " + Balance::formatHourMinute(e.hour);
        sf::Text tStamp(font, toUtf8(stamp), 12);
        tStamp.setFillColor(COL_LOG_DIM);
        tStamp.setPosition({ pos.x + 26.0f, ry + 3.0f });
        target.draw(tStamp);

        // Who: a small coloured chip (text label too, not colour only)
        const char* who = (e.player == 1) ? "P1" : (e.player == 2 ? "P2" : "ГРАД");
        sf::Color whoCol = (e.player == 1) ? COL_P1 : (e.player == 2 ? COL_P2 : COL_CITY);
        sf::RectangleShape chip({ 44.0f, 15.0f });
        chip.setPosition({ pos.x + 116.0f, ry + 3.0f });
        chip.setFillColor(whoCol);
        target.draw(chip);
        sf::Text tWho(font, toUtf8(who), 10);
        tWho.setStyle(sf::Text::Bold);
        tWho.setFillColor(sf::Color(12, 16, 24));
        float ww = infoText::width(tWho);
        tWho.setPosition({ pos.x + 116.0f + (44.0f - ww) / 2.0f, ry + 4.0f });
        target.draw(tWho);

        sf::Color textCol = (e.priority == ToastPriority::CRITICAL) ? sf::Color(255, 140, 140)
                          : (e.priority == ToastPriority::WARNING ? sf::Color(255, 205, 120) : sf::Color(225, 238, 255));
        const float textX = pos.x + 172.0f;
        const float textW = pos.x + size.x - 26.0f - textX;
        sf::Text tText(font, infoText::ellipsize(font, toUtf8(e.text), 12, textW), 12);
        tText.setFillColor(textCol);
        tText.setPosition({ textX, ry + 3.0f });
        target.draw(tText);
    }

    // Footer: scroll position and keys
    std::string footer = "[Стрелки] или колелцето на мишката: превъртане   ·   [L] / [ESC]: обратно към паузата";
    if (total > visibleRows) {
        int from = logScroll + 1;
        int to = std::min(total, logScroll + visibleRows);
        footer = std::to_string(from) + "-" + std::to_string(to) + " от " + std::to_string(total) + "   ·   " + footer;
    }
    sf::Text tFoot(font, toUtf8(footer), 12);
    tFoot.setFillColor(COL_ACTION);
    float fw = infoText::width(tFoot);
    tFoot.setPosition({ pos.x + (size.x - fw) / 2.0f, pos.y + size.y - 30.0f });
    target.draw(tFoot);
}

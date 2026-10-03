// =============================================================================
// ENERGY CRISIS - JUDGE DEMO MODE (HX-02): the default ~3-minute beat script.
//                                                                    [Team Demo]
// Pure data: one DemoBeat per line. Times are demo seconds (fixed 60 Hz clock).
// Grid cells "c,r" are the player's 9 x 12 building slots (engine getGridSlot):
//   West (P1): start plot = cells 0..2 x 0..2, river-bank plot (0,2) = cells 6..8 x 0..2
//   East (P2): start plot = cells 6..8 x 0..2, river-bank plot (0,0) = cells 0..2 x 0..2
// Plots "row,col" are the 4 x 3 plot grid of that player.
//
// TODO beats (.todo(...)) belong to features that are merged later. They are skipped
// while their action is not registered; registering the action (registerDemoAction)
// switches them on without touching this file. See docs/demo_mode.md.
// =============================================================================
#include "../includes/game_demo.h"

namespace Demo {

std::vector<DemoBeat> defaultDemoScript() {
    std::vector<DemoBeat> s;

    // ---- 0:00  Opening ------------------------------------------------------------------
    s.push_back(DemoBeat(0.0f).title("ЕНЕРГИЙНА КРИЗА", "Демо за журито: един мач, всички механики, 3 минути")
                    .act("weather", "player=0 type=sunny").speed(1.0f).holdFor(6.0f));
    s.push_back(DemoBeat(6.0f).say("ДВАМА ЕНЕРГИЙНИ МАГНАТИ",
                                   "Запад (син) срещу Изток (розов): който захранва града, печели територия"));

    // ---- 0:13  Economy: mining, first plants, land --------------------------------------
    s.push_back(DemoBeat(13.0f).say("ДОБИВ НА РЕСУРСИ", "Дърво, желязо, мед, силиций, въглища, сребро и злато от мините")
                    .act("mine", "player=0 res=wood,iron,copper,silicon times=2"));
    s.push_back(DemoBeat(16.0f).act("mine", "player=0 res=coal,silver,gold times=2"));

    s.push_back(DemoBeat(20.0f).say("ПЪРВИТЕ ЦЕНТРАЛИ", "И двамата залагат на слънцето: всеки панел дава до 60 MW")
                    .act("build", "player=1 type=solar cells=0,0"));
    s.push_back(DemoBeat(21.5f).act("build", "player=2 type=solar cells=8,0"));
    s.push_back(DemoBeat(23.0f).act("build", "player=1 type=solar cells=1,0"));
    s.push_back(DemoBeat(24.5f).act("build", "player=2 type=solar cells=7,0"));
    s.push_back(DemoBeat(26.0f).act("build", "player=1 type=solar cells=2,0"));
    s.push_back(DemoBeat(27.5f).act("build", "player=2 type=solar cells=6,0"));

    s.push_back(DemoBeat(29.0f).say("ЗЕМЯ ДО РЕКАТА", "Златото купува нови парцели; ВЕЦ се строи само на брега на реката")
                    .act("buy_plot", "player=1 plot=0,2"));
    s.push_back(DemoBeat(31.0f).act("build", "player=1 type=hydro cells=6,1"));
    s.push_back(DemoBeat(33.0f).act("buy_plot", "player=2 plot=0,0"));

    // ---- 0:36  Day and night ---------------------------------------------------------------
    s.push_back(DemoBeat(36.0f).say("ДЕН И НОЩ", "Слънцето залязва: панелите спират, батериите и лампите поемат")
                    .act("build", "player=1 type=battery cells=0,1;1,1").speed(10.0f));
    s.push_back(DemoBeat(38.0f).act("build", "player=1 type=lamp cells=2,1"));
    s.push_back(DemoBeat(39.0f).act("build", "player=2 type=lamp cells=7,1"));

    // ---- 0:45  The city starts to demand power, storm ---------------------------------------
    s.push_back(DemoBeat(45.0f).say("ГРАТИСНИЯТ ПЕРИОД СВЪРШИ", "От ден 3 градът иска ток: {demand} MW, после +15 MW всеки ден")
                    .act("jump_day", "day=3 hour=7").speed(3.0f));
    s.push_back(DemoBeat(45.0f).act("weather", "player=2 type=stormy"));
    s.push_back(DemoBeat(45.0f).act("weather", "player=1 type=rainy"));
    s.push_back(DemoBeat(50.0f).say("БУРЯ НАД ИЗТОКА, ДЪЖД НАД ЗАПАДА",
                                    "Дъждът удвоява ВЕЦ-а, а бурята почти спира слънчевите панели").speed(2.0f));
    s.push_back(DemoBeat(55.0f).say("МЪЛНИЯ!", "Мълния унищожи слънчев панел на Изтока")
                    .act("lightning", "player=2 cell=8,0 hit=1"));
    s.push_back(DemoBeat(61.0f).say("ДНЕВНО ОТЧИТАНЕ", "{result}").act("settle").speed(1.0f));

    // ---- 1:09  Summer, comeback ---------------------------------------------------------------
    s.push_back(DemoBeat(69.0f).say("СМЯНА НА СЕЗОНА: {season}", "По-дълги дни и +15% слънчева енергия")
                    .act("season", "type=summer hour=7").speed(4.0f));
    s.push_back(DemoBeat(69.0f).act("weather", "player=0 type=sunny"));
    s.push_back(DemoBeat(76.0f).say("ГРАДСКО СЪБИТИЕ", "Карта от градското тесте разклаща деня")
                    .act("event.card", "id=random").todo("City Event Deck (F-09) / boss events (HX-09)"));
    s.push_back(DemoBeat(80.0f).say("ИЗТОКЪТ ОТВРЪЩА", "ВЕЦ на реката и вятърна мелница: ток и без слънце")
                    .act("build", "player=2 type=hydro cells=1,0"));
    s.push_back(DemoBeat(82.0f).act("build", "player=2 type=wind cells=8,0"));
    s.push_back(DemoBeat(87.0f).say("ДНЕВНО ОТЧИТАНЕ", "{result}").act("settle"));
    s.push_back(DemoBeat(95.0f).say("ЯДРЕНА ЦЕНТРАЛА", "Огромна базова мощност, но и огромна отговорност")
                    .act("nuclear.build", "player=1").todo("Nuclear plant (nuclear / terrain / mega-projects team)"));

    // ---- 1:40  Winter ---------------------------------------------------------------------------
    s.push_back(DemoBeat(100.0f).say("ЗИМА", "Къси дни, сняг и слаби панели: печели който е планирал")
                    .act("jump_day", "day=16 hour=9").speed(4.0f));
    s.push_back(DemoBeat(100.0f).act("weather", "player=0 type=snowy"));
    s.push_back(DemoBeat(104.0f).say("ЗАПАДЪТ ИНВЕСТИРА", "Още два ВЕЦ-а на реката: стабилна мощност и през зимата")
                    .act("build", "player=1 type=hydro cells=7,1;8,1"));
    s.push_back(DemoBeat(111.0f).say("АВАРИЯ В ЕНЕРГОСИСТЕМАТА", "Изтокът не успя да захрани града")
                    .act("blackout.trigger", "player=2").todo("Blackout set piece (HX-04)"));
    s.push_back(DemoBeat(115.0f).say("ДНЕВНО ОТЧИТАНЕ", "{result}").act("settle"));
    s.push_back(DemoBeat(123.0f).say("МЕГАПРОЕКТ", "Многодневен строеж, който променя края на играта")
                    .act("mega.start", "player=1").todo("Mega-projects (HX-10)"));
    s.push_back(DemoBeat(129.0f).say("ЖИВО ТАБЛО", "Мощност, търсене и дял на града в реално време")
                    .act("dashboard.show", "seconds=5").todo("Real-time energy dashboard (HX-06)"));

    // ---- 2:14  Final day and victory -------------------------------------------------------------
    s.push_back(DemoBeat(134.0f).say("ФИНАЛНИЯТ ДЕН", "Ден {day}: градът иска {demand} MW. Запад {p1}% : Изток {p2}%")
                    .act("jump_day", "day=20 hour=8").speed(5.0f));
    s.push_back(DemoBeat(142.0f).say("ПОСЛЕДНИТЕ ЧАСОВЕ", "Всеки мегават решава: Запад {mw1} MW, Изток {mw2} MW"));
    s.push_back(DemoBeat(148.0f).say("КРАЙ НА МАЧА", "{result}").act("settle"));
    s.push_back(DemoBeat(156.0f).say("ПОБЕДА ЗА ЗАПАДА!", "Западът контролира {p1}% от града с чиста енергия")
                    .act("winner", "player=1 ifnone=1 share=0.86"));
    s.push_back(DemoBeat(166.0f).title("БЛАГОДАРИМ ВИ!", "Energy Crisis  •  C++17 / SFML 3  •  натиснете клавиш за изход")
                    .holdFor(14.0f));
    return s;
}

} // namespace Demo

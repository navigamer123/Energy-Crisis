#ifndef UI_THEME_H
#define UI_THEME_H

#include <SFML/Graphics/Color.hpp>
#include <cstdint>

// =============================================================================
// Energy Crisis UI theme: every UI colour and text size comes from here, so a palette
// change is one edit and the same role looks the same on every screen.
//
// Colour rules
//  - P1 is cyan, P2 is pink: player colours mean "this belongs to that player" only.
//  - Gold (#F5C542) means the gold currency only; focus / selection is white.
//  - Good / Warn / Bad carry state (enough / careful / missing, error).
//  - Every resource has one fixed hue (silicon is violet, never P1 cyan).
//  - Text on light or saturated fills uses TextOnLight (#0A0E16) for contrast.
// =============================================================================
namespace theme {

// Same colour with another alpha (panels are drawn slightly translucent over the map)
constexpr sf::Color withAlpha(sf::Color c, std::uint8_t a) {
    return sf::Color(c.r, c.g, c.b, a);
}

// --- Surfaces ----------------------------------------------------------------
constexpr sf::Color Window(10, 14, 22);          // clear colour behind everything
constexpr sf::Color Panel(16, 22, 34);           // panels, dialogs, HUD cards
constexpr sf::Color PanelHeader(24, 34, 52);     // header strips of dialogs
constexpr sf::Color Card(22, 30, 44);            // cards inside a panel
constexpr sf::Color CardHover(30, 46, 66);
constexpr sf::Color CardSelected(40, 62, 86);
constexpr sf::Color Well(12, 18, 28);            // icon wells, bar tracks
constexpr sf::Color Button(30, 40, 56);          // neutral button
constexpr sf::Color ButtonHover(55, 75, 105);
constexpr sf::Color Line(65, 88, 120);           // dividers, neutral outlines
constexpr sf::Color LineStrong(100, 125, 160);
constexpr sf::Color Dim(5, 8, 14);               // full-screen dimmers (use withAlpha)

// --- Text --------------------------------------------------------------------
constexpr sf::Color TextPrimary(232, 240, 252);
constexpr sf::Color TextSecondary(176, 194, 218);
constexpr sf::Color TextMuted(140, 160, 188);    // hints; still >= 4.5:1 on Panel
constexpr sf::Color TextOnLight(10, 14, 22);     // #0A0E16 on light or saturated fills
constexpr sf::Color Focus(255, 255, 255);        // selection / keyboard focus outline

// --- Players -----------------------------------------------------------------
constexpr sf::Color P1(0, 229, 255);             // West, cyan
constexpr sf::Color P1Light(150, 240, 255);
constexpr sf::Color P1Dark(18, 40, 60);
constexpr sf::Color P2(255, 120, 200);           // East, pink
constexpr sf::Color P2Light(255, 182, 228);
constexpr sf::Color P2Dark(48, 24, 44);
constexpr sf::Color Neutral(0, 220, 100);        // the shared centre: border line and city frame

constexpr sf::Color player(int p) { return p == 1 ? P1 : P2; }
constexpr sf::Color playerLight(int p) { return p == 1 ? P1Light : P2Light; }
constexpr sf::Color playerDark(int p) { return p == 1 ? P1Dark : P2Dark; }

// --- Semantic ----------------------------------------------------------------
constexpr sf::Color Good(110, 230, 140);         // enough, success, ready
constexpr sf::Color GoodFill(28, 110, 64);       // success buttons (white or primary text on it)
constexpr sf::Color Warn(255, 190, 80);          // cooldowns, cautions, tips
constexpr sf::Color Bad(255, 105, 105);          // missing, errors, danger
constexpr sf::Color BadFill(120, 34, 44);        // danger buttons
constexpr sf::Color WarnFill(130, 88, 24);        // caution buttons (medium difficulty)
constexpr sf::Color InfoFill(32, 78, 130);        // neutral primary buttons
constexpr sf::Color Info(120, 200, 255);         // neutral highlights (night, hints)

// --- Currency and power ------------------------------------------------------
constexpr sf::Color Gold(245, 197, 66);          // #F5C542: the gold currency only
constexpr sf::Color Money(80, 230, 150);         // city money ($)
constexpr sf::Color Energy(255, 225, 60);        // MW / electricity

// --- Resources (one fixed hue each) --------------------------------------------
constexpr sf::Color Wood(85, 205, 110);
constexpr sf::Color Iron(170, 195, 220);
constexpr sf::Color Copper(232, 142, 72);
constexpr sf::Color Coal(130, 140, 155);
constexpr sf::Color Silicon(180, 140, 255);      // #B48CFF violet
constexpr sf::Color Silver(215, 225, 240);

} // namespace theme

// =============================================================================
// Typography: six sizes (px). Nothing is drawn below Caption (11 px).
// =============================================================================
namespace fontsize {
constexpr unsigned int Display = 48; // title fallback
constexpr unsigned int H1 = 24;      // screen titles, winner line
constexpr unsigned int H2 = 16;      // dialog titles, menu buttons, section heads
constexpr unsigned int Body = 14;    // body text, HUD counters
constexpr unsigned int Label = 12;   // labels, card text, buttons in panels
constexpr unsigned int Caption = 11; // captions, hints, badges (minimum size)
} // namespace fontsize

#endif // UI_THEME_H

#ifndef UI_BOTBANNER_H
#define UI_BOTBANNER_H

// =============================================================================
// [AI team] In-match bot presentation: name tag over the bot cursor (rival + difficulty, red
// for НЕВЪЗМОЖНО), its current plan in one line, and the rival intro banner at match start.
// =============================================================================

#include <SFML/Graphics.hpp>
#include "UI_bot.h"
#include <vector>

constexpr float BOT_INTRO_BANNER_SEC = 4.5f; // how long the rival intro banner stays up

// Two-line plate above the cursor: rival + difficulty, then the bot's current plan (hidden while
// mining). Stays inside the play field and off the `avoid` cards (mine stations).
void drawBotNameTag(sf::RenderWindow& window, const sf::Font& font, const UIBot& bot, sf::Vector2f cursorPos,
                    bool showIntent, const std::vector<sf::FloatRect>& avoid = {});

// Rival intro card in the screen centre; timeLeft runs from BOT_INTRO_BANNER_SEC down to 0
void drawRivalIntroBanner(sf::RenderWindow& window, const sf::Font& font, const UIBot& bot, float timeLeft);

#endif // UI_BOTBANNER_H

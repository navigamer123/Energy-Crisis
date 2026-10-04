#include "../includes/UI_textpool.h"
#include <deque>
#include <iostream>

namespace {
// A deque never moves its elements, so references handed out this frame stay valid while it grows
std::deque<sf::Text> g_pool;
std::size_t g_used = 0;
// More texts than this in one frame means beginTextFrame() is missing from a render loop
constexpr std::size_t RUNAWAY_SLOTS = 4096;
bool g_warned = false;
} // namespace

namespace ui {

void beginTextFrame() { g_used = 0; }

sf::Text& pooledText(const sf::Font& font, const sf::String& str, unsigned int size) {
    if (g_used == g_pool.size()) {
        if (g_used == RUNAWAY_SLOTS && !g_warned) {
            g_warned = true;
            std::cerr << "[UI_textpool] " << RUNAWAY_SLOTS << " texts in one frame: is ui::beginTextFrame() called?\n";
        }
        g_pool.emplace_back(font, str, size);
        return g_pool[g_used++];
    }
    sf::Text& t = g_pool[g_used++];
    t.setFont(font);
    t.setString(str);
    t.setCharacterSize(size);
    t.setStyle(sf::Text::Regular);
    t.setFillColor(sf::Color::White);
    t.setOutlineColor(sf::Color::Black);
    t.setOutlineThickness(0.0f);
    t.setLetterSpacing(1.0f);
    t.setLineSpacing(1.0f);
    t.setPosition({ 0.0f, 0.0f });
    t.setOrigin({ 0.0f, 0.0f });
    t.setRotation(sf::degrees(0.0f));
    t.setScale({ 1.0f, 1.0f });
    return t;
}

} // namespace ui

// Cached text drawing for the match-options UI [team b-options]
#include "../includes/UI_optionsText.h"
#include "../includes/UI_types.h"
#include <map>
#include <utility>

sf::Text& OptionsTextCache::get(int slot, const sf::Font& font, const std::string& utf8, unsigned int size,
                                float maxWidth, unsigned int minSize) {
    Entry& e = entries[slot];
    if (!e.text || e.str != utf8 || e.requestedSize != size || e.maxWidth != maxWidth || &e.text->getFont() != &font) {
        e.str = utf8;
        e.requestedSize = size;
        e.maxWidth = maxWidth;
        e.text.emplace(font, toUtf8(utf8), size);
        while (e.text->getCharacterSize() > minSize && e.text->getLocalBounds().size.x > maxWidth) {
            e.text->setCharacterSize(e.text->getCharacterSize() - 1);
        }
    }
    return *e.text;
}

void OptionsTextCache::draw(sf::RenderWindow& window, int slot, const sf::Font& font, const std::string& utf8,
                            unsigned int size, sf::Color color, float x, float centerY, float maxWidth, Align align,
                            unsigned int minSize) {
    sf::Text& t = get(slot, font, utf8, size, maxWidth, minSize);
    if (t.getFillColor() != color) t.setFillColor(color);
    sf::FloatRect b = t.getLocalBounds();
    sf::FloatRect caps = capsBounds(font, t.getCharacterSize());
    float px = x - b.position.x;
    if (align == CENTER) px = x - b.size.x / 2.0f - b.position.x;
    else if (align == RIGHT) px = x - b.size.x - b.position.x;
    t.setPosition({ px, centerY - caps.size.y / 2.0f - caps.position.y });
    window.draw(t);
}

sf::FloatRect OptionsTextCache::capsBounds(const sf::Font& font, unsigned int size) {
    static std::map<std::pair<const sf::Font*, unsigned int>, sf::FloatRect> cache;
    auto key = std::make_pair(&font, size);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    sf::Text ref(font, toUtf8("НЕ"), size);
    sf::FloatRect b = ref.getLocalBounds();
    cache[key] = b;
    return b;
}

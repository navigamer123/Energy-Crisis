#ifndef UI_INFO_TEXT_H
#define UI_INFO_TEXT_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "UI_types.h"

// -----------------------------------------------------------------------------
// team info: small text-fitting helpers (UX-03) used by the information UI and the
// popup/modal fixes. Wave A brings a general text helper with layout lint; the
// integrator can point these three functions at it.
// -----------------------------------------------------------------------------
namespace infoText {

// Width of a text object from its left origin to the right edge of its last glyph
inline float width(const sf::Text& t) {
    sf::FloatRect b = t.getLocalBounds();
    return b.position.x + b.size.x;
}

// Width of a string at a character size
inline float advance(const sf::Font& font, const sf::String& s, unsigned size, bool bold = false) {
    sf::Text t(font, s, size);
    if (bold) t.setStyle(sf::Text::Bold);
    return width(t);
}

// Shrink the character size until the text fits maxWidth (never below minSize). Returns the size used.
inline unsigned fitSize(sf::Text& t, float maxWidth, unsigned minSize) {
    unsigned size = t.getCharacterSize();
    while (size > minSize && width(t) > maxWidth) {
        --size;
        t.setCharacterSize(size);
    }
    return size;
}

// Cut the string and add "…" so it fits maxWidth
inline sf::String ellipsize(const sf::Font& font, const sf::String& s, unsigned size, float maxWidth, bool bold = false) {
    if (advance(font, s, size, bold) <= maxWidth) return s;
    const sf::String dots = sf::String(static_cast<char32_t>(0x2026));
    std::size_t n = s.getSize();
    while (n > 0) {
        --n;
        sf::String cut = s.substring(0, n);
        // drop trailing spaces before the ellipsis
        while (!cut.isEmpty() && cut[cut.getSize() - 1] == U' ') cut.erase(cut.getSize() - 1);
        if (advance(font, cut + dots, size, bold) <= maxWidth) return cut + dots;
    }
    return dots;
}

// Word-wrap UTF-8 text to lines no wider than maxWidth. '\n' starts a new line. Words that are
// wider than a whole line are cut. With maxLines > 0 the last kept line ends with "…" if text was dropped.
inline std::vector<sf::String> wrap(const sf::Font& font, const std::string& utf8, unsigned size, float maxWidth,
                                    int maxLines = 0, bool bold = false) {
    std::vector<sf::String> lines;
    sf::String all = toUtf8(utf8);
    std::vector<sf::String> paragraphs;
    {
        sf::String cur;
        for (std::size_t i = 0; i < all.getSize(); ++i) {
            if (all[i] == U'\n') { paragraphs.push_back(cur); cur.clear(); }
            else cur += all[i];
        }
        paragraphs.push_back(cur);
    }
    bool truncated = false;
    const sf::String space(U" ");
    for (const sf::String& para : paragraphs) {
        std::vector<sf::String> words;
        {
            sf::String w;
            for (std::size_t i = 0; i < para.getSize(); ++i) {
                if (para[i] == U' ') { if (!w.isEmpty()) words.push_back(w); w.clear(); }
                else w += para[i];
            }
            if (!w.isEmpty()) words.push_back(w);
        }
        sf::String line;
        for (sf::String word : words) {
            sf::String candidate = line.isEmpty() ? word : line + space + word;
            if (advance(font, candidate, size, bold) <= maxWidth) {
                line = candidate;
                continue;
            }
            if (!line.isEmpty()) {
                lines.push_back(line);
                line.clear();
            }
            // a single word wider than the line is cut into pieces
            while (advance(font, word, size, bold) > maxWidth && word.getSize() > 1) {
                std::size_t k = word.getSize() - 1;
                while (k > 1 && advance(font, word.substring(0, k), size, bold) > maxWidth) --k;
                lines.push_back(word.substring(0, k));
                word = word.substring(k);
            }
            line = word;
        }
        lines.push_back(line);
    }
    if (maxLines > 0 && static_cast<int>(lines.size()) > maxLines) {
        lines.resize(static_cast<std::size_t>(maxLines));
        truncated = true;
    }
    if (truncated && !lines.empty()) {
        sf::String& last = lines.back();
        last = ellipsize(font, last + sf::String(static_cast<char32_t>(0x2026)), size, maxWidth, bold);
    }
    return lines;
}

} // namespace infoText

#endif // UI_INFO_TEXT_H

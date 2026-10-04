#ifndef UI_TEXTPOOL_H
#define UI_TEXTPOOL_H

#include <SFML/Graphics.hpp>

// -----------------------------------------------------------------------------
// Frame-slot text pool (PF-02). On the Intel UHD drivers the first draw of a freshly
// constructed (or copied) sf::Text costs about 0.4 ms, while a text object that lives on
// across frames costs about 15 us even when its string changes. The UI used to build
// about 110 texts per frame, so every per-frame text now comes from this pool:
//
//     sf::Text& label = ui::pooledText(font, toUtf8("..."), fontsize::Label);
//
// The N-th call of a frame always returns the same persistent object, reset to the
// sf::Text defaults. The reference is valid until the next beginTextFrame(), which the
// main loop calls once at the start of every frame. Never copy it into an sf::Text.
// -----------------------------------------------------------------------------
namespace ui {

void beginTextFrame();
sf::Text& pooledText(const sf::Font& font, const sf::String& str = "", unsigned int size = 30);

} // namespace ui

#endif // UI_TEXTPOOL_H

#ifndef UI_POSTFX_H
#define UI_POSTFX_H

// =============================================================================
// Team b-session (HX-15): projector picture. When projector mode is on, the finished
// frame is copied into a texture and drawn back through a small fragment shader that
// lifts dark tones, raises contrast and saturation, so the game survives a washed-out
// school projector. No shader support (old drivers): the frame is left untouched.
// =============================================================================

#include <SFML/Graphics.hpp>

class UI_postfx {
public:
    // Call right before window.display()
    void apply(sf::RenderWindow& window);
    bool isSupported(); // loads the shader on first use

private:
    bool tried = false;
    bool ok = false;
    sf::Shader shader;
    sf::Texture frame;
};

#endif // UI_POSTFX_H

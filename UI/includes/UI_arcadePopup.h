#ifndef UI_ARCADAPOPUP_H
#define UI_ARCADAPOPUP_H

#include <SFML/Graphics.hpp>
#include <string>

class ArcadePopup {
public:
    static ArcadePopup& get();

    void init();
    void show(const std::string& message, float durationSeconds = 2.0f);
    void update(float dt);
    void draw(sf::RenderWindow& window);
    bool isVisible() const { return timer > 0.0f; }

private:
    ArcadePopup();
    sf::Font font;
    bool fontLoaded = false;
    std::string currentMessage;
    float timer = 0.0f;
    float maxDuration = 2.0f;
};

#endif // UI_ARCADAPOPUP_H

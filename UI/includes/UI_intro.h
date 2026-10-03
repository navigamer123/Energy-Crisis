#ifndef UI_INTRO_H
#define UI_INTRO_H

// =============================================================================
// [b-showcase] Cinematic intro (HX-01)
//
// Plays once before the main menu: a stormy night skyline, two lightning bolts strike the
// Energy Crisis logo, the city lights flicker on in both players' colours and the tagline
// appears. Any key, mouse button or gamepad button skips it (short fade). About 6 seconds.
// Set EC_SKIP_INTRO=1 to start straight in the menu (screenshots, automated runs).
// =============================================================================

#include <SFML/Graphics.hpp>
#include <random>
#include <string>
#include <vector>

class UI_intro {
public:
    UI_intro();

    // False when the EC_SKIP_INTRO environment variable is set (any value except "0")
    static bool enabledByEnvironment();

    void restart();
    // Advances the intro with its own clock (frame time capped) and draws it (virtual 1600x900)
    void render(sf::RenderTarget& target);
    // Any key / mouse button / gamepad button starts the skip fade. Returns true when consumed.
    bool handleEvent(const sf::Event& event);
    void skip();
    bool isFinished() const { return finished; }

    // Screenshot helper: freezes the intro at time t (seconds)
    void debugSetTime(float t);
    // Audio hook: "thunder" (each bolt) and "sting" (logo reveal), once each. There is no audio
    // system on this branch; the integrator forwards these cues to the audio director.
    bool takeSoundCue(std::string& outCue);

    static constexpr float DURATION = 6.4f;

private:
    struct Bolt {
        std::vector<sf::Vector2f> main;
        std::vector<std::vector<sf::Vector2f>> branches;
        float at = 0.0f; // Strike time
    };
    struct CityWindow {
        sf::FloatRect rect;
        float onAt = 0.0f;
        int hash = 0;
        sf::Color color;
    };
    struct Antenna {
        sf::Vector2f top;
        float phase = 0.0f;
    };

    sf::Texture logoTexture;
    bool logoLoaded = false;
    sf::Font font;
    bool fontLoaded = false;

    sf::Clock clock;
    float t = 0.0f;
    bool frozen = false;
    bool skipping = false;
    float skipStartT = 0.0f;
    bool finished = false;
    float lastCueT = -1.0f;

    std::mt19937 rng;
    std::vector<Bolt> bolts;
    std::vector<std::vector<sf::Vector2f>> arcs; // Short electric arcs crawling over the logo
    std::vector<sf::FloatRect> skylineBlocks;
    std::vector<CityWindow> windows;
    std::vector<Antenna> antennas;
    std::vector<sf::Vector3f> stars; // x, y, twinkle phase
    std::vector<sf::Vector3f> rain;  // x, y, speed
    std::vector<std::string> soundCues;

    void buildScene();
    Bolt makeBolt(sf::Vector2f from, sf::Vector2f to, float at);
    void queueCues(float from, float to);
    float fadeAlpha() const; // 0..1 black fade (start, end and skip)
};

#endif // UI_INTRO_H

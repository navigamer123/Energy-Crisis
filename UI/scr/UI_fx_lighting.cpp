// [b-effects] DS-11 Lighting pass (stub; filled in by a later commit)
#include "../includes/UI_fx.h"

void UI_fx::computeAmbient() {
    night = daylight ? 0.0f : 1.0f;
    for (int c = 0; c < 3; ++c) skyTop[c] = skyBottom[c] = sf::Color::White;
}
sf::Color UI_fx::ambientColorAt(float x) const { (void)x; return sf::Color::White; }
void UI_fx::ensureLightingResources() {}
void UI_fx::renderLightMap() {}
void UI_fx::applyLighting(sf::RenderTarget& target, bool mildPass) { (void)target; (void)mildPass; }

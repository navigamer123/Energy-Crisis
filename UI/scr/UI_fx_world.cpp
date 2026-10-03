// [b-effects] DS-10 River, bridges and traffic (stub; filled in by a later commit)
#include "../includes/UI_fx.h"

void UI_fx::initTraffic() { cars.clear(); }
void UI_fx::advanceTraffic(float dt) { (void)dt; }
void UI_fx::drawRiver(sf::RenderTarget& target) const { (void)target; }
void UI_fx::drawBorderTag(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded) const { (void)target; (void)font; (void)fontLoaded; }

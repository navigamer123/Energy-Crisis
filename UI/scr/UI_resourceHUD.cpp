#include "../includes/UI_resourceHUD.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_icons.h"
#include "../includes/UI_types.h"
#include "../includes/UI_settings.h"
#include <cmath>
#include <string>

UI_resourceHUD::UI_resourceHUD()
    : p1BuyLandBtn({ 14.0f, 834.0f }, { 186.0f, 26.0f }),
      p2BuyLandBtn({ 1600.0f - 200.0f, 834.0f }, { 186.0f, 26.0f }) {
}

void UI_resourceHUD::drawQuarterCircle(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                       const PlayerEconomy& econ, bool isWest) {
    const float screenWidth = VIRTUAL_WIDTH;
    const float screenHeight = VIRTUAL_HEIGHT;
    const float R = 250.0f; // Radius of the corner quarter-circle
    const int player = isWest ? 1 : 2;
    const sf::Color accent = theme::player(player);

    // -------------------------------------------------------------------------
    // Quarter circle geometry (bottom-left for P1, bottom-right for P2)
    // -------------------------------------------------------------------------
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan);
    sf::VertexArray border(sf::PrimitiveType::LineStrip);
    const sf::Vector2f corner = isWest ? sf::Vector2f(0.0f, screenHeight) : sf::Vector2f(screenWidth, screenHeight);
    fan.append(sf::Vertex{ corner, theme::withAlpha(theme::Panel, 245), {} });
    const int degFrom = isWest ? -90 : 180;
    for (int deg = degFrom; deg <= degFrom + 90; deg += 3) {
        float rad = static_cast<float>(deg) * 3.14159265f / 180.0f;
        sf::Vector2f pt(corner.x + R * std::cos(rad), corner.y + R * std::sin(rad));
        fan.append(sf::Vertex{ pt, theme::withAlpha(theme::playerDark(player), 245), {} });
        border.append(sf::Vertex{ pt, theme::withAlpha(accent, 220), {} });
    }
    border.append(sf::Vertex{ corner, theme::withAlpha(accent, 180), {} });
    border.append(sf::Vertex{ isWest ? sf::Vector2f(0.0f, screenHeight - R) : sf::Vector2f(screenWidth - R, screenHeight),
                              theme::withAlpha(accent, 180), {} });
    window.draw(fan);
    window.draw(border);

    sf::Vector2f mousePos = ui::pointerPos(window);

    // -------------------------------------------------------------------------
    // Stock: icon + number. Numbers are primary text; the colour is carried by the icon,
    // except gold (the currency) and money.
    // -------------------------------------------------------------------------
    struct Slot {
        ResourceType type;
        int value;
        const char* suffix;
        sf::Color color;
    };
    const Slot leftCol[4] = {
        { ResourceType::WOOD, econ.wood, "", theme::TextPrimary },
        { ResourceType::IRON, econ.iron, "", theme::TextPrimary },
        { ResourceType::COPPER, econ.copper, "", theme::TextPrimary },
        { ResourceType::COAL, econ.coal, "", theme::TextPrimary },
    };
    const Slot rightCol[4] = {
        { ResourceType::SILICON, econ.silicon, "", theme::TextPrimary },
        { ResourceType::SILVER, econ.silver, "", theme::TextPrimary },
        { ResourceType::GOLD, econ.gold, " G", theme::Gold },
        { ResourceType::MONEY, econ.money, " $", theme::Money },
    };
    const float col1X = isWest ? 18.0f : screenWidth - 196.0f;
    const float col2X = isWest ? 96.0f : screenWidth - 106.0f;
    const float rowY[4] = { screenHeight - 216.0f, screenHeight - 186.0f, screenHeight - 156.0f, screenHeight - 126.0f };

    auto drawSlot = [&](const Slot& s, float x, float y) {
        drawResourceIcon(window, s.type, { x + 8.0f, y + 8.0f }, 16.0f);
        if (!fontLoaded) return;
        sf::Text& t = ui::pooledText(font, toUtf8(std::to_string(s.value) + s.suffix), fontsize::H2);
        t.setStyle(sf::Text::Bold);
        t.setFillColor(s.color);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ x + 22.0f - tb.position.x, y + 8.0f - tb.size.y / 2.0f - tb.position.y });
        ui::drawText(window, t);
    };
    for (int i = 0; i < 4; ++i) {
        drawSlot(leftCol[i], col1X, rowY[i]);
        drawSlot(rightCol[i], col2X, rowY[i]);
    }

    // -------------------------------------------------------------------------
    // Power plaque: MW delivered now and the player's share of the city
    // -------------------------------------------------------------------------
    const float plaqueX = isWest ? 14.0f : screenWidth - 200.0f;
    sf::RectangleShape energyPlaque({ 186.0f, 26.0f });
    energyPlaque.setPosition({ plaqueX, screenHeight - 98.0f });
    energyPlaque.setFillColor(theme::withAlpha(theme::Panel, 235));
    energyPlaque.setOutlineThickness(1.0f);
    energyPlaque.setOutlineColor(accent);
    window.draw(energyPlaque);
    drawResourceIcon(window, ResourceType::ENERGY, { plaqueX + 13.0f, screenHeight - 85.0f }, 16.0f);
    if (fontLoaded) {
        bool isEn = (UI_settings::get().getLanguage() == "en");
        int sharePct = static_cast<int>(std::lround(econ.cityInfluence * 100.0f));
        std::string pStr = std::to_string(econ.energyMW) + (isEn ? " MW · city " : " MW · град ") + std::to_string(sharePct) + "%";
        sf::Text& tPwr = ui::pooledText(font, toUtf8(pStr), fontsize::Label);
        tPwr.setStyle(sf::Text::Bold);
        tPwr.setFillColor(theme::Energy);
        sf::FloatRect pb = tPwr.getLocalBounds();
        tPwr.setPosition({ plaqueX + 26.0f - pb.position.x, screenHeight - 85.0f - pb.size.y / 2.0f - pb.position.y });
        ui::drawText(window, tPwr, sf::FloatRect(energyPlaque.getPosition(), energyPlaque.getSize()));
    }

    // -------------------------------------------------------------------------
    // Land expansion button (buys the cheapest plot still for sale)
    // -------------------------------------------------------------------------
    sf::FloatRect& landRect = isWest ? p1BuyLandBtn : p2BuyLandBtn;
    landRect = sf::FloatRect({ plaqueX, screenHeight - 66.0f }, { 186.0f, 26.0f });
    bool hoverLand = landRect.contains(mousePos);
    sf::RectangleShape landBtn(landRect.size);
    landBtn.setPosition(landRect.position);
    landBtn.setFillColor(hoverLand ? theme::ButtonHover : theme::playerDark(player));
    landBtn.setOutlineThickness(hoverLand ? 2.0f : 1.0f);
    landBtn.setOutlineColor(hoverLand ? theme::Focus : accent);
    window.draw(landBtn);
    if (fontLoaded) {
        bool isEn = (UI_settings::get().getLanguage() == "en");
        sf::Text& tLand = ui::pooledText(font, toUtf8(isEn ? "+ BUY LAND" : "+ КУПИ ЗЕМЯ"), fontsize::Label);
        tLand.setStyle(sf::Text::Bold);
        tLand.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = tLand.getLocalBounds();
        tLand.setPosition({ landRect.position.x + (landRect.size.x - tb.size.x) / 2.0f - tb.position.x,
                            landRect.position.y + (landRect.size.y - tb.size.y) / 2.0f - tb.position.y });
        ui::drawText(window, tLand, landRect);
    }
}

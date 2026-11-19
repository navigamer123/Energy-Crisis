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
    : p1BuyLandBtn({ 12.0f, 814.0f }, { 196.0f, 34.0f }),
      p2BuyLandBtn({ 1600.0f - 208.0f, 814.0f }, { 196.0f, 34.0f }) {
}

void UI_resourceHUD::drawQuarterCircle(sf::RenderTarget& window, const sf::Font& font, bool fontLoaded,
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
    const float col1X = isWest ? 12.0f : screenWidth - 138.0f;
    const float col2X = isWest ? 76.0f : screenWidth - 76.0f;
    const float maxSlotTextW = isWest ? 38.0f : 44.0f;
    const float rowY[4] = { screenHeight - 205.0f, screenHeight - 184.0f, screenHeight - 163.0f, screenHeight - 142.0f };

    auto drawSlot = [&](const Slot& s, float x, float y, float maxW) {
        drawResourceIcon(window, s.type, { x + 8.0f, y + 8.0f }, 16.0f);
        if (!fontLoaded) return;
        std::string valStr = std::to_string(s.value) + s.suffix;
        unsigned int sz = ui::fitTextSize(font, valStr, fontsize::H2, fontsize::Caption, maxW, true);
        sf::Text& t = ui::pooledText(font, toUtf8(valStr), sz);
        t.setStyle(sf::Text::Bold);
        t.setFillColor(s.color);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ x + 21.0f - tb.position.x, y + 8.0f - tb.size.y / 2.0f - tb.position.y });
        ui::drawText(window, t);
    };
    for (int i = 0; i < 4; ++i) {
        drawSlot(leftCol[i], col1X, rowY[i], isWest ? 39.0f : 38.0f);
        drawSlot(rightCol[i], col2X, rowY[i], isWest ? 48.0f : maxSlotTextW);
    }

    // -------------------------------------------------------------------------
    // Power plaque: MW delivered now and the player's share of the city
    // -------------------------------------------------------------------------
    const float plaqueX = isWest ? 12.0f : screenWidth - 208.0f;
    const float plaqueW = 196.0f;
    const float plaqueH = 28.0f;
    const float plaqueY = screenHeight - 120.0f;
    sf::RectangleShape energyPlaque({ plaqueW, plaqueH });
    energyPlaque.setPosition({ plaqueX, plaqueY });
    energyPlaque.setFillColor(theme::withAlpha(theme::Panel, 235));
    energyPlaque.setOutlineThickness(1.2f);
    energyPlaque.setOutlineColor(accent);
    window.draw(energyPlaque);
    drawResourceIcon(window, ResourceType::ENERGY, { plaqueX + 13.0f, plaqueY + plaqueH / 2.0f }, 16.0f);
    if (fontLoaded) {
        bool isEn = (UI_settings::get().getLanguage() == "en");
        int sharePct = static_cast<int>(std::lround(econ.cityInfluence * 100.0f));
        std::string pStr = std::to_string(econ.energyMW) + (isEn ? " MW · city " : " MW · град ") + std::to_string(sharePct) + "%";
        unsigned int pSz = ui::fitTextSize(font, pStr, fontsize::Label, fontsize::Caption, plaqueW - 32.0f, true);
        sf::Text& tPwr = ui::pooledText(font, toUtf8(pStr), pSz);
        tPwr.setStyle(sf::Text::Bold);
        tPwr.setFillColor(theme::Energy);
        sf::FloatRect pb = tPwr.getLocalBounds();
        tPwr.setPosition({ plaqueX + 26.0f - pb.position.x, plaqueY + plaqueH / 2.0f - pb.size.y / 2.0f - pb.position.y });
        ui::drawText(window, tPwr, sf::FloatRect(energyPlaque.getPosition(), energyPlaque.getSize()));
    }

    // -------------------------------------------------------------------------
    // Land expansion button (buys the cheapest plot still for sale)
    // -------------------------------------------------------------------------
    sf::FloatRect& landRect = isWest ? p1BuyLandBtn : p2BuyLandBtn;
    landRect = sf::FloatRect({ plaqueX, screenHeight - 86.0f }, { plaqueW, 34.0f });
    bool hoverLand = landRect.contains(mousePos);
    sf::RectangleShape landBtn(landRect.size);
    landBtn.setPosition(landRect.position);
    landBtn.setFillColor(hoverLand ? theme::ButtonHover : theme::playerDark(player));
    landBtn.setOutlineThickness(hoverLand ? 2.0f : 1.5f);
    landBtn.setOutlineColor(hoverLand ? theme::Focus : accent);
    window.draw(landBtn);
    if (fontLoaded) {
        bool isEn = (UI_settings::get().getLanguage() == "en");
        std::string landLabel = isEn ? "+ BUY LAND" : "+ КУПИ ЗЕМЯ";
        unsigned int landSz = ui::fitTextSize(font, landLabel, fontsize::Body, fontsize::Label, landRect.size.x - 16.0f, true);
        sf::Text& tLand = ui::pooledText(font, toUtf8(landLabel), landSz);
        tLand.setStyle(sf::Text::Bold);
        tLand.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = tLand.getLocalBounds();
        tLand.setPosition({ landRect.position.x + (landRect.size.x - tb.size.x) / 2.0f - tb.position.x,
                            landRect.position.y + (landRect.size.y - tb.size.y) / 2.0f - tb.position.y });
        ui::drawText(window, tLand, landRect);
    }
}

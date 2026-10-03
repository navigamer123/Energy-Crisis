#include "../includes/UI_playControls.h"
#include <string>

UI_playControls::UI_playControls()
    : selectedIndex(0),
      activeScheme(0),
      requestStart(false),
      requestBack(false) {
}

void UI_playControls::drawButton(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                sf::FloatRect bounds, const sf::String& text,
                                sf::Color baseColor, sf::Color hoverColor, sf::Color textColor,
                                bool isSelected) {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(isSelected ? hoverColor : baseColor);
    shape.setOutlineThickness(isSelected ? 3.0f : 1.5f);
    shape.setOutlineColor(isSelected ? sf::Color(255, 215, 0) : sf::Color(70, 90, 120));
    window.draw(shape);

    if (fontLoaded) {
        sf::String displayText = isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
        sf::Text label(font, displayText, 18);
        label.setFillColor(isSelected ? sf::Color(255, 240, 150) : textColor);

        sf::FloatRect textBounds = label.getLocalBounds();
        label.setPosition({
            bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
            bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f - textBounds.position.y
        });
        window.draw(label);
    }
}

void UI_playControls::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded) {
    float screenWidth = VIRTUAL_WIDTH;
    float cardWidth = 950.0f;
    float cardHeight = 510.0f;
    float cardX = (screenWidth - cardWidth) / 2.0f;
    float cardY = 170.0f;

    // Card background
    sf::RectangleShape card({ cardWidth, cardHeight });
    card.setPosition({ cardX, cardY });
    card.setFillColor(sf::Color(16, 22, 34, 250));
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(sf::Color(0, 229, 255, 220));
    window.draw(card);

    if (fontLoaded) {
        // Modal title
        sf::Text title(font, toUtf8("ИЗБЕРЕТЕ 2-PLAYER УПРАВЛЕНИЕ / CONTROL SCHEME"), 22);
        title.setFillColor(sf::Color(255, 204, 0));
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ cardX + (cardWidth - tb.size.x) / 2.0f, cardY + 14.0f });
        window.draw(title);

        // Subtitle
        sf::Text sub(font, toUtf8("Изберете схема за Играч 1 (Западен сектор) и Играч 2 (Източен сектор)"), 14);
        sub.setFillColor(sf::Color(140, 185, 225));
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setPosition({ cardX + (cardWidth - sb.size.x) / 2.0f, cardY + 44.0f });
        window.draw(sub);

        // Divider
        sf::RectangleShape div({ cardWidth - 60.0f, 2.0f });
        div.setPosition({ cardX + 30.0f, cardY + 66.0f });
        div.setFillColor(sf::Color(50, 70, 95));
        window.draw(div);
    }

    // 4 Options layout
    float optWidth = cardWidth - 60.0f;
    float optHeight = 80.0f;
    float optX = cardX + 30.0f;
    float optStartY = cardY + 76.0f;
    float optSpacing = 88.0f;

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    struct OptionData {
        const char* title;
        const char* p1Text;
        const char* p2Text;
        const char* badgeText;
    };

    OptionData options[4] = {
        {
            "1. СПОДЕЛЕНА КЛАВИАТУРА (DUAL KEYBOARD)",
            "Играч 1 (Запад): [W][A][S][D] Движение  |  [Q] и [E] Действие",
            "Играч 2 (Изток): [Стрелки] Движение     |  [PgUp] и [PgDn] Действие",
            "KB + KB"
        },
        {
            "2. ИГРАЧ 1 КЛАВИАТУРА + ИГРАЧ 2 МИШКА",
            "Играч 1 (Запад): [W][A][S][D] Движение  |  [Q] и [E] Действие",
            "Играч 2 (Изток): [Мишка] Позиция        |  [Ляв / Десен клик] Действие",
            "KB + MOUSE"
        },
        {
            "3. ИГРАЧ 1 МИШКА + ИГРАЧ 2 КЛАВИАТУРА (ОБРАТНО)",
            "Играч 1 (Запад): [Мишка] Позиция        |  [Ляв / Десен клик] Действие",
            "Играч 2 (Изток): [Стрелки] Движение     |  [PgUp] и [PgDn] Действие",
            "MOUSE + KB"
        },
        {
            "4. ОБЩА МИШКА (ЕДНА МИШКА, ИГРА НА РЕД)",
            "Играч 1 (Запад): [Мишката в западната половина] Движение и Действие",
            "Играч 2 (Изток): [Мишката в източната половина] Движение и Действие",
            "1x MOUSE"
        }
    };

    bool mouseMoved = (std::abs(mousePos.x - lastMousePos.x) > 2.0f || std::abs(mousePos.y - lastMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMousePos = mousePos;
    }

    for (int i = 0; i < 4; i++) {
        float y = optStartY + i * optSpacing;
        sf::FloatRect bounds({ optX, y }, { optWidth, optHeight });

        if (mouseMoved && bounds.contains(mousePos)) {
            selectedIndex = i;
        }

        bool isNavSelected = (selectedIndex == i);
        bool isActive = (activeScheme == i);

        sf::RectangleShape optBox(bounds.size);
        optBox.setPosition(bounds.position);

        if (isActive) {
            optBox.setFillColor(isNavSelected ? sf::Color(35, 70, 85, 240) : sf::Color(25, 52, 68, 230));
            optBox.setOutlineThickness(2.5f);
            optBox.setOutlineColor(sf::Color(0, 255, 180));
        } else {
            optBox.setFillColor(isNavSelected ? sf::Color(35, 45, 65, 220) : sf::Color(22, 28, 42, 200));
            optBox.setOutlineThickness(isNavSelected ? 2.0f : 1.0f);
            optBox.setOutlineColor(isNavSelected ? sf::Color(255, 204, 0) : sf::Color(60, 80, 110));
        }
        window.draw(optBox);

        // Radio indicator
        sf::CircleShape radio(9.0f);
        radio.setPosition({ optX + 16.0f, y + 31.0f });
        radio.setFillColor(isActive ? sf::Color(0, 255, 180) : sf::Color(30, 40, 55));
        radio.setOutlineThickness(1.5f);
        radio.setOutlineColor(isActive ? sf::Color(100, 255, 200) : sf::Color(100, 130, 160));
        window.draw(radio);

        if (fontLoaded) {
            sf::Text tTitle(font, toUtf8(options[i].title), 16);
            tTitle.setFillColor(isActive ? sf::Color(255, 235, 120) : (isNavSelected ? sf::Color::White : sf::Color(210, 225, 240)));
            tTitle.setPosition({ optX + 44.0f, y + 8.0f });
            window.draw(tTitle);

            sf::Text tP1(font, toUtf8(options[i].p1Text), 13);
            tP1.setFillColor(sf::Color(0, 229, 255));
            tP1.setPosition({ optX + 44.0f, y + 34.0f });
            window.draw(tP1);

            sf::Text tP2(font, toUtf8(options[i].p2Text), 13);
            tP2.setFillColor(sf::Color(255, 140, 200));
            tP2.setPosition({ optX + 44.0f, y + 54.0f });
            window.draw(tP2);

            // Status Badge on Right
            float badgeW = 140.0f;
            float badgeH = 28.0f;
            float badgeX = optX + optWidth - badgeW - 14.0f;
            float badgeY = y + 26.0f;

            sf::RectangleShape badge({ badgeW, badgeH });
            badge.setPosition({ badgeX, badgeY });
            badge.setFillColor(isActive ? sf::Color(20, 80, 50) : sf::Color(30, 40, 55));
            badge.setOutlineThickness(1.0f);
            badge.setOutlineColor(isActive ? sf::Color(50, 220, 120) : sf::Color(70, 90, 120));
            window.draw(badge);

            sf::Text bText(font, toUtf8(isActive ? "ИЗБРАНО" : options[i].badgeText), 13);
            bText.setFillColor(isActive ? sf::Color(120, 255, 180) : sf::Color(150, 180, 210));
            sf::FloatRect bb = bText.getLocalBounds();
            bText.setPosition({ badgeX + (badgeW - bb.size.x) / 2.0f, badgeY + 4.0f });
            window.draw(bText);
        }
    }

    // Action buttons at the bottom
    float btnWidth = 260.0f;
    float btnHeight = 44.0f;
    float buttonY = cardY + cardHeight - 56.0f;

    sf::FloatRect startBtn({ cardX + cardWidth / 2.0f - btnWidth - 20.0f, buttonY }, { btnWidth, btnHeight });
    sf::FloatRect backBtn({ cardX + cardWidth / 2.0f + 20.0f, buttonY }, { btnWidth, btnHeight });

    if (mouseMoved) {
        if (startBtn.contains(mousePos)) selectedIndex = 4;
        else if (backBtn.contains(mousePos)) selectedIndex = 5;
    }

    drawButton(window, font, fontLoaded, startBtn, toUtf8("СТАРТ НА ИГРАТА"),
               sf::Color(25, 110, 60), sf::Color(40, 160, 85), sf::Color::White, selectedIndex == 4);
    drawButton(window, font, fontLoaded, backBtn, toUtf8("НАЗАД"),
               sf::Color(40, 50, 70), sf::Color(70, 85, 115), sf::Color::White, selectedIndex == 5);

    // Keyboard hints
    if (fontLoaded) {
        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter / Клик] | Назад: [ESC]"), 13);
        hint.setFillColor(sf::Color(130, 160, 190));
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, cardY + cardHeight + 10.0f });
        window.draw(hint);
    }
}

void UI_playControls::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        bool isUp = (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W);
        bool isDown = (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S);
        bool isLeft = (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A);
        bool isRight = (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D);
        bool isSelect = (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space);

        if (key->code == sf::Keyboard::Key::Escape) {
            requestBack = true;
        } else if (isUp) {
            if (selectedIndex == 0) selectedIndex = 4;
            else if (selectedIndex == 4 || selectedIndex == 5) selectedIndex = 3;
            else selectedIndex--;
        } else if (isDown) {
            if (selectedIndex == 3) selectedIndex = 4;
            else if (selectedIndex == 4 || selectedIndex == 5) selectedIndex = 0;
            else selectedIndex++;
        } else if (isLeft) {
            if (selectedIndex == 5) selectedIndex = 4;
        } else if (isRight) {
            if (selectedIndex == 4) selectedIndex = 5;
        } else if (isSelect) {
            if (selectedIndex >= 0 && selectedIndex <= 3) {
                if (activeScheme == selectedIndex) {
                    requestStart = true;
                } else {
                    activeScheme = selectedIndex;
                }
            } else if (selectedIndex == 4) {
                requestStart = true;
            } else if (selectedIndex == 5) {
                requestBack = true;
            }
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            float screenWidth = VIRTUAL_WIDTH;
            float cardWidth = 950.0f;
            float cardHeight = 510.0f;
            float cardX = (screenWidth - cardWidth) / 2.0f;
            float cardY = 170.0f;
            float optWidth = cardWidth - 60.0f;
            float optHeight = 80.0f;
            float optX = cardX + 30.0f;
            float optStartY = cardY + 76.0f;
            float optSpacing = 88.0f;

            for (int i = 0; i < 4; i++) {
                float y = optStartY + i * optSpacing;
                if (sf::FloatRect({ optX, y }, { optWidth, optHeight }).contains(clickPos)) {
                    activeScheme = i;
                    selectedIndex = i;
                    break;
                }
            }

            float btnWidth = 260.0f;
            float btnHeight = 44.0f;
            float buttonY = cardY + cardHeight - 56.0f;
            sf::FloatRect startBtn({ cardX + cardWidth / 2.0f - btnWidth - 20.0f, buttonY }, { btnWidth, btnHeight });
            sf::FloatRect backBtn({ cardX + cardWidth / 2.0f + 20.0f, buttonY }, { btnWidth, btnHeight });

            if (startBtn.contains(clickPos)) {
                requestStart = true;
            } else if (backBtn.contains(clickPos)) {
                requestBack = true;
            }
        }
    }
}

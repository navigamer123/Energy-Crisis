#include "../includes/UI_playControls.h"
#include "../includes/UI_controlsConfig.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <string>
#include <cmath>

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
    shape.setOutlineColor(isSelected ? theme::Focus : theme::Line);
    window.draw(shape);

    if (fontLoaded) {
        sf::String displayText = isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
        sf::Text& label = ui::pooledText(font, displayText, fontsize::H2);
        label.setFillColor(isSelected ? theme::TextPrimary : textColor);
        if (isSelected) label.setStyle(sf::Text::Bold);

        sf::FloatRect textBounds = label.getLocalBounds();
        label.setPosition({
            bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
            bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f - textBounds.position.y
        });
        ui::drawText(window, label, bounds);
    }
}

void UI_playControls::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded) {
    float screenWidth = VIRTUAL_WIDTH;
    float cardWidth = 980.0f;
    float cardHeight = 630.0f;
    float cardX = (screenWidth - cardWidth) / 2.0f;
    float cardY = 120.0f;

    // Card background
    sf::RectangleShape card({ cardWidth, cardHeight });
    card.setPosition({ cardX, cardY });
    card.setFillColor(theme::withAlpha(theme::Panel, 250));
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(theme::LineStrong);
    window.draw(card);
    const sf::FloatRect cardRect({ cardX, cardY }, { cardWidth, cardHeight });

    if (fontLoaded) {
        // Modal title
        sf::Text& title = ui::pooledText(font, toUtf8("ИЗБЕРЕТЕ УПРАВЛЕНИЕ ЗА ДВАМА ИГРАЧИ"), fontsize::H1);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ cardX + (cardWidth - tb.size.x) / 2.0f, cardY + 12.0f });
        ui::drawText(window, title, cardRect);

        // Subtitle + Hardware status
        std::string statusText = UI_controlsConfig::get().getJoystickStatusBg();
        sf::Text& sub = ui::pooledText(font, toUtf8(statusText), fontsize::Body);
        sub.setFillColor(UI_controlsConfig::get().isAnyJoystickConnected() ? theme::Good : theme::TextSecondary);
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setPosition({ cardX + (cardWidth - sb.size.x) / 2.0f, cardY + 44.0f });
        ui::drawText(window, sub, cardRect);

        // Divider
        sf::RectangleShape div({ cardWidth - 60.0f, 2.0f });
        div.setPosition({ cardX + 30.0f, cardY + 68.0f });
        div.setFillColor(theme::Line);
        window.draw(div);
    }

    // 5 Options layout
    float optWidth = cardWidth - 60.0f;
    float optHeight = 72.0f;
    float optX = cardX + 30.0f;
    float optStartY = cardY + 76.0f;
    float optSpacing = 78.0f;

    sf::Vector2f mousePos = ui::pointerPos(window);

    struct OptionData {
        const char* title;
        const char* p1Text;
        const char* p2Text;
        const char* badgeText;
    };

    OptionData options[5] = {
        {
            "1. СПОДЕЛЕНА КЛАВИАТУРА",
            "Играч 1 (Запад): [W][A][S][D] Движение  ·  [SPACE] Действие  ·  [E] / [Q] Сграда",
            "Играч 2 (Изток): [Стрелки] Движение  ·  [ENTER] Действие  ·  [PgDn] / [PgUp] Сграда",
            "КЛАВ. + КЛАВ."
        },
        {
            "2. ИГРАЧ 1 КЛАВИАТУРА + ИГРАЧ 2 МИШКА",
            "Играч 1 (Запад): [W][A][S][D] Движение  ·  [SPACE] Действие  ·  [E] / [Q] Сграда",
            "Играч 2 (Изток): [Мишка] Позиция  ·  [Ляв клик] Действие  ·  [Десен клик] Отказ",
            "КЛАВ. + МИШКА"
        },
        {
            "3. ИГРАЧ 1 МИШКА + ИГРАЧ 2 КЛАВИАТУРА",
            "Играч 1 (Запад): [Мишка] Позиция  ·  [Ляв клик] Действие  ·  [Десен клик] Отказ",
            "Играч 2 (Изток): [Стрелки] Движение  ·  [ENTER] Действие  ·  [PgDn] / [PgUp] Сграда",
            "МИШКА + КЛАВ."
        },
        {
            "4. ОБЩА МИШКА (ЕДНА МИШКА, ИГРА НА РЕД)",
            "Играч 1 (Запад): [Мишката в западната половина] Движение и Действие",
            "Играч 2 (Изток): [Мишката в източната половина] Движение и Действие",
            "1 МИШКА"
        },
        {
            "5. DEVHUB ONE АРКАДНА КОНЗОЛА (АРКАДНИ СТИКОВЕ & БУТОНИ)",
            "Играч 1 (Запад): Аркаден стик 1  ·  [Бутон 1/A] Действие  ·  [2/B] Отказ  ·  [3/X] Надграждане",
            "Играч 2 (Изток): Аркаден стик 2  ·  [Бутон 1/A] Действие  ·  [2/B] Отказ  ·  [3/X] Надграждане",
            "DEVHUB ONE"
        }
    };

    bool mouseMoved = (std::abs(mousePos.x - lastMousePos.x) > 2.0f || std::abs(mousePos.y - lastMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMousePos = mousePos;
    }

    for (int i = 0; i < 5; i++) {
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
            optBox.setFillColor(isNavSelected ? theme::withAlpha(theme::CardSelected, 240) : theme::withAlpha(theme::CardHover, 230));
            optBox.setOutlineThickness(2.5f);
            optBox.setOutlineColor(isNavSelected ? theme::Focus : theme::Good);
        } else {
            optBox.setFillColor(isNavSelected ? theme::withAlpha(theme::CardHover, 220) : theme::withAlpha(theme::Card, 200));
            optBox.setOutlineThickness(isNavSelected ? 2.0f : 1.0f);
            optBox.setOutlineColor(isNavSelected ? theme::Focus : theme::Line);
        }
        window.draw(optBox);
        ui::lint::ContainerScope optScope(bounds);

        // Radio indicator
        sf::CircleShape radio(8.0f);
        radio.setPosition({ optX + 16.0f, y + 28.0f });
        radio.setFillColor(isActive ? theme::Good : theme::Well);
        radio.setOutlineThickness(1.5f);
        radio.setOutlineColor(isActive ? theme::TextPrimary : theme::LineStrong);
        window.draw(radio);

        if (fontLoaded) {
            sf::Text& tTitle = ui::pooledText(font, toUtf8(options[i].title), fontsize::H2);
            tTitle.setStyle(sf::Text::Bold);
            if (i == 4) {
                tTitle.setFillColor(isActive ? theme::Gold : (isNavSelected ? theme::TextPrimary : theme::TextSecondary));
            } else {
                tTitle.setFillColor((isActive || isNavSelected) ? theme::TextPrimary : theme::TextSecondary);
            }
            tTitle.setPosition({ optX + 42.0f, y + 6.0f });
            ui::drawText(window, tTitle);

            sf::Text& tP1 = ui::pooledText(font, toUtf8(options[i].p1Text), fontsize::Label);
            tP1.setFillColor(theme::P1Light);
            tP1.setPosition({ optX + 42.0f, y + 30.0f });
            ui::drawText(window, tP1);

            sf::Text& tP2 = ui::pooledText(font, toUtf8(options[i].p2Text), fontsize::Label);
            tP2.setFillColor(theme::P2Light);
            tP2.setPosition({ optX + 42.0f, y + 49.0f });
            ui::drawText(window, tP2);

            // Status Badge on Right
            float badgeW = 140.0f;
            float badgeH = 26.0f;
            float badgeX = optX + optWidth - badgeW - 14.0f;
            float badgeY = y + 23.0f;

            sf::RectangleShape badge({ badgeW, badgeH });
            badge.setPosition({ badgeX, badgeY });
            badge.setFillColor(isActive ? theme::GoodFill : (i == 4 ? theme::withAlpha(theme::Gold, 40) : theme::Well));
            badge.setOutlineThickness(1.0f);
            badge.setOutlineColor(isActive ? theme::Good : (i == 4 ? theme::Gold : theme::Line));
            window.draw(badge);

            sf::Text& bText = ui::pooledText(font, toUtf8(isActive ? "ИЗБРАНО" : options[i].badgeText), fontsize::Label);
            bText.setFillColor(isActive ? theme::TextPrimary : (i == 4 ? theme::Gold : theme::TextSecondary));
            sf::FloatRect bb = bText.getLocalBounds();
            bText.setPosition({ badgeX + (badgeW - bb.size.x) / 2.0f, badgeY + 4.0f });
            ui::drawText(window, bText, sf::FloatRect({ badgeX, badgeY }, { badgeW, badgeH }));
        }
    }

    // Action buttons at the bottom
    float btnWidth = 260.0f;
    float btnHeight = 44.0f;
    float buttonY = cardY + cardHeight - 56.0f;

    sf::FloatRect startBtn({ cardX + cardWidth / 2.0f - btnWidth - 20.0f, buttonY }, { btnWidth, btnHeight });
    sf::FloatRect backBtn({ cardX + cardWidth / 2.0f + 20.0f, buttonY }, { btnWidth, btnHeight });

    if (mouseMoved) {
        if (startBtn.contains(mousePos)) selectedIndex = 5;
        else if (backBtn.contains(mousePos)) selectedIndex = 6;
    }

    drawButton(window, font, fontLoaded, startBtn, toUtf8("СТАРТ НА ИГРАТА"),
               theme::GoodFill, theme::GoodFill, theme::TextPrimary, selectedIndex == 5);
    drawButton(window, font, fontLoaded, backBtn, toUtf8("НАЗАД"),
               theme::Button, theme::ButtonHover, theme::TextPrimary, selectedIndex == 6);

    // Keyboard hints
    if (fontLoaded) {
        sf::Text& hint = ui::pooledText(font, toUtf8("Навигация: [Стрелки / Стик / W,S] | Избор: [Enter / Бутон 1 / Start] | Назад: [ESC / Бутон 2]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, cardY + cardHeight + 8.0f });
        ui::drawText(window, hint);
    }
}

void UI_playControls::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    bool isUp = false;
    bool isDown = false;
    bool isLeft = false;
    bool isRight = false;
    bool isSelect = false;
    bool isEscape = false;

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        isUp = (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W);
        isDown = (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S);
        isLeft = (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A);
        isRight = (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D);
        isSelect = (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space);
        isEscape = (key->code == sf::Keyboard::Key::Escape);
    }

    if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
        if (jb->button == 0 || jb->button == 7) isSelect = true; // Button 1 (A) or Start
        if (jb->button == 1 || jb->button == 6) isEscape = true; // Button 2 (B) or Back / Coin
    }

    if (const auto* jm = event.getIf<sf::Event::JoystickMoved>()) {
        if (jm->axis == sf::Joystick::Axis::Y || jm->axis == sf::Joystick::Axis::PovY) {
            if (jm->position < -55.0f) isUp = true;
            else if (jm->position > 55.0f) isDown = true;
        } else if (jm->axis == sf::Joystick::Axis::X || jm->axis == sf::Joystick::Axis::PovX) {
            if (jm->position < -55.0f) isLeft = true;
            else if (jm->position > 55.0f) isRight = true;
        }
    }

    if (isEscape) {
        requestBack = true;
    } else if (isUp) {
        if (selectedIndex == 0) selectedIndex = 5;
        else if (selectedIndex == 5 || selectedIndex == 6) selectedIndex = 4;
        else selectedIndex--;
    } else if (isDown) {
        if (selectedIndex == 4) selectedIndex = 5;
        else if (selectedIndex == 5 || selectedIndex == 6) selectedIndex = 0;
        else selectedIndex++;
    } else if (isLeft) {
        if (selectedIndex == 6) selectedIndex = 5;
    } else if (isRight) {
        if (selectedIndex == 5) selectedIndex = 6;
    } else if (isSelect) {
        if (selectedIndex >= 0 && selectedIndex <= 4) {
            if (activeScheme == selectedIndex) {
                requestStart = true;
            } else {
                activeScheme = selectedIndex;
            }
        } else if (selectedIndex == 5) {
            requestStart = true;
        } else if (selectedIndex == 6) {
            requestBack = true;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            float screenWidth = VIRTUAL_WIDTH;
            float cardWidth = 980.0f;
            float cardHeight = 630.0f;
            float cardX = (screenWidth - cardWidth) / 2.0f;
            float cardY = 120.0f;
            float optWidth = cardWidth - 60.0f;
            float optHeight = 72.0f;
            float optX = cardX + 30.0f;
            float optStartY = cardY + 76.0f;
            float optSpacing = 78.0f;

            for (int i = 0; i < 5; i++) {
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

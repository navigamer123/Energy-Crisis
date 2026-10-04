#include "../includes/UI_playControls.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_input.h" // Team b-session (F-10): scheme descriptions from the real bindings
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
    shape.setOutlineColor(isSelected ? theme::Focus : theme::Line);
    window.draw(shape);

    if (fontLoaded) {
        sf::String displayText = isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
        sf::Text label(font, displayText, fontsize::H2);
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
    float cardWidth = 950.0f;
    float cardHeight = 510.0f;
    float cardX = (screenWidth - cardWidth) / 2.0f;
    float cardY = 170.0f;

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
        sf::Text title(font, toUtf8("ИЗБЕРЕТЕ УПРАВЛЕНИЕ ЗА ДВАМА ИГРАЧИ"), fontsize::H1);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ cardX + (cardWidth - tb.size.x) / 2.0f, cardY + 14.0f });
        ui::drawText(window, title, cardRect);

        // Subtitle
        sf::Text sub(font, toUtf8("Изберете схема за Играч 1 (Западен сектор) и Играч 2 (Източен сектор)"), fontsize::Body);
        sub.setFillColor(theme::TextSecondary);
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setPosition({ cardX + (cardWidth - sb.size.x) / 2.0f, cardY + 48.0f });
        ui::drawText(window, sub, cardRect);

        // Divider
        sf::RectangleShape div({ cardWidth - 60.0f, 2.0f });
        div.setPosition({ cardX + 30.0f, cardY + 70.0f });
        div.setFillColor(theme::Line);
        window.draw(div);
    }

    // 4 Options layout
    float optWidth = cardWidth - 60.0f;
    float optHeight = 80.0f;
    float optX = cardX + 30.0f;
    float optStartY = cardY + 76.0f;
    float optSpacing = 88.0f;

    sf::Vector2f mousePos = ui::pointerPos(window);

    struct OptionData {
        const char* title;
        std::string p1Text;
        std::string p2Text;
        const char* badgeText;
    };

    // b-session (F-10): keyboard lines are generated from the current key bindings
    inputRouter().setMatchContext(false, ControlScheme::BOTH_KEYBOARD);
    auto keysLine = [](int p) {
        const InputRouter& r = inputRouter();
        return r.moveHint(p) + " Движение  ·  " + r.hint(p, InputAction::Action, false, 1) + " Действие  ·  " +
               r.hint(p, InputAction::NextBuilding, false, 1) + " / " + r.hint(p, InputAction::PrevBuilding, false, 1) + " Сграда";
    };
    const std::string p1Keys = "Играч 1 (Запад): " + keysLine(1);
    const std::string p2Keys = "Играч 2 (Изток): " + keysLine(2);
    OptionData options[4] = {
        {
            "1. СПОДЕЛЕНА КЛАВИАТУРА",
            p1Keys,
            p2Keys,
            "КЛАВ. + КЛАВ."
        },
        {
            "2. ИГРАЧ 1 КЛАВИАТУРА + ИГРАЧ 2 МИШКА",
            p1Keys,
            "Играч 2 (Изток): [Мишка] Позиция  ·  [Ляв клик] Действие  ·  [Десен клик] Отказ",
            "КЛАВ. + МИШКА"
        },
        {
            "3. ИГРАЧ 1 МИШКА + ИГРАЧ 2 КЛАВИАТУРА",
            "Играч 1 (Запад): [Мишка] Позиция  ·  [Ляв клик] Действие  ·  [Десен клик] Отказ",
            p2Keys,
            "МИШКА + КЛАВ."
        },
        {
            "4. ОБЩА МИШКА (ЕДНА МИШКА, ИГРА НА РЕД)",
            "Играч 1 (Запад): [Мишката в западната половина] Движение и Действие",
            "Играч 2 (Изток): [Мишката в източната половина] Движение и Действие",
            "1 МИШКА"
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
        sf::CircleShape radio(9.0f);
        radio.setPosition({ optX + 16.0f, y + 31.0f });
        radio.setFillColor(isActive ? theme::Good : theme::Well);
        radio.setOutlineThickness(1.5f);
        radio.setOutlineColor(isActive ? theme::TextPrimary : theme::LineStrong);
        window.draw(radio);

        if (fontLoaded) {
            sf::Text tTitle(font, toUtf8(options[i].title), fontsize::H2);
            tTitle.setStyle(sf::Text::Bold);
            tTitle.setFillColor((isActive || isNavSelected) ? theme::TextPrimary : theme::TextSecondary);
            tTitle.setPosition({ optX + 44.0f, y + 8.0f });
            ui::drawText(window, tTitle);

            sf::Text tP1(font, toUtf8(options[i].p1Text), fontsize::Label);
            tP1.setFillColor(theme::P1Light);
            tP1.setPosition({ optX + 44.0f, y + 34.0f });
            ui::drawText(window, tP1);

            sf::Text tP2(font, toUtf8(options[i].p2Text), fontsize::Label);
            tP2.setFillColor(theme::P2Light);
            tP2.setPosition({ optX + 44.0f, y + 54.0f });
            ui::drawText(window, tP2);

            // Status Badge on Right
            float badgeW = 140.0f;
            float badgeH = 28.0f;
            float badgeX = optX + optWidth - badgeW - 14.0f;
            float badgeY = y + 26.0f;

            sf::RectangleShape badge({ badgeW, badgeH });
            badge.setPosition({ badgeX, badgeY });
            badge.setFillColor(isActive ? theme::GoodFill : theme::Well);
            badge.setOutlineThickness(1.0f);
            badge.setOutlineColor(isActive ? theme::Good : theme::Line);
            window.draw(badge);

            sf::Text bText(font, toUtf8(isActive ? "ИЗБРАНО" : options[i].badgeText), fontsize::Label);
            bText.setFillColor(isActive ? theme::TextPrimary : theme::TextSecondary);
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
        if (startBtn.contains(mousePos)) selectedIndex = 4;
        else if (backBtn.contains(mousePos)) selectedIndex = 5;
    }

    drawButton(window, font, fontLoaded, startBtn, toUtf8("СТАРТ НА ИГРАТА"),
               theme::GoodFill, theme::GoodFill, theme::TextPrimary, selectedIndex == 4);
    drawButton(window, font, fontLoaded, backBtn, toUtf8("НАЗАД"),
               theme::Button, theme::ButtonHover, theme::TextPrimary, selectedIndex == 5);

    // Keyboard hints
    if (fontLoaded) {
        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter / Клик] | Назад: [ESC]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, cardY + cardHeight + 10.0f });
        ui::drawText(window, hint);
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

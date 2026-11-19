#include "../includes/UI_playControls.h"
#include "../includes/UI_controlsConfig.h"
#include "../includes/UI_settings.h"
#include "../includes/UI_main.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_text.h"
#include "../includes/UI_theme.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr float CARD_WIDTH = 1040.0f;
constexpr float CARD_HEIGHT = 606.0f;
constexpr float CARD_X = (VIRTUAL_WIDTH - CARD_WIDTH) / 2.0f;
constexpr float CARD_Y = 176.0f;
constexpr float OPT_WIDTH = CARD_WIDTH - 56.0f;
constexpr float OPT_HEIGHT = 80.0f;
constexpr float OPT_X = CARD_X + 28.0f;
constexpr float OPT_START_Y = CARD_Y + 86.0f;
constexpr float OPT_SPACING = 88.0f;
constexpr float BTN_WIDTH = 310.0f;
constexpr float BTN_HEIGHT = 54.0f;
constexpr float BTN_Y = CARD_Y + CARD_HEIGHT - BTN_HEIGHT - 18.0f;

std::string toStdUtf8(const sf::String &s) {
  auto u8 = s.toUtf8();
  return std::string(u8.begin(), u8.end());
}
} // namespace

UI_playControls::UI_playControls()
    : selectedIndex(0), activeScheme(0), requestStart(false),
      requestBack(false), requestQuit(false) {}

sf::FloatRect UI_playControls::cardRect() {
  return sf::FloatRect({CARD_X, CARD_Y}, {CARD_WIDTH, CARD_HEIGHT});
}

sf::FloatRect UI_playControls::optionRect(int index) {
  return sf::FloatRect({OPT_X, OPT_START_Y + index * OPT_SPACING},
                       {OPT_WIDTH, OPT_HEIGHT});
}

sf::FloatRect UI_playControls::startButtonRect() {
  return sf::FloatRect({CARD_X + CARD_WIDTH / 2.0f - BTN_WIDTH - 20.0f, BTN_Y},
                       {BTN_WIDTH, BTN_HEIGHT});
}

sf::FloatRect UI_playControls::backButtonRect() {
  return sf::FloatRect({CARD_X + CARD_WIDTH / 2.0f + 20.0f, BTN_Y},
                       {BTN_WIDTH, BTN_HEIGHT});
}

bool UI_playControls::isPointInside(sf::FloatRect bounds, sf::Vector2f pt) {
#if defined(__ANDROID__)
  sf::FloatRect padded({bounds.position.x - 12.0f, bounds.position.y - 12.0f},
                       {bounds.size.x + 24.0f, bounds.size.y + 24.0f});
  return padded.contains(pt);
#else
  return bounds.contains(pt);
#endif
}

void UI_playControls::drawButton(sf::RenderTarget &window, const sf::Font &font,
                                 bool fontLoaded, sf::FloatRect bounds,
                                 const sf::String &text, sf::Color baseColor,
                                 sf::Color hoverColor, sf::Color textColor,
                                 bool isSelected) {
  sf::RectangleShape shadow(bounds.size);
  shadow.setPosition({bounds.position.x + 2.0f, bounds.position.y + 3.0f});
  shadow.setFillColor(sf::Color(0, 0, 0, 110));
  window.draw(shadow);

  sf::RectangleShape shape(bounds.size);
  shape.setPosition(bounds.position);
  shape.setFillColor(isSelected ? hoverColor : baseColor);
  shape.setOutlineThickness(isSelected ? 3.5f : 2.0f);
  shape.setOutlineColor(isSelected ? theme::Focus : theme::LineStrong);
  window.draw(shape);

  if (fontLoaded) {
    sf::String displayText =
        isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
    float maxTextW = std::max(16.0f, bounds.size.x - 28.0f);
    unsigned int textSize =
        ui::fitTextSize(font, toStdUtf8(displayText), 20, 14, maxTextW, true);
    sf::Text &label = ui::pooledText(font, displayText, textSize);
    label.setStyle(sf::Text::Bold);
    label.setFillColor(isSelected ? theme::TextPrimary : textColor);

    sf::FloatRect textBounds = label.getLocalBounds();
    label.setPosition(
        {bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f -
             textBounds.position.x,
         bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f -
             textBounds.position.y});
    ui::drawText(window, label, bounds);
  }
}

void UI_playControls::draw(sf::RenderTarget &window, const sf::Font &font,
                           bool fontLoaded) {
  const sf::FloatRect panel = cardRect();
  const float cardX = panel.position.x;
  const float cardY = panel.position.y;
  const float cardWidth = panel.size.x;
  const float cardHeight = panel.size.y;

  // Card background
  sf::RectangleShape card(panel.size);
  card.setPosition(panel.position);
  card.setFillColor(theme::withAlpha(theme::Panel, 250));
  card.setOutlineThickness(2.0f);
  card.setOutlineColor(theme::LineStrong);
  window.draw(card);
  ui::lint::occlude(panel);

  bool isEn = (UI_settings::get().getLanguage() == "en");

  if (fontLoaded) {
    std::string titleStr = isEn ? "CHOOSE A 2-PLAYER CONTROL SCHEME"
                                : "ИЗБЕРЕТЕ УПРАВЛЕНИЕ ЗА ДВАМА ИГРАЧИ";
    unsigned int titleSize =
        ui::fitTextSize(font, titleStr, fontsize::H1, 18, cardWidth - 60.0f, true);
    sf::Text &title = ui::pooledText(font, toUtf8(titleStr), titleSize);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(theme::TextPrimary);
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition({cardX + (cardWidth - tb.size.x) / 2.0f - tb.position.x,
                       cardY + 14.0f});
    ui::drawText(window, title, panel);

    // Subtitle + Hardware status
    std::string statusText = UI_controlsConfig::get().getJoystickStatusBg();
    unsigned int subSize =
        ui::fitTextSize(font, statusText, fontsize::Body, 12, cardWidth - 60.0f, false);
    sf::Text &sub = ui::pooledText(font, toUtf8(statusText), subSize);
    sub.setFillColor(UI_controlsConfig::get().isAnyJoystickConnected()
                         ? theme::Good
                         : theme::TextSecondary);
    sf::FloatRect sb = sub.getLocalBounds();
    sub.setPosition({cardX + (cardWidth - sb.size.x) / 2.0f - sb.position.x,
                     cardY + 48.0f});
    ui::drawText(window, sub, panel);

    // Divider
    sf::RectangleShape div({cardWidth - 56.0f, 2.0f});
    div.setPosition({cardX + 28.0f, cardY + 74.0f});
    div.setFillColor(theme::Line);
    window.draw(div);
  }

  sf::Vector2f mousePos = ui::pointerPos(window);

  struct OptionData {
    const char *title;
    const char *p1Text;
    const char *p2Text;
    const char *badgeText;
  };

  static const OptionData optionsBg[5] = {
      {"1. СПОДЕЛЕНА КЛАВИАТУРА",
       "Играч 1 (Запад): [W][A][S][D] Движение  ·  [SPACE] Действие  ·  [E] / [Q] Сграда",
       "Играч 2 (Изток): [Стрелки] Движение  ·  [ENTER] Действие  ·  [PgDn] / [PgUp] Сграда",
       "КЛАВ. + КЛАВ."},
      {"2. ИГРАЧ 1 КЛАВИАТУРА + ИГРАЧ 2 МИШКА",
       "Играч 1 (Запад): [W][A][S][D] Движение  ·  [SPACE] Действие  ·  [E] / [Q] Сграда",
       "Играч 2 (Изток): [Мишка] Позиция  ·  [Ляв клик] Действие  ·  [Десен клик] Отказ",
       "КЛАВ. + МИШКА"},
      {"3. ИГРАЧ 1 МИШКА + ИГРАЧ 2 КЛАВИАТУРА",
       "Играч 1 (Запад): [Мишка] Позиция  ·  [Ляв клик] Действие  ·  [Десен клик] Отказ",
       "Играч 2 (Изток): [Стрелки] Движение  ·  [ENTER] Действие  ·  [PgDn] / [PgUp] Сграда",
       "МИШКА + КЛАВ."},
      {"4. ОБЩА МИШКА (ЕДНА МИШКА, ИГРА НА РЕД)",
       "Играч 1 (Запад): [Мишката в западната половина] Движение и Действие",
       "Играч 2 (Изток): [Мишката в източната половина] Движение и Действие",
       "1 МИШКА"},
      {"5. DEVHUB ONE АРКАДНА КОНЗОЛА (АРКАДНИ СТИКОВЕ & БУТОНИ)",
       "Играч 1 (Запад): Аркаден стик 1  ·  [Бутон 1/A] Действие  ·  [2/B] Отказ  ·  [3/X] Надграждане",
       "Играч 2 (Изток): Аркаден стик 2  ·  [Бутон 1/A] Действие  ·  [2/B] Отказ  ·  [3/X] Надграждане",
       "DEVHUB ONE"}};

  static const OptionData optionsEn[5] = {
      {"1. SHARED KEYBOARD",
       "Player 1 (West): [W][A][S][D] Move  ·  [SPACE] Action  ·  [E] / [Q] Building",
       "Player 2 (East): [Arrows] Move  ·  [ENTER] Action  ·  [PgDn] / [PgUp] Building",
       "KB + KB"},
      {"2. PLAYER 1 KEYBOARD + PLAYER 2 MOUSE",
       "Player 1 (West): [W][A][S][D] Move  ·  [SPACE] Action  ·  [E] / [Q] Building",
       "Player 2 (East): [Mouse] Position  ·  [Left Click] Action  ·  [Right Click] Cancel",
       "KB + MOUSE"},
      {"3. PLAYER 1 MOUSE + PLAYER 2 KEYBOARD",
       "Player 1 (West): [Mouse] Position  ·  [Left Click] Action  ·  [Right Click] Cancel",
       "Player 2 (East): [Arrows] Move  ·  [ENTER] Action  ·  [PgDn] / [PgUp] Building",
       "MOUSE + KB"},
      {"4. SHARED MOUSE (ONE MOUSE, TAKE TURNS)",
       "Player 1 (West): [Mouse in west half] Move & Action",
       "Player 2 (East): [Mouse in east half] Move & Action",
       "1x MOUSE"},
      {"5. DEVHUB ONE ARCADE (ARCADE STICKS & BUTTONS)",
       "Player 1 (West): Arcade Stick 1  ·  [Button 1/A] Action  ·  [2/B] Cancel  ·  [3/X] Upgrade",
       "Player 2 (East): Arcade Stick 2  ·  [Button 1/A] Action  ·  [2/B] Cancel  ·  [3/X] Upgrade",
       "DEVHUB ONE"}};

  const OptionData *options = isEn ? optionsEn : optionsBg;

  bool mouseMoved = (std::abs(mousePos.x - lastMousePos.x) > 2.0f ||
                     std::abs(mousePos.y - lastMousePos.y) > 2.0f);
  if (mouseMoved) {
    lastMousePos = mousePos;
  }

  for (int i = 0; i < 5; i++) {
    sf::FloatRect bounds = optionRect(i);
    float x = bounds.position.x;
    float y = bounds.position.y;

    if (mouseMoved && bounds.contains(mousePos)) {
      selectedIndex = i;
    }

    bool isNavSelected = (selectedIndex == i);
    bool isActive = (activeScheme == i);

    sf::RectangleShape optBox(bounds.size);
    optBox.setPosition(bounds.position);

    if (isActive) {
      optBox.setFillColor(isNavSelected
                              ? theme::withAlpha(theme::CardSelected, 245)
                              : theme::withAlpha(theme::CardHover, 235));
      optBox.setOutlineThickness(2.5f);
      optBox.setOutlineColor(isNavSelected ? theme::Focus : theme::Good);
    } else {
      optBox.setFillColor(isNavSelected
                              ? theme::withAlpha(theme::CardHover, 230)
                              : theme::withAlpha(theme::Card, 225));
      optBox.setOutlineThickness(isNavSelected ? 2.5f : 1.5f);
      optBox.setOutlineColor(isNavSelected ? theme::Focus : theme::LineStrong);
    }
    window.draw(optBox);
    ui::lint::ContainerScope optScope(bounds);

    // Radio indicator
    sf::CircleShape radio(9.0f);
    radio.setPosition({x + 18.0f, y + (bounds.size.y - 18.0f) / 2.0f});
    radio.setFillColor(isActive ? theme::Good : theme::Well);
    radio.setOutlineThickness(2.0f);
    radio.setOutlineColor(isActive ? theme::TextPrimary : theme::LineStrong);
    window.draw(radio);

    if (fontLoaded) {
      float badgeW = 156.0f;
      float badgeH = 30.0f;
      float badgeX = x + bounds.size.x - badgeW - 16.0f;
      float badgeY = y + (bounds.size.y - badgeH) / 2.0f;
      sf::FloatRect badgeRect({badgeX, badgeY}, {badgeW, badgeH});

      float textX = x + 48.0f;
      float maxTextW = badgeX - textX - 16.0f;

      unsigned int titleSz =
          ui::fitTextSize(font, options[i].title, 17, 14, maxTextW, true);
      sf::Text &tTitle = ui::pooledText(font, toUtf8(options[i].title), titleSz);
      tTitle.setStyle(sf::Text::Bold);
      if (i == 4) {
        tTitle.setFillColor(isActive ? theme::Gold
                                     : (isNavSelected ? theme::TextPrimary
                                                      : theme::TextSecondary));
      } else {
        tTitle.setFillColor((isActive || isNavSelected) ? theme::TextPrimary
                                                        : theme::TextSecondary);
      }
      tTitle.setPosition({textX, y + 8.0f});
      ui::drawText(window, tTitle);

      unsigned int p1Sz =
          ui::fitTextSize(font, options[i].p1Text, 13, 11, maxTextW, false);
      sf::Text &tP1 = ui::pooledText(font, toUtf8(options[i].p1Text), p1Sz);
      tP1.setFillColor(theme::P1Light);
      tP1.setPosition({textX, y + 32.0f});
      ui::drawText(window, tP1);

      unsigned int p2Sz =
          ui::fitTextSize(font, options[i].p2Text, 13, 11, maxTextW, false);
      sf::Text &tP2 = ui::pooledText(font, toUtf8(options[i].p2Text), p2Sz);
      tP2.setFillColor(theme::P2Light);
      tP2.setPosition({textX, y + 53.0f});
      ui::drawText(window, tP2);

      // Status Badge on Right
      sf::RectangleShape badge(badgeRect.size);
      badge.setPosition(badgeRect.position);
      badge.setFillColor(isActive ? theme::GoodFill
                                  : (i == 4 ? theme::withAlpha(theme::Gold, 45)
                                            : theme::Well));
      badge.setOutlineThickness(1.5f);
      badge.setOutlineColor(isActive ? theme::Good
                                     : (i == 4 ? theme::Gold : theme::LineStrong));
      window.draw(badge);

      std::string badgeLabel =
          isActive ? (isEn ? "SELECTED" : "ИЗБРАНО") : options[i].badgeText;
      unsigned int badgeSz =
          ui::fitTextSize(font, badgeLabel, 13, 11, badgeW - 12.0f, true);
      sf::Text &bText = ui::pooledText(font, toUtf8(badgeLabel), badgeSz);
      bText.setStyle(sf::Text::Bold);
      bText.setFillColor(isActive
                             ? theme::TextPrimary
                             : (i == 4 ? theme::Gold : theme::TextSecondary));
      sf::FloatRect bb = bText.getLocalBounds();
      bText.setPosition({badgeX + (badgeW - bb.size.x) / 2.0f - bb.position.x,
                         badgeY + (badgeH - bb.size.y) / 2.0f - bb.position.y});
      ui::drawText(window, bText, badgeRect);
    }
  }

  // Action buttons at the bottom
  sf::FloatRect startBtn = startButtonRect();
  sf::FloatRect backBtn = backButtonRect();

  if (mouseMoved) {
    if (startBtn.contains(mousePos))
      selectedIndex = 5;
    else if (backBtn.contains(mousePos))
      selectedIndex = 6;
  }

  drawButton(window, font, fontLoaded, startBtn,
             toUtf8(isEn ? "START GAME" : "СТАРТ НА ИГРАТА"), theme::GoodFill,
             sf::Color(40, 148, 86), theme::TextPrimary, selectedIndex == 5);
  drawButton(window, font, fontLoaded, backBtn,
             toUtf8(isEn ? "BACK" : "НАЗАД"), theme::Button,
             theme::ButtonHover, theme::TextPrimary, selectedIndex == 6);

  // Keyboard hints
  if (fontLoaded) {
    std::string hintStr =
        isEn ? "Navigate: [Arrows / Stick / W,S] | Select: [Enter / Button 1 / Start] | Back: [ESC / Button 2]"
             : "Навигация: [Стрелки / Стик / W,S] | Избор: [Enter / Бутон 1 / Start] | Назад: [ESC / Бутон 2]";
    sf::Text &hint = ui::pooledText(font, toUtf8(hintStr), fontsize::Label);
    hint.setFillColor(theme::TextMuted);
    sf::FloatRect hb = hint.getLocalBounds();
    hint.setPosition({(VIRTUAL_WIDTH - hb.size.x) / 2.0f - hb.position.x,
                      cardY + cardHeight + 14.0f});
    ui::drawText(window, hint);
  }
}

void UI_playControls::handleEvent(const sf::Event &event,
                                  const sf::RenderWindow &window) {
  bool isUp = false;
  bool isDown = false;
  bool isLeft = false;
  bool isRight = false;
  bool isSelect = false;
  bool isEscape = false;
  bool changeGame = false;
  if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
    isUp = (key->code == sf::Keyboard::Key::Up ||
            key->code == sf::Keyboard::Key::W);
    isDown = (key->code == sf::Keyboard::Key::Down ||
              key->code == sf::Keyboard::Key::S);
    isLeft = (key->code == sf::Keyboard::Key::Left ||
              key->code == sf::Keyboard::Key::A);
    isRight = (key->code == sf::Keyboard::Key::Right ||
               key->code == sf::Keyboard::Key::D);
    isSelect = (key->code == sf::Keyboard::Key::Enter ||
                key->code == sf::Keyboard::Key::Space);
    isEscape = (key->code == sf::Keyboard::Key::Escape);
  }

  if (const auto *jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
    if (jb->button == 0 || jb->button == 7)
      isSelect = true; // Button 1 (A) or Start
    if (jb->button == 1 || jb->button == 6)
      isEscape = true; // Button 2 (B) or Back / Coin
    if (jb->button == 8 || jb->button == 9)
      changeGame = true; // Button 9 (Pearson button / BTN_BASE3)
  }

  if (const auto *jm = event.getIf<sf::Event::JoystickMoved>()) {
    if (jm->axis == sf::Joystick::Axis::Y ||
        jm->axis == sf::Joystick::Axis::PovY) {
      if (jm->position < -55.0f)
        isUp = true;
      else if (jm->position > 55.0f)
        isDown = true;
    } else if (jm->axis == sf::Joystick::Axis::X ||
               jm->axis == sf::Joystick::Axis::PovX) {
      if (jm->position < -55.0f)
        isLeft = true;
      else if (jm->position > 55.0f)
        isRight = true;
    }
  }

  if (isEscape) {
    requestBack = true;
  } else if (isUp) {
    if (selectedIndex == 0)
      selectedIndex = 5;
    else if (selectedIndex == 5 || selectedIndex == 6)
      selectedIndex = 4;
    else
      selectedIndex--;
  } else if (isDown) {
    if (selectedIndex == 4)
      selectedIndex = 5;
    else if (selectedIndex == 5 || selectedIndex == 6)
      selectedIndex = 0;
    else
      selectedIndex++;
  } else if (isLeft) {
    if (selectedIndex == 6)
      selectedIndex = 5;
  } else if (isRight) {
    if (selectedIndex == 5)
      selectedIndex = 6;
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
  } else if (changeGame) {
    changeGame = false;
    requestQuit = true;
  }

  if (const auto *mb = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mb->button == sf::Mouse::Button::Left) {
      sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);

      for (int i = 0; i < 5; i++) {
        if (isPointInside(optionRect(i), clickPos)) {
          activeScheme = i;
          selectedIndex = i;
          break;
        }
      }

      if (isPointInside(startButtonRect(), clickPos)) {
        requestStart = true;
      } else if (isPointInside(backButtonRect(), clickPos)) {
        requestBack = true;
      }
    }
  }
}

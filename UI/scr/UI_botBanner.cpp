// =============================================================================
// [AI team] Bot name tag, plan line and rival intro banner (see UI/includes/UI_botBanner.h)
// =============================================================================
#include "../includes/UI_botBanner.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// ---- Colours (the integrator maps these to UI_theme.h tokens) ---------------
const sf::Color kPillFill(12, 16, 26, 215);
const sf::Color kPlanText(215, 225, 240);
const sf::Color kMutedText(165, 180, 205);
const sf::Color kTaglineText(228, 234, 246);
const sf::Color kPanelFill(14, 19, 30, 236);

constexpr float kCanvasW = 1600.0f;
constexpr float kPlayLeft = 254.0f;  // right edge of the West HUD column
constexpr float kPlayRight = 1346.0f; // left edge of the East HUD column
constexpr float kPlayTop = 56.0f;    // below the demand bar

sf::Color withAlpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::clamp(a, 0.0f, 1.0f) * c.a);
    return c;
}

float textWidth(const sf::Text& t) { return t.getLocalBounds().size.x; }

// Places a text so its glyph box starts exactly at (x, y)
void placeAt(sf::Text& t, float x, float y) {
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ std::round(x - b.position.x), std::round(y - b.position.y) });
}

void drawPill(sf::RenderWindow& window, sf::FloatRect r, sf::Color fill, sf::Color outline, float thickness) {
    sf::RectangleShape box(r.size);
    box.setPosition(r.position);
    box.setFillColor(fill);
    box.setOutlineThickness(thickness);
    box.setOutlineColor(outline);
    window.draw(box);
}

} // namespace

void drawBotNameTag(sf::RenderWindow& window, const sf::Font& font, const UIBot& bot, sf::Vector2f cursorPos,
                    bool showIntent) {
    const BotProfile& p = bot.getProfile();
    const BotDifficulty diff = bot.getDifficulty();
    const sf::Color diffColor = botDifficultyColor(diff);

    sf::Text name(font, toUtf8(p.nameBg), 13);
    name.setFillColor(p.accent);
    sf::Text sep(font, toUtf8("·"), 13);
    sep.setFillColor(kMutedText);
    sf::Text level(font, toUtf8(botDifficultyTagBg(diff)), 13);
    level.setFillColor(diffColor);
    level.setStyle(sf::Text::Bold);

    // Second line: the current plan (hidden while it stands on a mine, where it would cover the card)
    const bool mining = bot.getActionState() == BotActionState::MINING_RESOURCE;
    const bool withPlan = showIntent && !mining && !bot.getIntentText().empty();
    sf::Text plan(font, toUtf8(withPlan ? bot.getIntentText() : std::string()), 11);
    plan.setFillColor(kPlanText);

    const float gap = 6.0f;
    const float padX = 7.0f;
    const float padY = 4.0f;
    const float line1H = 14.0f;
    const float line2H = withPlan ? 15.0f : 0.0f;
    float line1W = textWidth(name) + gap + textWidth(sep) + gap + textWidth(level);
    float boxW = std::max(line1W, withPlan ? textWidth(plan) : 0.0f) + 2.0f * padX;
    float boxH = padY + line1H + line2H + padY;
    // Stay inside the play field: never over the side HUD columns or the top demand bar
    float boxX = std::clamp(cursorPos.x - boxW / 2.0f, kPlayLeft, std::max(kPlayLeft, kPlayRight - boxW));
    float boxY = std::max(kPlayTop, cursorPos.y - 22.0f - boxH);

    drawPill(window, { { boxX, boxY }, { boxW, boxH } }, kPillFill, withAlpha(diffColor, 0.85f),
             diff == BotDifficulty::IMPOSSIBLE ? 1.5f : 1.0f);
    // One shared baseline (the name's glyph box sets it), so the small middle dot sits on the line
    float tx = boxX + (boxW - line1W) / 2.0f;
    float lineY = std::round(boxY + padY - name.getLocalBounds().position.y);
    auto putOnLine = [&](sf::Text& t) {
        t.setPosition({ std::round(tx - t.getLocalBounds().position.x), lineY });
        window.draw(t);
        tx += textWidth(t) + gap;
    };
    putOnLine(name);
    putOnLine(sep);
    putOnLine(level);

    if (withPlan) {
        placeAt(plan, boxX + (boxW - textWidth(plan)) / 2.0f, boxY + padY + line1H + 3.0f);
        window.draw(plan);
    }
}

void drawRivalIntroBanner(sf::RenderWindow& window, const sf::Font& font, const UIBot& bot, float timeLeft) {
    if (timeLeft <= 0.0f) return;
    const BotProfile& p = bot.getProfile();
    const BotDifficulty diff = bot.getDifficulty();
    const sf::Color diffColor = botDifficultyColor(diff);

    float shown = BOT_INTRO_BANNER_SEC - timeLeft;
    float alpha = std::min(std::min(1.0f, shown / 0.35f), std::min(1.0f, timeLeft / 0.6f));
    float slide = (1.0f - std::min(1.0f, shown / 0.35f)) * 36.0f;

    sf::Text label(font, toUtf8("СЪПЕРНИК"), 13);
    label.setFillColor(withAlpha(kMutedText, alpha));
    label.setLetterSpacing(1.6f);
    sf::Text name(font, toUtf8(p.nameBg), 30);
    name.setFillColor(withAlpha(p.accent, alpha));
    name.setStyle(sf::Text::Bold);
    sf::Text level(font, toUtf8(botDifficultyNameBg(diff)), 14);
    level.setFillColor(withAlpha(diffColor, alpha));
    level.setStyle(sf::Text::Bold);
    sf::Text tagline(font, toUtf8("„" + p.taglineBg + "“"), 16);
    tagline.setFillColor(withAlpha(kTaglineText, alpha));
    tagline.setStyle(sf::Text::Italic);
    sf::Text style(font, toUtf8(p.styleBg), 13);
    style.setFillColor(withAlpha(diff == BotDifficulty::IMPOSSIBLE ? diffColor : kMutedText, alpha));

    const float padX = 26.0f;
    float levelPillW = textWidth(level) + 18.0f;
    float contentW = std::max({ textWidth(label), textWidth(name) + 14.0f + levelPillW, textWidth(tagline), textWidth(style) });
    float panelW = std::max(460.0f, contentW + 2.0f * padX);
    float panelH = 152.0f;
    // Centre of the screen, between the city and the mine cards (a short "VS" moment)
    float panelX = std::round(kCanvasW / 2.0f - panelW / 2.0f + slide);
    float panelY = 424.0f;

    drawPill(window, { { panelX, panelY }, { panelW, panelH } }, withAlpha(kPanelFill, alpha), withAlpha(p.accent, alpha),
             2.0f);
    sf::RectangleShape stripe({ 6.0f, panelH });
    stripe.setPosition({ panelX, panelY });
    stripe.setFillColor(withAlpha(diffColor, alpha));
    window.draw(stripe);

    float x = panelX + padX;
    placeAt(label, x, panelY + 16.0f);
    window.draw(label);

    placeAt(name, x, panelY + 40.0f);
    window.draw(name);
    // Difficulty pill, vertically centred on the name's glyph box
    float nameH = name.getLocalBounds().size.y;
    float pillH = 22.0f;
    float pillX = x + textWidth(name) + 14.0f;
    float pillY = panelY + 40.0f + nameH / 2.0f - pillH / 2.0f;
    drawPill(window, { { pillX, pillY }, { levelPillW, pillH } }, withAlpha(sf::Color(diffColor.r / 5, diffColor.g / 5, diffColor.b / 5, 230), alpha),
             withAlpha(diffColor, alpha), 1.0f);
    placeAt(level, pillX + 9.0f, pillY + (pillH - level.getLocalBounds().size.y) / 2.0f);
    window.draw(level);

    placeAt(tagline, x, panelY + 88.0f);
    window.draw(tagline);
    placeAt(style, x, panelY + 118.0f);
    window.draw(style);
}

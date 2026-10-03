// =============================================================================
// UI_mainMenu <-> Match Setup screen glue [team b-options] (F-03 / F-21)
// =============================================================================
#include "../includes/UI_mainMenu.h"
#include <iostream>

void UI_mainMenu::openMatchSetup() {
    // Back from the setup screen returns to the submenu that opened it
    setupReturnState = (selectedBotDifficulty == BotDifficulty::NONE) ? MenuState::PLAY_CONTROLS : MenuState::BOT_DIFFICULTY;
    matchSetup.open(selectedBotDifficulty != BotDifficulty::NONE);
    state = MenuState::MATCH_SETUP;
    std::cout << "[UI_mainMenu] Match Setup opened.\n";
}

void UI_mainMenu::handleMatchSetupEvent(const sf::Event& event, const sf::RenderWindow& window) {
    matchSetup.handleEvent(event, window);
    if (matchSetup.isStartRequested()) {
        matchSetup.resetRequests();
        matchSetupConfirmed = true;
        onPlay(); // really starts now (onPlay resets the menu to MAIN)
    } else if (matchSetup.isBackRequested()) {
        matchSetup.resetRequests();
        state = setupReturnState;
        if (state == MenuState::PLAY_CONTROLS) playControls.resetRequests();
    }
}

MatchRules UI_mainMenu::getMatchRules() const {
    if (sandboxSelected) {
        MatchRules r = MatchRules::fromPreset(MatchPreset::STANDARD);
        r.sandbox = true;
        r.graceDays = 0; // the demand set in the panel counts from day 1
        return r;
    }
    return matchSetup.getRules();
}

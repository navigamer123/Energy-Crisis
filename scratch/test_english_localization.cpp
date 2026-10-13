#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <regex>

#include "UI/includes/UI_settings.h"
#include "UI/includes/UI_controlsConfig.h"
#include "UI/includes/UI_buildings.h"

// Regex for Cyrillic unicode range U+0400 to U+04FF (UTF-8 encoding in bytes)
static bool containsCyrillic(const std::string& str) {
    // In UTF-8, Cyrillic characters start with byte 0xD0 or 0xD1
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c == 0xD0 || c == 0xD1) {
            if (i + 1 < str.size()) {
                unsigned char c2 = static_cast<unsigned char>(str[i + 1]);
                if (c == 0xD0 && c2 >= 0x80 && c2 <= 0xBF) return true;
                if (c == 0xD1 && c2 >= 0x80 && c2 <= 0x9F) return true;
            }
        }
    }
    return false;
}

void test_controls_localization() {
    std::cout << "[Test] Testing controls action names in English...\n";
    for (int i = 0; i < static_cast<int>(ControlAction::COUNT); ++i) {
        ControlAction act = static_cast<ControlAction>(i);
        std::string bg = getControlActionName(act, false);
        std::string en = getControlActionName(act, true);
        assert(!en.empty());
        assert(!containsCyrillic(en));
        assert(en != bg);
        std::cout << "  Action " << i << ": EN='" << en << "', BG='" << bg << "'\n";
    }

    std::string jsBg = UI_controlsConfig::get().getJoystickStatusBg();
    std::cout << "  Joystick Status default: '" << jsBg << "'\n";
    std::cout << "-> Controls localization PASSED!\n";
}

void test_resources_localization() {
    std::cout << "[Test] Testing resource names in English...\n";
    ResourceType types[] = {
        ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER,
        ResourceType::COAL, ResourceType::SILICON, ResourceType::SILVER,
        ResourceType::GOLD, ResourceType::MONEY, ResourceType::ENERGY
    };

    for (ResourceType t : types) {
        const char* bg = resourceNameBg(t);
        const char* en = resourceNameEn(t);
        assert(en != nullptr && en[0] != '\0');
        assert(!containsCyrillic(en));
        std::cout << "  Resource " << static_cast<int>(t) << ": EN='" << en << "', BG='" << bg << "'\n";
    }

    // Test recipeText and missingResourcesText in English
    UI_settings::get().setLanguage("en");
    GameEngine engine;
    BuildingCost cost = engine.getBuildingCost(BuildingType::SOLAR_PANEL);

    std::string rec = recipeText(cost);
    std::cout << "  recipeText (EN): " << rec << "\n";
    assert(!containsCyrillic(rec));
    assert(rec.find("wood") != std::string::npos);
    assert(rec.find("iron") != std::string::npos);

    PlayerEconomy econ;
    econ.wood = 2; // missing 3
    econ.iron = 3; // have enough
    econ.copper = 0; // missing 2
    econ.silicon = 4; // have enough
    econ.silver = 0; // missing 1
    std::string missing = missingResourcesText(econ, cost);
    std::cout << "  missingResourcesText (EN): " << missing << "\n";
    assert(!containsCyrillic(missing));
    assert(missing.find("Missing: ") != std::string::npos);

    // Reset back
    UI_settings::get().setLanguage("bg");
    std::string recBg = recipeText(cost);
    assert(containsCyrillic(recBg));
    std::cout << "  recipeText (BG): " << recBg << "\n";

    std::cout << "-> Resource names localization PASSED!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << " Testing English Localization Quality   \n";
    std::cout << "========================================\n";

    test_controls_localization();
    test_resources_localization();

    std::cout << "========================================\n";
    std::cout << " ALL LOCALIZATION QUALITY TESTS PASSED! \n";
    std::cout << "========================================\n";
    return 0;
}

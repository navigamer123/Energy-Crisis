// =============================================================================
// ENERGY CRISIS - SETTINGS FILE AND KEY BINDING TESTS (team b-session, F-06 / F-10)
// Headless: the SFML-free settings and InputMap cores are compiled into this program
// directly (unity include), so "make test" and build_headless.sh need no extra files.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cstdlib>
#include <iostream>
#include <string>
#include "../UI/scr/UI_inputmap.cpp"
#include "../UI/scr/UI_settings.cpp"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond, details)                                                                   \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                      \
    } while (0)

void testDefaultBindings() {
    std::cout << "\n[Default key bindings are complete and conflict-free]\n";
    InputMap m = InputMap::defaults();
    CHECK(!m.hasConflicts(), m.conflicts().size() << " conflicting cells in the defaults");
    for (int p = 1; p <= 2; ++p)
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a)
            CHECK(m.key(p, static_cast<InputAction>(a), 0) != KeyCode::Unknown,
                  "P" << p << " " << InputMap::actionId(static_cast<InputAction>(a)) << " has no primary key");
    // The classic layout the help screen and README describe
    CHECK(m.key(1, InputAction::MoveUp, 0) == KeyCode::W, "P1 up");
    CHECK(m.key(1, InputAction::Action, 0) == KeyCode::Space, "P1 action");
    CHECK(m.key(1, InputAction::NextBuilding, 0) == KeyCode::E, "P1 next");
    CHECK(m.key(2, InputAction::Action, 0) == KeyCode::Enter, "P2 action");
    CHECK(m.key(2, InputAction::Upgrade, 1) == KeyCode::End, "P2 upgrade secondary");
    CHECK(m.key(2, InputAction::Quick1, 0) == KeyCode::Numpad1, "P2 got quick-select keys");
    // No default uses a system shortcut
    for (int p = 1; p <= 2; ++p)
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a)
            for (int s = 0; s < 2; ++s)
                CHECK(!isReservedKey(m.key(p, static_cast<InputAction>(a), s)), "reserved key in defaults");
}

void testConflicts() {
    std::cout << "\n[Duplicates across players and reserved keys are conflicts]\n";
    InputMap m = InputMap::defaults();
    m.set(2, InputAction::Upgrade, 0, KeyCode::E); // P1's "next building" key
    CHECK(m.hasConflicts(), "duplicate across players not detected");
    CHECK(m.isConflicted(2, InputAction::Upgrade, 0), "P2 cell not red");
    CHECK(m.isConflicted(1, InputAction::NextBuilding, 0), "P1 cell not red");
    CHECK(!m.isConflicted(1, InputAction::Action, 0), "unrelated cell red");
    CHECK(m.conflicts().size() == 2, "expected exactly 2 red cells, got " << m.conflicts().size());
    m.clear(2, InputAction::Upgrade, 0);
    CHECK(!m.hasConflicts(), "clearing the duplicate did not resolve it");

    m.set(1, InputAction::Upgrade, 1, KeyCode::F5); // quicksave key
    CHECK(m.isConflicted(1, InputAction::Upgrade, 1), "reserved key accepted");
    m.clear(1, InputAction::Upgrade, 1);

    // Same key twice for one action (primary + secondary) is also a duplicate
    m.set(1, InputAction::Cancel, 1, KeyCode::X);
    CHECK(m.isConflicted(1, InputAction::Cancel, 0) && m.isConflicted(1, InputAction::Cancel, 1), "self duplicate");
}

void testNamesAndHints() {
    std::cout << "\n[Key names, file ids and generated hints]\n";
    CHECK(keyCodeFromId("pagedown") == KeyCode::PageDown, "case-insensitive id");
    CHECK(keyCodeFromId(" Numpad7 ") == KeyCode::Numpad7, "trimmed id");
    CHECK(keyCodeFromId("Num0") == KeyCode::Num0, "digit 0");
    CHECK(keyCodeFromId("NoSuchKey") == KeyCode::Unknown, "unknown id");
    CHECK(std::string(keyCodeId(KeyCode::Space)) == "Space", "Space id");
    CHECK(keyCodeLabel(KeyCode::PageDown) == "PgDn", "PgDn label");
    CHECK(keyCodeLabel(KeyCode::Up) == "\xE2\x86\x91", "arrow label");
    // Every bindable code survives id -> code
    for (int c = 0; c < KeyCode::Max; ++c) {
        std::string id = keyCodeId(c);
        if (!id.empty()) CHECK(keyCodeFromId(id) == c, "id round trip for " << id);
    }

    InputMap m = InputMap::defaults();
    CHECK(bindingHint(m, 1, InputAction::Action) == "[SPACE]", bindingHint(m, 1, InputAction::Action));
    CHECK(bindingHint(m, 1, InputAction::Action, 2) == "[SPACE/ENTER]", "single-player merge: " << bindingHint(m, 1, InputAction::Action, 2));
    CHECK(bindingHint(m, 2, InputAction::Upgrade) == "[RShift/End]", bindingHint(m, 2, InputAction::Upgrade));
    m.set(1, InputAction::Action, 0, KeyCode::K);
    CHECK(bindingHint(m, 1, InputAction::Action) == "[K]", "hint follows the rebinding");
    m.clear(1, InputAction::Action, 0);
    CHECK(bindingHint(m, 1, InputAction::Action).empty(), "unbound action gives an empty hint");
}

void testEncodeDecode() {
    std::cout << "\n[Binding encode / decode]\n";
    InputMap m = InputMap::defaults();
    CHECK(m.encode(2, InputAction::Upgrade) == "RShift,End", m.encode(2, InputAction::Upgrade));
    CHECK(m.encode(1, InputAction::MoveUp) == "W", m.encode(1, InputAction::MoveUp));
    CHECK(m.decode(1, InputAction::MoveUp, "Up,W"), "decode two keys");
    CHECK(m.key(1, InputAction::MoveUp, 0) == KeyCode::Up && m.key(1, InputAction::MoveUp, 1) == KeyCode::W, "decoded keys");
    CHECK(!m.decode(1, InputAction::MoveUp, "Banana"), "garbage accepted");
    CHECK(m.key(1, InputAction::MoveUp, 0) == KeyCode::Up, "garbage changed the binding");
    CHECK(m.decode(1, InputAction::MoveUp, ""), "empty = unbound");
    CHECK(m.key(1, InputAction::MoveUp, 0) == KeyCode::Unknown, "empty did not unbind");
    CHECK(m.decode(1, InputAction::MoveUp, ",T"), "secondary only");
    CHECK(m.key(1, InputAction::MoveUp, 0) == KeyCode::T && m.key(1, InputAction::MoveUp, 1) == KeyCode::Unknown, "secondary promoted");
}

void testSettingsRoundTrip() {
    std::cout << "\n[Settings file round trip, clamping and bad input]\n";
    GameSettings s;
    s.windowWidth = 1920;
    s.windowHeight = 1080;
    s.fullscreen = true;
    s.fpsLimit = 144;
    s.vsync = true;
    s.uiScalePercent = 125;
    s.projectorMode = true;
    s.masterVolume = 40;
    s.musicVolume = 10;
    s.sfxVolume = 90;
    s.tutorialPolicy = TutorialPolicy::Never;
    s.defaultBotDifficulty = 3;
    s.popupSeconds = 6.5f;
    s.autosave = false;
    s.tutorialDoneSolo = true;
    s.gamepadEnabled = false;
    s.singlePadOwner = 1;
    s.keys.set(1, InputAction::Action, 0, KeyCode::K);
    s.keys.set(2, InputAction::Quick6, 1, KeyCode::Numpad0);

    GameSettings r;
    int applied = r.parse(s.serialize());
    CHECK(applied == 18 + 2 * INPUT_ACTION_COUNT, "only " << applied << " keys applied");
    CHECK(r.serialize() == s.serialize(), "serialize(parse(serialize(x))) != serialize(x)");
    CHECK(r.windowWidth == 1920 && r.fullscreen && r.fpsLimit == 144 && r.vsync, "window values");
    CHECK(r.uiScalePercent == 125 && r.projectorMode, "scale values");
    CHECK(r.tutorialPolicy == TutorialPolicy::Never && r.defaultBotDifficulty == 3, "gameplay values");
    CHECK(r.popupSeconds > 6.49f && r.popupSeconds < 6.51f, "popup seconds " << r.popupSeconds);
    CHECK(r.keys == s.keys, "key bindings differ");
    CHECK(r.revision > 0, "parse must bump the revision so UI_main re-applies the window");

    GameSettings c;
    c.parse("ui_scale=117\nfps_limit=5\nmaster_volume=250\npopup_seconds=99\nsingle_pad_owner=7\n"
            "default_bot_difficulty=0\nwindow_width=10\nfullscreen=maybe\nunknown_key=1\nno equals sign\n");
    CHECK(c.uiScalePercent == 125, "ui_scale snaps to the nearest step: " << c.uiScalePercent);
    CHECK(c.fpsLimit == 15, "fps clamped: " << c.fpsLimit);
    CHECK(c.masterVolume == 100, "volume clamped");
    CHECK(c.popupSeconds == 8.0f, "popup clamped");
    CHECK(c.singlePadOwner == 2 && c.defaultBotDifficulty == 1, "owner/difficulty clamped");
    CHECK(c.windowWidth == 800, "width clamped");
    CHECK(!c.fullscreen, "malformed bool kept the default");

    GameSettings d;
    d.parse("fps_limit=0\n");
    CHECK(d.fpsLimit == 0, "0 = unlimited must survive clamping");

    // A hand-edited file that gives two actions the same key falls back to the default keys
    GameSettings e;
    e.parse("key.p1.action=Enter\n");
    CHECK(e.keys == InputMap::defaults(), "conflicting bindings from a file were kept");

    GameSettings f;
    f.projectorMode = true;
    f.uiScalePercent = 90;
    CHECK(f.overlayScale() >= 1.25f, "projector mode must enlarge overlays");
    f.uiScalePercent = 150;
    CHECK(f.overlayScale() == 1.5f, "projector keeps a larger chosen scale");
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - SETTINGS AND KEY BINDING TESTS\n";
    std::cout << "========================================================\n";
    testDefaultBindings();
    testConflicts();
    testNamesAndHints();
    testEncodeDecode();
    testSettingsRoundTrip();
    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " SETTINGS/INPUT CHECKS PASSED\n";
        return 0;
    }
    std::cout << " " << g_failures << " OF " << g_checks << " SETTINGS/INPUT CHECKS FAILED\n";
    return 1;
}

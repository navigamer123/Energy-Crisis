#ifndef UI_ARCADEMODE_H
#define UI_ARCADEMODE_H

class ArcadeMode {
public:
    static bool isEnabled() {
#if defined(ARCADE_MODE) && (ARCADE_MODE != 0)
        return true;
#else
        return s_enabled;
#endif
    }

    static void setEnabled(bool enabled) {
        s_enabled = enabled;
    }

private:
    inline static bool s_enabled = false;
};

#endif // UI_ARCADEMODE_H

#ifndef UI_ARCADEMODE_H
#define UI_ARCADEMODE_H

class ArcadeMode {
public:
    static bool isEnabled() {
        if (s_override) return s_overrideVal;
#if defined(ARCADE_MODE) && (ARCADE_MODE != 0)
        return true;
#else
        return s_enabled;
#endif
    }

    static void setEnabled(bool enabled) {
        s_enabled = enabled;
        s_override = true;
        s_overrideVal = enabled;
    }

    static void clearOverride() {
        s_override = false;
    }

private:
    inline static bool s_enabled = false;
    inline static bool s_override = false;
    inline static bool s_overrideVal = false;
};

#endif // UI_ARCADEMODE_H

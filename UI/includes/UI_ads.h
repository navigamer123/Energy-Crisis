#ifndef UI_ADS_H
#define UI_ADS_H

#include <string>

// =============================================================================
// AdManager: Handles rewarded video ads and session unlocks for Energy Crisis.
//
// On Android:
//   Communicates with MainActivity.java via JNI to load and show Google AdMob
//   rewarded video ads using unit ID: ca-app-pub-8879039481537676/4813432180.
//
// On Desktop / Arcade / Non-Android:
//   Simulates or auto-unlocks to keep desktop development, CI, and arcade cabinets
//   fully operational without any Android dependencies.
// =============================================================================

enum class AdStatus {
    IDLE,
    LOADING,
    READY,
    SHOWING,
    REWARD_EARNED,
    CLOSED,
    FAILED,
    NOT_READY
};

class AdManager {
public:
    static AdManager& get();

    // Whether 30m Battle mode has been unlocked
    bool is30mUnlocked() const;
    void set30mUnlocked(bool unlocked);

    // Ad playback controls
    void showRewardedAd();
    bool isAdReady() const;

    // Status queries
    AdStatus getStatus() const;
    const std::string& getStatusMessage() const;
    void clearStatusMessage();

    // Polls whether a reward was just granted (resets the flag on read)
    bool pollRewardEarned();

    // Callbacks triggered from Android Java layer (via JNI) or simulated desktop
    void onRewardEarned();
    void onAdClosed();
    void onAdFailed();
    void onAdNotReady();

private:
    AdManager();
    ~AdManager() = default;
    AdManager(const AdManager&) = delete;
    AdManager& operator=(const AdManager&) = delete;

    bool m_30mUnlocked = false;
    bool m_rewardEarnedFlag = false;
    AdStatus m_status = AdStatus::IDLE;
    std::string m_statusMessage;
};

#endif // UI_ADS_H


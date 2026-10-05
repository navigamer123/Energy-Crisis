#ifndef UI_CREDITS_H
#define UI_CREDITS_H

#include <string>
#include <mutex>

class CreditsManager {
public:
    static CreditsManager& get();

    void init();
    void update(float dt);

    bool isDatabaseAvailable() const { return dbAvailable; }
    int getCurrentCredits() const { return currentCredits; }

    // Consumes amount of credits. Returns true if successful.
    bool tryConsumeCredits(int amount = 1);

    // Requirement: "след първата приключена да иска токен"
    bool requiresCreditForNewGame() const { return matchesCompletedCount >= 1; }
    void onMatchCompleted() { matchesCompletedCount++; }
    int getMatchesCompletedCount() const { return matchesCompletedCount; }

private:
    CreditsManager();
    ~CreditsManager();

    std::string resolveDatabasePath();
    int readCreditsFromDb(const std::string& path);
    bool writeCreditsToDb(const std::string& path, int newCredits);

    std::string resolvedDbPath;
    bool dbAvailable = false;
    int currentCredits = 0;
    int lastCredits = 0;
    bool hasShownInitial = false;
    float pollTimer = 0.0f;
    int matchesCompletedCount = 0;
    mutable std::mutex dbMutex;
};

#endif // UI_CREDITS_H

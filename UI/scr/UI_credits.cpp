#include "../includes/UI_credits.h"
#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_settings.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>

CreditsManager& CreditsManager::get() {
    static CreditsManager instance;
    return instance;
}

CreditsManager::CreditsManager() {
    init();
}

CreditsManager::~CreditsManager() {}

std::string CreditsManager::resolveDatabasePath() {
    // 1) Environment variable DHO_DB_PATH (Arcade system)
    const char* envPath = std::getenv("DHO_DB_PATH");
    if (envPath && envPath[0] != '\0') {
        std::error_code ec;
        if (std::filesystem::exists(envPath, ec)) {
            std::cout << "[CreditsManager] Using DB from DHO_DB_PATH: " << envPath << "\n";
            return std::string(envPath);
        }
    }

    // 2) Fallback: assets/test_db.db
    std::string fallbackPath = "assets/test_db.db";
    std::error_code ec;
    if (!std::filesystem::exists(fallbackPath, ec)) {
        // Create initial test DB with settings table and default 2 credits
        sqlite3* db = nullptr;
        if (sqlite3_open(fallbackPath.c_str(), &db) == SQLITE_OK) {
            const char* sql = "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT);"
                              "INSERT OR IGNORE INTO settings (key, value) VALUES ('credits', '2');";
            char* errMsg = nullptr;
            sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
            if (errMsg) sqlite3_free(errMsg);
            sqlite3_close(db);
            std::cout << "[CreditsManager] Created fallback test DB: " << fallbackPath << " with 2 initial credits.\n";
        }
    }
    return fallbackPath;
}

void CreditsManager::init() {
    resolvedDbPath = resolveDatabasePath();
    if (!resolvedDbPath.empty()) {
        int initial = readCreditsFromDb(resolvedDbPath);
        if (dbAvailable) {
            currentCredits = initial;
            lastCredits = initial;
            hasShownInitial = true;
            std::cout << "[CreditsManager] Initial credits: " << currentCredits << "\n";
        }
    }
}

int CreditsManager::readCreditsFromDb(const std::string& path) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_SHAREDCACHE, nullptr) != SQLITE_OK) {
        dbAvailable = false;
        if (db) sqlite3_close(db);
        return 0;
    }

    const char* sql = "SELECT value FROM settings WHERE key = 'credits' LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int credits = 0;
    bool found = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        dbAvailable = true;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* val = sqlite3_column_text(stmt, 0);
            if (val) {
                try {
                    std::string s = reinterpret_cast<const char*>(val);
                    while (!s.empty() && (s.front() == '\'' || s.front() == '"' || s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
                    while (!s.empty() && (s.back() == '\'' || s.back() == '"' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
                    if (!s.empty()) {
                        credits = std::stoi(s);
                    }
                } catch (...) {
                    credits = 0;
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);

    return credits;
}

bool CreditsManager::writeCreditsToDb(const std::string& path, int newCredits) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_SHAREDCACHE, nullptr) != SQLITE_OK) {
        dbAvailable = false;
        if (db) sqlite3_close(db);
        return false;
    }

    const char* sql = "INSERT INTO settings(key, value) VALUES('credits', ?) "
                      "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string valStr = std::to_string(newCredits);
        sqlite3_bind_text(stmt, 1, valStr.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = true;
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

void CreditsManager::update(float dt) {
    pollTimer += dt;
    if (pollTimer < 1.0f) return;
    pollTimer = 0.0f;

    if (resolvedDbPath.empty()) return;

    int newCredits = readCreditsFromDb(resolvedDbPath);
    if (!dbAvailable) return;

    if (!hasShownInitial) {
        hasShownInitial = true;
        lastCredits = newCredits;
        currentCredits = newCredits;
        return;
    }

    if (newCredits != lastCredits) {
        int delta = newCredits - lastCredits;
        lastCredits = newCredits;
        currentCredits = newCredits;

        bool isEn = (UI_settings::get().getLanguage() == "en");
        if (delta > 0) {
            std::string msg = isEn ? ("CREDITS +" + std::to_string(delta) + "\n(TOTAL: " + std::to_string(newCredits) + ")")
                                   : ("КРЕДИТИ +" + std::to_string(delta) + "\n(ОБЩО: " + std::to_string(newCredits) + ")");
            ArcadePopup::get().show(msg, 2.5f);
        }
    }
}

bool CreditsManager::tryConsumeCredits(int amount) {
    if (amount <= 0) return true;

    if (resolvedDbPath.empty()) {
        resolvedDbPath = resolveDatabasePath();
    }

    int current = readCreditsFromDb(resolvedDbPath);
    bool isEn = (UI_settings::get().getLanguage() == "en");

    if (!dbAvailable || current < amount) {
        std::string msg = isEn ? ("NOT ENOUGH CREDITS\n(HAVE: " + std::to_string(current) + ")")
                               : ("НЯМА КРЕДИТИ\n(НАЛИЧНИ: " + std::to_string(current) + ")");
        ArcadePopup::get().show(msg, 2.5f);
        return false;
    }

    int newCredits = current - amount;
    if (writeCreditsToDb(resolvedDbPath, newCredits)) {
        currentCredits = newCredits;
        lastCredits = newCredits;

        std::string msg = isEn ? ("CREDITS -" + std::to_string(amount) + "\n(TOTAL: " + std::to_string(newCredits) + ")")
                               : ("КРЕДИТИ -" + std::to_string(amount) + "\n(ОБЩО: " + std::to_string(newCredits) + ")");
        ArcadePopup::get().show(msg, 2.0f);
        return true;
    }
    return false;
}

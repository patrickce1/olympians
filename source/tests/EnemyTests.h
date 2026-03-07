// EnemyTests.h
#ifndef __ENEMY_TESTS_H__
#define __ENEMY_TESTS_H__

#include <string>

/**
 * Static test suite for Enemy + EnemyController state flow, cooldown behavior,
 * and event resolution.
 *
 * HOW TO RUN:
 *   Call EnemyTests::runAll() from your app startup AFTER cugl is initialized
 *   but BEFORE the game loop starts (e.g. in AppDelegate::onStartup()).
 *
 * Example:
 *   EnemyTests::runAll("json/enemies.json", "json/characters.json");
 */
class EnemyTests {
public:
    /**
     * Loads fixtures from JSON and runs all Enemy tests.
     *
     * @param enemiesJsonPath     Path to enemies.json (e.g. "json/enemies.json")
     * @param charactersJsonPath  Path to characters.json (e.g. "json/characters.json")
     */
    static void runAll(const std::string& enemiesJsonPath,
                       const std::string& charactersJsonPath);
};

#endif /* __ENEMY_TESTS_H__ */

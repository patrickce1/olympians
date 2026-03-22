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
 *   EnemyTests::runAll("json/enemies.json", "json/houses.json");
 */
class EnemyTests {
public:
    /**
     * Loads fixtures from JSON and runs all Enemy tests.
     *
     * @param enemiesJsonPath     Path to enemies.json (e.g. "json/enemies.json")
     * @param housesJsonPath  Path to houses.json (e.g. "json/houses.json")
     */
    static void runAll(const std::string& enemiesJsonPath,
                       const std::string& housesJsonPath);
};

#endif /* __ENEMY_TESTS_H__ */

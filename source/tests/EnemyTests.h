// EnemyTests.h
#ifndef __ENEMY_TESTS_H__
#define __ENEMY_TESTS_H__

#include <string>

/**
 * Static test suite for Enemy + EnemyController state flow, cooldown behavior,
 * and event resolution.
 *
 * ARCHITECTURE:
 *   The Enemy class uses file-scope static instances for the EnemyLoader to avoid
 *   reloading JSON data on each Enemy instantiation. The first call to Enemy::init()
 *   loads the JSON; subsequent calls reuse the cached data.
 *   
 *   Tests benefit from this pattern:
 *   - First test initializes loader once
 *   - Remaining tests run quickly without I/O
 *   - Each test creates isolated Enemy instances
 *   - No state carryover between tests
 *
 * HOW TO RUN:
 *   Call EnemyTests::runAll() from your app startup AFTER cugl is initialized
 *   but BEFORE the game loop starts (e.g. in AppDelegate::onStartup()).
 *
 * Example:
 *   EnemyTests::runAll("json/enemies.json", "json/houses.json");
 *
 * TEST COVERAGE:
 *   - Section 1: Initialization of core fields (health, state, etc.)
 *   - Section 2: State timing, buildup -> fire -> next state transitions
 *   - Section 3: Health clamping at 0 and maxHealth boundaries
 *   - Section 4: EnemyController mechanics (targeting, damage resolution)
 *   - Section 5: Boss-specific mechanics (Cerberus heal, Cyclops multipliers)
 */
class EnemyTests {
public:
    /**
     * Loads fixtures from JSON and runs all Enemy tests.
     *
     * @param enemiesJsonPath     Path to enemies.json (e.g. "json/enemies.json")
     * @param housesJsonPath      Path to houses.json (e.g. "json/houses.json")
     */
    static void runAll(const std::string& enemiesJsonPath,
                       const std::string& housesJsonPath);
};

#endif /* __ENEMY_TESTS_H__ */

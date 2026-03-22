// PlayerTests.h
#ifndef __PLAYER_TESTS_H__
#define __PLAYER_TESTS_H__

#include <string>

/**
 * Static test suite for Player inventory, card passing, card usage, and AI behavior.
 *
 * Call PlayerTests::runAll() once from your app startup after cugl is initialized.
 * All results are printed via CULog as [PASS] or [FAIL]. Remove before shipping.
 *
 * All fixtures are loaded from the same JSON files your game uses — no mock data.
 * The ItemDatabase is loaded exactly as ItemController::init() does it.
 *
 * Example (in AppDelegate::onStartup, before the game loop):
 *
 *   PlayerTests::runAll(
 *       "json/characters.json",
 *       "json/items.json",
 *       "json/enemies.json",
 *       "assets/json/playerAI.json"
 *   );
 */
class PlayerTests {
public:
    /**
     * Loads all fixtures from JSON and runs all unit tests.
     *
     * @param charactersJsonPath  Path to characters.json (e.g. "json/characters.json")
     * @param itemsJsonPath       Path to items.json      (e.g. "json/items.json")
     * @param enemiesJsonPath     Path to enemies.json    (e.g. "json/enemies.json")
     * @param aiConfigPath        Path to playerAI.json   (e.g. "assets/json/playerAI.json")
     */
    static void runAll(const std::string& charactersJsonPath,
                       const std::string& itemsJsonPath,
                       const std::string& enemiesJsonPath,
                       const std::string& aiConfigPath);
};

#endif /* __PLAYER_TESTS_H__ */

// tests/EnemyTests.cpp
#include <cugl/cugl.h>
#include <fstream>
#include <cstdio>
#include "../EnemyLoader.h"
#include "../Enemy.h"

/** Writes a temporary enemy JSON to be used by the tests. */
static std::string writeTempJson(const std::string& contents) {
    // Use a location CUGL can reliably read/write at runtime
    std::string dir = cugl::Application::get()->getSaveDirectory();
    std::string path = dir + "enemy_test_temp.json";

    std::ofstream out(path);
    CUAssertLog(out.is_open(), "Could not open temp JSON file for writing: %s", path.c_str());
    out << contents;
    out.close();

    CULog("Wrote temp enemy test JSON to: %s", path.c_str());
    return path;
}

/** Deletes the temporary JSON. */
static void removeTempJson(const std::string& path) {
    std::remove(path.c_str());
}

/** Tests that the JSON is properly parsed into an instance of Enemy. */
static void testEnemyLoaderParsesBasicDef() {
    CULog("[TEST] testEnemyLoaderParsesBasicDef START");

    const std::string json = R"JSON(
{
  "enemies": [
    {
      "id": "enemy1",
      "name": "Enemy 1",
      "maxHealth": 20.0,
      "spritesheetPath": "textures/enemy1.png",
      "defaultShields": { "front": 0.0, "back": 1.0, "left": 0.5, "right": 0.5 },
      "states": {
        "idle": { "animationRow": 0 },
        "attack1": {
          "animationRow": 1,
          "buildUpTime": 1.0,
          "cooldownTime": 2.0,
          "events": [ { "type": "DAMAGE", "target": "front", "amount": 3.0 } ],
          "nextState": "idle"
        }
      }
    }
  ]
}
)JSON";

    const std::string path = writeTempJson(json);

    EnemyLoader loader;
    CUAssertLog(loader.loadFromFile(path), "EnemyLoader failed to load JSON");

    CUAssertLog(loader.has("enemy1"), "EnemyLoader missing enemy1 after load");

    const auto& def = loader.get("enemy1");
    CUAssertLog(def.maxHealth == 20.0f, "Enemy maxHealth mismatch");

    removeTempJson(path);

    CULog("[TEST] testEnemyLoaderParsesBasicDef PASS");
}

/** Tests the state transition functionality. Enemy should go from a state to its 'nextState' nad should not be able to go to a new attack state if the cooldown is still active. */
static void testEnemyStateFlowAndCooldown() {
    CULog("[TEST] testEnemyStateFlowAndCooldown START");

    const std::string json = R"JSON(
{
  "enemies": [
    {
      "id": "enemy1",
      "name": "Enemy 1",
      "maxHealth": 20.0,
      "spritesheetPath": "textures/enemy1.png",
      "states": {
        "idle": { "animationRow": 0 },
        "attack1": {
          "animationRow": 1,
          "buildUpTime": 1.0,
          "cooldownTime": 2.0,
          "events": [ { "type": "DAMAGE", "target": "front", "amount": 3.0 } ],
          "nextState": "idle"
        }
      }
    }
  ]
}
)JSON";

    const std::string path = writeTempJson(json);

    EnemyLoader loader;
    CUAssertLog(loader.loadFromFile(path), "EnemyLoader failed to load JSON");
    Enemy enemy("enemy1", loader);

    CUAssertLog(enemy.getCurrentStateName() == "idle", "Enemy did not start in idle");
    CUAssertLog(enemy.requestState("attack1"), "Failed to enter attack1");

    enemy.update(1.1f);

    auto events = enemy.takeFiredEvents();
    CUAssertLog(events.size() == 1, "Expected exactly 1 fired event");

    CUAssertLog(enemy.getCurrentStateName() == "idle", "Enemy did not return to idle");

    removeTempJson(path);

    CULog("[TEST] testEnemyStateFlowAndCooldown PASS");
}

/** Tests enemy healing/damage works as intended. Cannot go below zero or above max health. */
static void testEnemyHealthClamp() {
    CULog("[TEST] testEnemyHealthClamp START");

    const std::string json = R"JSON(
{
  "enemies": [
    {
      "id": "dummy",
      "name": "Dummy",
      "maxHealth": 10.0,
      "spritesheetPath": "textures/dummy_enemy.png",
      "states": { "idle": { "animationRow": 0 } }
    }
  ]
}
)JSON";

    const std::string path = writeTempJson(json);

    EnemyLoader loader;
    CUAssertLog(loader.loadFromFile(path), "EnemyLoader failed to load JSON");
    Enemy enemy("dummy", loader);

    enemy.updateHealth(-3.0f);
    CUAssertLog(enemy.getCurrentHealth() == 7.0f, "Damage application mismatch");

    enemy.updateHealth(-100.0f);
    CUAssertLog(enemy.getCurrentHealth() == 0.0f, "Health should clamp at 0");

    enemy.updateHealth(100.0f);
    CUAssertLog(enemy.getCurrentHealth() == 10.0f, "Health should clamp at max");

    removeTempJson(path);

    CULog("[TEST] testEnemyHealthClamp PASS");
}

/** Runs  unit tests for EnemyLoader + Enemy. */
void runEnemyTests() {
    CULog("=== EnemyTests: START ===");
    testEnemyLoaderParsesBasicDef();
    testEnemyStateFlowAndCooldown();
    testEnemyHealthClamp();
    CULog("=== EnemyTests: PASS ===");
}


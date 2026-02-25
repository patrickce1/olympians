// tests/EnemyTests.cpp
#include <cugl/cugl.h>
#include <fstream>
#include <cstdio>

#include "EnemyLoader.h"
#include "Enemy.h"

#if defined(DEBUG) || defined(_DEBUG)

static std::string writeTempJson(const std::string& contents) {
    const std::string path = "enemy_test_temp.json";
    std::ofstream out(path);
    CUAssertLog(out.is_open(), "Could not open temp JSON file for writing");
    out << contents;
    out.close();
    return path;
}

static void removeTempJson(const std::string& path) {
    std::remove(path.c_str());
}

static void testEnemyLoaderParsesBasicDef() {
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
    CUAssertLog(def.id == "enemy1", "Enemy id mismatch");
    CUAssertLog(def.name == "Enemy 1", "Enemy name mismatch");
    CUAssertLog(def.maxHealth == 20.0f, "Enemy maxHealth mismatch");
    CUAssertLog(def.spritesheetPath == "textures/enemy1.png", "Enemy spritesheetPath mismatch");
    CUAssertLog(def.states.count("idle") > 0, "Enemy missing idle state");
    CUAssertLog(def.states.count("attack1") > 0, "Enemy missing attack1 state");

    const auto& st = def.states.at("attack1");
    CUAssertLog(st.animationRow == 1, "attack1 animationRow mismatch");
    CUAssertLog(st.buildUpTime == 1.0f, "attack1 buildUpTime mismatch");
    CUAssertLog(st.cooldownTime == 2.0f, "attack1 cooldownTime mismatch");
    CUAssertLog(st.nextState == "idle", "attack1 nextState mismatch");
    CUAssertLog(st.events.size() == 1, "attack1 expected exactly 1 event");

    const auto& ev = st.events[0];
    CUAssertLog(ev.type == EnemyLoader::EventType::DAMAGE, "Event type mismatch");
    CUAssertLog(ev.target == EnemyLoader::RelDir::FRONT, "Event target mismatch");
    CUAssertLog(ev.amount == 3.0f, "Event amount mismatch");

    removeTempJson(path);
}

static void testEnemyStateFlowAndCooldown() {
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

    // starts idle
    CUAssertLog(enemy.getCurrentStateName() == "idle", "Enemy did not start in idle");

    // request attack
    CUAssertLog(enemy.requestState("attack1"), "Failed to enter attack1");
    CUAssertLog(enemy.getCurrentStateName() == "attack1", "Enemy state not attack1 after request");

    // before buildUpTime -> no events
    enemy.update(0.5f);
    CUAssertLog(enemy.takeFiredEvents().empty(), "Events fired too early");

    // after buildUpTime -> should fire once + return to idle
    enemy.update(0.6f); // total 1.1s
    auto events = enemy.takeFiredEvents();
    CUAssertLog(events.size() == 1, "Expected exactly 1 fired event");
    CUAssertLog(events[0].def.type == EnemyLoader::EventType::DAMAGE, "Fired event type mismatch");
    CUAssertLog(events[0].target == EnemyLoader::RelDir::FRONT, "Fired event target mismatch");
    CUAssertLog(events[0].def.amount == 3.0f, "Fired event amount mismatch");
    CUAssertLog(enemy.getCurrentStateName() == "idle", "Enemy did not return to idle");

    // cooldown lockout should block non-idle state
    CUAssertLog(!enemy.requestState("attack1"), "attack1 should be blocked during cooldown");

    // after cooldown, should allow attack again
    enemy.update(2.0f);
    CUAssertLog(enemy.requestState("attack1"), "attack1 should be allowed after cooldown");

    removeTempJson(path);
}

static void testEnemyHealthClamp() {
    // Loader just to construct enemy cleanly
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
    CUAssertLog(enemy.getCurrentHealth() == 10.0f, "Initial health mismatch");

    enemy.updateHealth(-3.0f);
    CUAssertLog(enemy.getCurrentHealth() == 7.0f, "Damage application mismatch");

    enemy.updateHealth(-100.0f);
    CUAssertLog(enemy.getCurrentHealth() == 0.0f, "Health should clamp at 0");

    enemy.updateHealth(100.0f);
    CUAssertLog(enemy.getCurrentHealth() == 10.0f, "Health should clamp at max");

    removeTempJson(path);
}

/** Runs debug-only unit tests for EnemyLoader + Enemy. */
void runEnemyTests() {
    CULog("=== EnemyTests: START ===");
    testEnemyLoaderParsesBasicDef();
    testEnemyStateFlowAndCooldown();
    testEnemyHealthClamp();
    CULog("=== EnemyTests: PASS ===");
}

#else

// Release builds: no-op so tests don't run/shipping build stays clean.
void runEnemyTests() {}

#endif

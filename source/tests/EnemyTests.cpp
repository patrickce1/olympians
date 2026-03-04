// EnemyTests.cpp
// Unit tests for Enemy + EnemyController state flow + event resolution.
//
// HOW TO RUN:
//   Call EnemyTests::runAll() from your app startup AFTER cugl is initialized
//   but BEFORE the game loop starts, e.g. in AppDelegate::onStartup().
//   All results print via CULog as [PASS] or [FAIL]. Remove the call before shipping.
//
//   Example:
//     EnemyTests::runAll("json/enemies.json", "json/characters.json");

#include "EnemyTests.h"
#include "Enemy.h"
#include "EnemyController.h"
#include "Player.h"
#include "CharacterLoader.h"
#include <cugl/cugl.h>
#include <unordered_set>

namespace {

int _passed = 0;
int _failed = 0;

void expect(bool condition, const std::string& label) {
    if (condition) {
        CULog("[PASS] %s", label.c_str());
        _passed++;
    } else {
        CULog("[FAIL] %s", label.c_str());
        _failed++;
    }
}

void printSummary() {
    CULog("─────────────────────────────────────────");
    CULog("  %d passed   %d failed", _passed, _failed);
    CULog("─────────────────────────────────────────");
}

// ─────────────────────────────────────────────
// Fixtures / helpers
// ─────────────────────────────────────────────

CharacterLoader loadCharacters(const std::string& charactersJsonPath) {
    CharacterLoader loader;
    if (!loader.loadFromFile(charactersJsonPath)) {
        CULogError("EnemyTests: failed to load characters from '%s'", charactersJsonPath.c_str());
    }
    return loader;
}

std::vector<Player> makePlayersRing(const CharacterLoader& loader,
                                   const std::string& characterId,
                                   int count) {
    std::vector<Player> players;
    for (int i = 0; i < count; i++) {
        std::string name = "Player " + std::to_string(i + 1);
        players.emplace_back(characterId, i + 1, name, loader);
    }

    const int n = (int)players.size();
    for (int i = 0; i < n; i++) {
        players[i].setLeftPlayer (&players[(i - 1 + n) % n]);
        players[i].setRightPlayer(&players[(i + 1)     % n]);
    }
    return players;
}

std::shared_ptr<Enemy> makeEnemy(const std::string& enemiesJsonPath,
                                 const std::string& enemyId) {
    auto e = std::make_shared<Enemy>();
    bool ok = e->init(enemyId, enemiesJsonPath);
    expect(ok, "Enemy::init succeeds for '" + enemyId + "'");
    return ok ? e : nullptr;
}

std::string firstAttackStateName(const std::shared_ptr<Enemy>& enemy) {
    if (!enemy) return "";
    const auto& states = enemy->getStates();
    for (const auto& kv : states) {
        const auto& name = kv.first;
        const auto& def  = kv.second;
        if (def.tag == "attack") return name;
    }
    return "";
}

std::string expectedNextOrIdle(const std::shared_ptr<Enemy>& enemy,
                               const std::string& stateName) {
    if (!enemy) return "idle";
    const auto& states = enemy->getStates();
    auto it = states.find(stateName);
    if (it == states.end()) return "idle";

    const std::string& next = it->second.nextState;
    if (!next.empty() && states.count(next) > 0) return next;
    return "idle";
}

/**
 * Advances the enemy until it fires at least one event (or times out).
 * Returns true if events fired; fills firedEventsOut and preserves enemy state after the fire.
 */
bool stepUntilFire(const std::shared_ptr<Enemy>& enemy,
                   float dtStep,
                   int maxSteps,
                   std::vector<Enemy::FiredEvent>& firedEventsOut) {
    firedEventsOut.clear();
    if (!enemy) return false;

    for (int i = 0; i < maxSteps; i++) {
        enemy->update(dtStep);
        auto ev = enemy->takeFiredEvents();
        if (!ev.empty()) {
            firedEventsOut = std::move(ev);
            return true;
        }
    }
    return false;
}

/**
 * Walks the configured nextState chain starting from startState, returning the first state
 * in the chain that has cooldownTime > 0. Returns "" if none found or chain loops.
 *
 * This matches your design:
 * - intermediate phases can have cooldownTime=0 (combo continuation)
 * - lockout should kick in on the final phase (cooldownTime>0)
 */
std::string findFirstCooldownStateInChain(const std::shared_ptr<Enemy>& enemy,
                                         const std::string& startState) {
    if (!enemy) return "";
    const auto& states = enemy->getStates();
    if (states.count(startState) == 0) return "";

    std::unordered_set<std::string> seen;
    std::string cur = startState;

    for (int hops = 0; hops < 32; hops++) {
        if (seen.count(cur) > 0) return ""; // loop
        seen.insert(cur);

        auto it = states.find(cur);
        if (it == states.end()) return "";
        if (it->second.cooldownTime > 0.0f) return cur;

        // follow next
        std::string next = it->second.nextState;
        if (next.empty()) next = "idle";
        if (states.count(next) == 0) next = "idle";

        if (next == "idle") {
            return "";
        }
        cur = next;
    }

    return "";
}

bool anyPlayersAlive(const std::vector<Player>& players) {
    for (const auto& p : players) {
        if (p.isAlive()) return true;
    }
    return false;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 1 — Init
// ─────────────────────────────────────────────────────────────────────────────

static void testEnemyInitSetsCoreFields(const std::string& enemiesJsonPath) {
    auto enemy = std::make_shared<Enemy>();
    bool ok = enemy->init("enemy1", enemiesJsonPath);

    expect(ok, "init: returns true");
    if (!ok) return;

    expect(enemy->getId() == "enemy1", "init: id set");
    expect(!enemy->getName().empty(), "init: name set");
    expect(enemy->getMaxHealth() > 0.0f, "init: maxHealth > 0");
    expect(enemy->getCurrentHealth() == enemy->getMaxHealth(), "init: currentHealth starts at max");
    expect(enemy->getCurrentStateName() == "idle", "init: starts in idle");
    expect(enemy->getStates().count("idle") > 0, "init: states include idle");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2 — Enemy state timing (buildUp → fire → next + cooldown)
// ─────────────────────────────────────────────────────────────────────────────

static void testEnemyFiresEventsAfterBuildUp(const std::string& enemiesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "enemy1");
    if (!enemy) return;

    std::string attack = firstAttackStateName(enemy);
    expect(!attack.empty(), "stateTiming: found at least 1 attack-tagged state");
    if (attack.empty()) return;

    bool entered = enemy->requestState(attack);
    expect(entered, "stateTiming: requestState enters attack");
    expect(enemy->getCurrentStateName() == attack, "stateTiming: current state is attack");

    enemy->update(0.5f);
    expect(enemy->takeFiredEvents().empty(), "stateTiming: no events before buildUpTime");

    const std::string expectedNext = expectedNextOrIdle(enemy, attack);

    std::vector<Enemy::FiredEvent> fired;
    bool didFire = stepUntilFire(enemy, /*dtStep=*/0.5f, /*maxSteps=*/120, fired);
    expect(didFire, "stateTiming: eventually fires events after buildUpTime");
    if (!didFire) return;

    expect(fired.size() >= 1, "stateTiming: fired >= 1 event");
    expect(enemy->getCurrentStateName() == expectedNext,
           "stateTiming: transitions to nextState (or idle fallback) after firing");
}

static void testEnemyCooldownBlocksNonIdle(const std::string& enemiesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "enemy1");
    if (!enemy) return;

    std::string attack = firstAttackStateName(enemy);
    if (attack.empty()) { expect(false, "cooldown: missing attack state"); return; }

    std::string cooldownState = findFirstCooldownStateInChain(enemy, attack);
    expect(!cooldownState.empty(),
           "cooldown: found a cooldownTime>0 state in the attack chain");
    if (cooldownState.empty()) return;

    enemy->requestState(attack);

    bool firedCooldownPhase = false;
    for (int phase = 0; phase < 8; phase++) {
        std::string stateThatWillFire = enemy->getCurrentStateName();
        std::vector<Enemy::FiredEvent> fired;

        bool didFire = stepUntilFire(enemy, 0.5f, 240, fired);
        expect(didFire, "cooldown: phase fires at least one event");
        if (!didFire) return;

        if (stateThatWillFire == cooldownState) {
            firedCooldownPhase = true;
            break;
        }
    }
    expect(firedCooldownPhase, "cooldown: reached and fired the cooldown-applying phase");
    if (!firedCooldownPhase) return;

    expect(!enemy->canStartNonIdleState(), "cooldown: lockout active after cooldown phase fires");

    bool allowedNow = enemy->requestState(attack);
    expect(!allowedNow, "cooldown: non-idle state blocked during lockout");

    bool becameReady = false;
    for (int i = 0; i < 120; i++) { // up to 60s at 0.5
        enemy->update(0.5f);
        if (enemy->canStartNonIdleState()) { becameReady = true; break; }
    }
    expect(becameReady, "cooldown: lockout eventually ends");
    if (becameReady) {
        expect(enemy->requestState(attack), "cooldown: attack allowed after lockout ends");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 3 — Health clamping
// ─────────────────────────────────────────────────────────────────────────────

static void testEnemyHealthClamp(const std::string& enemiesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "enemy1");
    if (!enemy) return;

    float maxHp = enemy->getMaxHealth();

    enemy->updateHealth(-999999.0f);
    expect(enemy->getCurrentHealth() == 0.0f, "health: clamps at 0");

    enemy->updateHealth(+999999.0f);
    expect(enemy->getCurrentHealth() == maxHp, "health: clamps at max");

    enemy->updateHealth(-1.0f);
    expect(enemy->getCurrentHealth() == maxHp - 1.0f, "health: subtracts normally");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 4 — EnemyController mechanics (attack selection + damage offsets)
// ─────────────────────────────────────────────────────────────────────────────

static void testControllerStartsAttackFromIdle(const std::string& enemiesJsonPath,
                                               const std::string& charactersJsonPath) {
    CharacterLoader loader = loadCharacters(charactersJsonPath);
    auto players = makePlayersRing(loader, "Percy", 4);

    auto enemy = makeEnemy(enemiesJsonPath, "enemy1");
    if (!enemy) return;

    EnemyController controller;

    enemy->requestState("idle");

    bool leftIdle = false;
    for (int i = 0; i < 40; i++) {
        controller.update(0.5f, enemy, players);
        if (enemy->getCurrentStateName() != "idle") { leftIdle = true; break; }
    }

    expect(leftIdle, "controller: when idle and unlocked, starts an attack state");
}

static void testControllerDoesNotAttackWhenAllPlayersDead(const std::string& enemiesJsonPath,
                                                         const std::string& charactersJsonPath) {
    CharacterLoader loader = loadCharacters(charactersJsonPath);
    auto players = makePlayersRing(loader, "Percy", 4);

    // Kill everyone
    for (auto& p : players) {
        p.updateHealth(-999999.0f);
    }
    expect(!anyPlayersAlive(players), "controller(noLiving): all players confirmed dead");

    auto enemy = makeEnemy(enemiesJsonPath, "enemy1");
    if (!enemy) return;

    EnemyController controller;

    enemy->requestState("idle");

    bool everLeftIdle = false;
    for (int i = 0; i < 40; i++) {
        controller.update(0.5f, enemy, players);
        if (enemy->getCurrentStateName() != "idle") { everLeftIdle = true; break; }
    }

    expect(!everLeftIdle, "controller(noLiving): stays idle (does not start attacks)");
}

static void testControllerDamageEventHitsSomeone(const std::string& enemiesJsonPath,
                                                 const std::string& charactersJsonPath) {
    CharacterLoader loader = loadCharacters(charactersJsonPath);
    auto players = makePlayersRing(loader, "Percy", 4);

    auto enemy = makeEnemy(enemiesJsonPath, "enemy1");
    if (!enemy) return;

    EnemyController controller;

    std::vector<float> before;
    before.reserve(players.size());
    for (auto& p : players) before.push_back(p.getCurrentHealth());

    bool damagedSomeone = false;
    for (int i = 0; i < 240; i++) { // combos + 4s buildUpTime
        controller.update(0.5f, enemy, players);

        for (size_t k = 0; k < players.size(); k++) {
            if (players[k].getCurrentHealth() < before[k]) {
                damagedSomeone = true;
                break;
            }
        }
        if (damagedSomeone) break;
    }

    expect(damagedSomeone, "controller: resolves DAMAGE events and reduces some player's hp");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

void EnemyTests::runAll(const std::string& enemiesJsonPath,
                        const std::string& charactersJsonPath) {
    _passed = 0;
    _failed = 0;

    CULog("═════════════════════════════════════════");
    CULog("  EnemyTests::runAll");
    CULog("═════════════════════════════════════════");
    CULog("  enemiesJsonPath    : %s", enemiesJsonPath.c_str());
    CULog("  charactersJsonPath : %s", charactersJsonPath.c_str());
    CULog("─────────────────────────────────────────");

    CULog("── Section 1: Init ──────────────────────");
    testEnemyInitSetsCoreFields(enemiesJsonPath);

    CULog("── Section 2: State timing ──────────────");
    testEnemyFiresEventsAfterBuildUp(enemiesJsonPath);
    testEnemyCooldownBlocksNonIdle(enemiesJsonPath);

    CULog("── Section 3: Health clamp ──────────────");
    testEnemyHealthClamp(enemiesJsonPath);

    CULog("── Section 4: Controller mechanics ──────");
    testControllerStartsAttackFromIdle(enemiesJsonPath, charactersJsonPath);
    testControllerDoesNotAttackWhenAllPlayersDead(enemiesJsonPath, charactersJsonPath);
    testControllerDamageEventHitsSomeone(enemiesJsonPath, charactersJsonPath);

    printSummary();
}

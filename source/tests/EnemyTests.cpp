// EnemyTests.cpp
// Unit tests for Enemy + EnemyController state flow + event resolution.
//
// HOW TO RUN:
//   Call EnemyTests::runAll() from your app startup AFTER cugl is initialized
//   but BEFORE the game loop starts, e.g. in AppDelegate::onStartup().
//   All results print via CULog as [PASS] or [FAIL]. Remove the call before shipping.
//
//   Example:
//     EnemyTests::runAll("json/enemies.json", "json/houses.json");
//
// ARCHITECTURE NOTES:
//   - Enemy uses a file-scope static EnemyLoader shared across all instances
//   - First test to call makeEnemy() initializes the loader from JSON
//   - Subsequent tests reuse the same loader (no redundant file I/O)
//   - Each test creates fresh Enemy instances for isolation

#include "EnemyTests.h"
#include "../Enemy.h"
#include "../EnemyController.h"
#include "../Player.h"
#include "../HouseLoader.h"
#include <cugl/cugl.h>
#include <fstream>
#include <cstdio>
#include "../EnemyLoader.h"
#include <unordered_set>

namespace {

    int _passed = 0;
    int _failed = 0;

    /**
     * Evaluates a test condition and logs [PASS] or [FAIL].
     * @param condition Boolean test result
     * @param label Description of the test
     */
    void expect(bool condition, const std::string& label) {
        if (condition) {
            CULog("[PASS] %s", label.c_str());
            _passed++;
        }
        else {
            CULog("[FAIL] %s", label.c_str());
            _failed++;
        }
    }

    void printSummary() {
        CULog("─────────────────────────────────────────");
        CULog("  %d passed   %d failed", _passed, _failed);
        CULog("─────────────────────────────────────────");
    }

    HouseLoader loadHouses(const std::string& housesJsonPath) {
        HouseLoader loader;
        if (!loader.loadFromFile(housesJsonPath)) {
            CULogError("EnemyTests: failed to load houses from '%s'", housesJsonPath.c_str());
        }
        return loader;
    }

    std::vector<std::shared_ptr<Player>> makePlayersRing(const HouseLoader& loader,
        const std::string& houseId,
        int count) {
        std::vector<std::shared_ptr<Player>> players;
        players.reserve(count);
        for (int i = 0; i < count; i++) {
            std::string name = "Player " + std::to_string(i + 1);
            players.push_back(std::make_shared<Player>(houseId, i + 1, name, loader));
        }

        const int n = (int)players.size();
        for (int i = 0; i < n; i++) {
            players[i]->setLeftPlayer(players[(i - 1 + n) % n].get());
            players[i]->setRightPlayer(players[(i + 1) % n].get());
        }
        return players;
    }

    std::shared_ptr<Enemy> makeEnemy(const std::string& enemiesJsonPath,
        const std::string& enemyId) {
        // Creates a fresh Enemy instance. The EnemyLoader is shared across all instances
        // via file-scope statics in Enemy.cpp. First call initializes; subsequent calls reuse.
        auto e = std::make_shared<Enemy>();
        bool ok = e->init(enemyId, enemiesJsonPath);
        expect(ok, "Enemy::init succeeds for '" + enemyId + "'");
        return ok ? e : nullptr;
    }

    EnemyLoader::State expectedNextOrIdle(const std::shared_ptr<Enemy>& enemy,
        EnemyLoader::State state) {
        if (!enemy) return EnemyLoader::State::IDLE;
        const auto& states = enemy->getStates();
        auto it = states.find(state);
        if (it == states.end()) return EnemyLoader::State::IDLE;
        EnemyLoader::State next = it->second.nextState;
        if (states.count(next) > 0 && next != EnemyLoader::State::IDLE) return next;
        return EnemyLoader::State::IDLE;
    }

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

    EnemyLoader::State findFirstCooldownStateInChain(const std::shared_ptr<Enemy>& enemy,
        EnemyLoader::State startState) {
        if (!enemy) return EnemyLoader::State::IDLE;
        const auto& states = enemy->getStates();
        if (states.count(startState) == 0) return EnemyLoader::State::IDLE;

        std::unordered_set<EnemyLoader::State> seen;
        EnemyLoader::State cur = startState;

        for (int hops = 0; hops < 32; hops++) {
            if (seen.count(cur)) return EnemyLoader::State::IDLE;
            seen.insert(cur);
            auto it = states.find(cur);
            if (it == states.end()) return EnemyLoader::State::IDLE;
            if (it->second.cooldownTime > 0.0f) return cur;
            EnemyLoader::State next = it->second.nextState;
            if (states.count(next) == 0 || next == EnemyLoader::State::IDLE)
                return EnemyLoader::State::IDLE;
            cur = next;
        }
        return EnemyLoader::State::IDLE;
    }

    bool anyPlayersAlive(const std::vector<std::shared_ptr<Player>>& players) {
        for (const auto& p : players) {
            if (p->isAlive()) return true;
        }
        return false;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 1 — Init
// ─────────────────────────────────────────────────────────────────────────────

static void testEnemyInitSetsCoreFields(const std::string& enemiesJsonPath) {
    auto enemy = std::make_shared<Enemy>();
    bool ok = enemy->init("cyclops", enemiesJsonPath);

    expect(ok, "init: returns true");
    if (!ok) return;

    expect(enemy->getId() == "cyclops", "init: id set");
    expect(enemy->getMaxHealth() > 0.0f, "init: maxHealth > 0");
    expect(enemy->getCurrentHealth() == enemy->getMaxHealth(), "init: currentHealth starts at max");
    expect(enemy->getCurrentState() == EnemyLoader::State::IDLE, "init: starts in idle");
    expect(enemy->getStates().count(EnemyLoader::State::IDLE) > 0, "init: states include idle");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2 — Enemy state timing (buildUp → fire → next + cooldown)
// ─────────────────────────────────────────────────────────────────────────────

static void testEnemyFiresEventsAfterBuildUp(const std::string& enemiesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;

    EnemyLoader::State attack = EnemyLoader::State::ATTACK_1;

    bool entered = enemy->requestState(EnemyLoader::State::ATTACK_1);
    expect(entered, "stateTiming: requestState enters attack");
    expect(enemy->getCurrentState() == EnemyLoader::State::ATTACK_1, "stateTiming: current state is attack");

    enemy->update(0.5f);
    expect(enemy->takeFiredEvents().empty(), "stateTiming: no events before buildUpTime");

    EnemyLoader::State expectedNext = expectedNextOrIdle(enemy, attack);

    std::vector<Enemy::FiredEvent> fired;
    bool didFire = stepUntilFire(enemy, 0.5f, 120, fired);
    expect(didFire, "stateTiming: eventually fires events after buildUpTime");
    if (!didFire) return;

    expect(fired.size() >= 1, "stateTiming: fired >= 1 event");
    expect(enemy->getCurrentState() == expectedNext,
        "stateTiming: transitions to nextState (or idle fallback) after firing");
}

static void testEnemyCooldownBlocksNonIdle(const std::string& enemiesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;

    EnemyLoader::State attack = EnemyLoader::State::ATTACK_1;
    if (attack == EnemyLoader::State::IDLE) { expect(false, "cooldown: missing attack state"); return; }

    EnemyLoader::State cooldownState = findFirstCooldownStateInChain(enemy, attack);
    expect(cooldownState != EnemyLoader::State::IDLE,
        "cooldown: found a cooldownTime>0 state in the attack chain");
    if (cooldownState == EnemyLoader::State::IDLE) return;

    enemy->requestState(attack);

    bool firedCooldownPhase = false;
    for (int phase = 0; phase < 8; phase++) {
        EnemyLoader::State stateThatWillFire = enemy->getCurrentState();
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
    for (int i = 0; i < 120; i++) {
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
    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
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
// SECTION 4 — EnemyController mechanics
// ─────────────────────────────────────────────────────────────────────────────

static void testControllerStartsAttackFromIdle(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    HouseLoader loader = loadHouses(housesJsonPath);
    auto players = makePlayersRing(loader, "poseidon", 4);

    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;

    EnemyController controller;
    enemy->requestState(EnemyLoader::State::IDLE);

    bool leftIdle = false;
    for (int i = 0; i < 40; i++) {
        controller.update(0.5f, enemy, players);
        if (enemy->getCurrentState() != EnemyLoader::State::IDLE) { leftIdle = true; break; }
    }

    expect(leftIdle, "controller: when idle and unlocked, starts an attack state");
}

static void testControllerDoesNotAttackWhenAllPlayersDead(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    HouseLoader loader = loadHouses(housesJsonPath);
    auto players = makePlayersRing(loader, "poseidon", 4);

    for (auto& p : players) {
        p->updateHealth(-999999.0f);
    }
    expect(!anyPlayersAlive(players), "controller(noLiving): all players confirmed dead");

    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;

    EnemyController controller;
    enemy->requestState(EnemyLoader::State::IDLE);

    bool everLeftIdle = false;
    for (int i = 0; i < 40; i++) {
        controller.update(0.5f, enemy, players);
        if (enemy->getCurrentState() != EnemyLoader::State::IDLE) { everLeftIdle = true; break; }
    }

    expect(!everLeftIdle, "controller(noLiving): stays idle (does not start attacks)");
}

static void testControllerDamageEventHitsSomeone(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    HouseLoader loader = loadHouses(housesJsonPath);
    auto players = makePlayersRing(loader, "poseidon", 4);

    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;
    enemy->setDefenseLikelihood(0.0f);

    EnemyController controller;

    std::vector<float> before;
    before.reserve(players.size());
    for (auto& p : players) before.push_back(p->getCurrentHealth());

    bool damagedSomeone = false;
    for (int i = 0; i < 240; i++) {
        controller.update(0.5f, enemy, players);

        for (size_t k = 0; k < players.size(); k++) {
            if (players[k]->getCurrentHealth() < before[k]) {
                damagedSomeone = true;
                break;
            }
        }
        if (damagedSomeone) break;
    }

    expect(damagedSomeone, "controller: resolves DAMAGE events and reduces some player's hp");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 5 — Boss specific mechanics
// ─────────────────────────────────────────────────────────────────────────────

static void testCerberusHealMove(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "cerberus");
    if (!enemy) return;

    // Lower health so there's room to heal
    enemy->updateHealth(-50.0f);
    float healthBeforeHeal = enemy->getCurrentHealth();
    expect(healthBeforeHeal < enemy->getMaxHealth(), "cerberus heal: health lowered before heal");

    EnemyController controller;
    HouseLoader loader = loadHouses(housesJsonPath);
    auto players = makePlayersRing(loader, "poseidon", 4);

    // Force defense so heal fires
    enemy->setDefenseLikelihood(1.0f);

    bool healed = false;
    for (int i = 0; i < 240; i++) {
        controller.update(0.5f, enemy, players);
        if (enemy->getCurrentHealth() > healthBeforeHeal) {
            healed = true;
            break;
        }
    }

    expect(healed, "cerberus heal: health increased after heal move fired");
}

static void testCyclopsMultiplierScalesDamage(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;

    // Hardcode a 2x multiplier on relative side 1
    enemy->setSideMultiplier(1, 2.0f);
    enemy->setTargetIndex(0);

    float healthBefore = enemy->getCurrentHealth();
    float rawDamage = 10.0f;

    // Player at absolute index 1 is relative side 1 from target 0 → should hit for 20
    enemy->takeDamage(rawDamage, 1);

    float actualDamage = healthBefore - enemy->getCurrentHealth();
    expect(std::abs(actualDamage - (rawDamage * 2.0f)) < 0.01f,
        "cyclops multiplier: damage scaled by 2x for side 1");
}

static void testCyclopsDefensiveMove(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    auto enemy = makeEnemy(enemiesJsonPath, "cyclops");
    if (!enemy) return;

    EnemyController controller;
    HouseLoader loader = loadHouses(housesJsonPath);
    auto players = makePlayersRing(loader, "poseidon", 4);

    enemy->setDefenseLikelihood(1.0f);
    enemy->setRetargetLikelihood(0.0f);
    enemy->setTargetIndex(0);

    expect(enemy->getStates().count(EnemyLoader::State::DEFENSE_MOVE) > 0,
        "cyclops: has defense move state");

    // Run for a bit so the passive SIDE_MODIFIER events fire
    for (int i = 0; i < 10; i++) {
        controller.update(0.5f, enemy, players);
    }

    // Direction 0 (facing player) should be 2x
    float mult0 = enemy->getSideMultiplier(0);
    expect(std::abs(mult0 - 2.0f) < 0.01f, "cyclops passive: direction 0 has 2x multiplier");

    float healthBefore0 = enemy->getCurrentHealth();
    float rawDamage = 10.0f;
    enemy->takeDamage(rawDamage, 0);
    float actualDamage0 = healthBefore0 - enemy->getCurrentHealth();
    expect(std::abs(actualDamage0 - (rawDamage * 2.0f)) < 0.01f,
        "cyclops passive: direction 0 takes 2x damage");

    // Direction 3 (behind player) should be 0x — no damage
    float mult3 = enemy->getSideMultiplier(3);
    expect(std::abs(mult3 - 0.0f) < 0.01f, "cyclops passive: direction 3 has 0x multiplier");

    float healthBefore3 = enemy->getCurrentHealth();
    enemy->takeDamage(rawDamage, 3);
    float actualDamage3 = healthBefore3 - enemy->getCurrentHealth();
    expect(std::abs(actualDamage3 - 0.0f) < 0.01f,
        "cyclops passive: direction 3 takes no damage");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

void EnemyTests::runAll(const std::string& enemiesJsonPath,
    const std::string& housesJsonPath) {
    _passed = 0;
    _failed = 0;

    CULog("═════════════════════════════════════════");
    CULog("  EnemyTests::runAll");
    CULog("═════════════════════════════════════════");
    CULog("  enemiesJsonPath : %s", enemiesJsonPath.c_str());
    CULog("  housesJsonPath  : %s", housesJsonPath.c_str());
    CULog("─────────────────────────────────────────");

    CULog("── Section 1: Init ──────────────────────");
    testEnemyInitSetsCoreFields(enemiesJsonPath);

    CULog("── Section 2: State timing ──────────────");
    testEnemyFiresEventsAfterBuildUp(enemiesJsonPath);
    testEnemyCooldownBlocksNonIdle(enemiesJsonPath);

    CULog("── Section 3: Health clamp ──────────────");
    testEnemyHealthClamp(enemiesJsonPath);

    CULog("── Section 4: Controller mechanics ──────");
    testControllerStartsAttackFromIdle(enemiesJsonPath, housesJsonPath);
    testControllerDoesNotAttackWhenAllPlayersDead(enemiesJsonPath, housesJsonPath);
    testControllerDamageEventHitsSomeone(enemiesJsonPath, housesJsonPath);

    CULog("── Section 5: Defensive mechanics ────────────");
    testCerberusHealMove(enemiesJsonPath, housesJsonPath);
    testCyclopsMultiplierScalesDamage(enemiesJsonPath, housesJsonPath);
    testCyclopsDefensiveMove(enemiesJsonPath, housesJsonPath);

    printSummary();
    
    // CRITICAL: Clear static loader state so game can reinitialize with animation metadata
    // Tests used basic init() which doesn't load animations. Game needs to reinit with assets.
    CULog("─────────────────────────────────────────");
    CULog("  Clearing static loader for game init...");
    Enemy::clearStaticLoaderForTesting();
}
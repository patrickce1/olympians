// EnemyController.cpp
#include "EnemyController.h"
#include <algorithm>

using namespace cugl;

/** Clamps a float to between 0 and 1. */
static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

/** Constructor */
EnemyController::EnemyController() {
    _rng.init(); // auto-seeded
}

/** Selects a random index in the range [0, n).  */
int EnemyController::randomIndex(int n) {
    if (n <= 0) return -1;
    // random index in [0, n)
    return (int)(_rng.getUint32() % (Uint32)n);
}

/** Handles wrap-around if i goes over n. */
int EnemyController::wrapIndex(int i, int n) const {
    if (n <= 0) return -1;
    int r = i % n;
    return (r < 0) ? (r + n) : r;
}

/** Validates enemy target is in the range of the number of players. */
void EnemyController::ensureTargetIndexInRange(std::vector<Player>& players) {
    int n = (int)players.size();
    if (n <= 0) {
        _targetIndex = -1;
        return;
    }
    if (_targetIndex < 0 || _targetIndex >= n) {
        _targetIndex = randomIndex(n);
    }
}

/** Upon entering idle state, this function possibly chooses a new target for the enemy. */
void EnemyController::maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy, std::vector<Player>& players) {
    float chance = clamp01(enemy->getRetargetLikelihood());
    if (chance <= 0.0f) return;

    int n = (int)players.size();
    if (n <= 0) { CULog("EnemyController: No players to retarget on idle entry"); return; }

    float r = (float)_rng.getFloat(); // [0,1)
    if (r < chance) {
        _targetIndex = randomIndex(n);
        CULog("EnemyController: switched target on idle entry -> Player[%d]", _targetIndex);
    } else {
        CULog("EnemyController: Retain current target index %d (no retarget)", _targetIndex);
    }
}

/** Checks whether the enemy has just entered idle on this frame. */
void EnemyController::handleIdleEntryIfNeeded(const std::string& prevState, const std::string& curState, const std::shared_ptr<Enemy>& enemy, std::vector<Player>& players) {
    if (curState == "idle" && prevState != "idle") {
        maybeRetargetOnIdleEntry(enemy, players);
    }
}

/** Chooses the next state tagged with "attack" for the enemy to enter. */
std::string EnemyController::chooseNextAttackState(const std::shared_ptr<Enemy>& enemy) {
    std::vector<std::string> attacks;

    const auto& states = enemy->getStates();
    for (const auto& pair : states) {
        const std::string& name = pair.first;
        const auto& def = pair.second;

        if (name == "idle") continue;
        if (def.tag == "attack") {
            attacks.push_back(name);
        }
    }

    if (attacks.empty()) { CULog("EnemyController: No attack states available"); return ""; }

    int idx = (int)(_rng.getUint32() % (Uint32)attacks.size());
    CULog("EnemyController: Chose attack '%s' (idx=%d)", attacks[idx].c_str(), idx);
    return attacks[idx];
}

void EnemyController::enterIdle(const std::shared_ptr<Enemy>& enemy, std::vector<Player>& players) {
    CULog("EnemyController: Forcing enemy '%s' to idle", enemy->getId().c_str());
    enemy->requestState("idle");

    ensureTargetIndexInRange(players);
    maybeRetargetOnIdleEntry(enemy, players);
}

/** Main update loop for enemy controller. Handles state changes and attack events. */
void EnemyController::update(float dt, const std::shared_ptr<Enemy>& enemy, std::vector<Player>& players) {

    std::string prev = enemy->getCurrentStateName();

    enemy->update(dt);

    auto events = enemy->takeFiredEvents();
    if (!events.empty()) {
        resolveEnemyEvents(enemy, players, events);
    }
    
    std::string cur = enemy->getCurrentStateName();
    if (cur != prev) { CULog("EnemyController: State changed '%s' -> '%s'", prev.c_str(), cur.c_str()); }

    ensureTargetIndexInRange(players);
    handleIdleEntryIfNeeded(prev, cur, enemy, players);

    // If idle and not locked out, pick an attack by tag and start it
    if (cur == "idle" && enemy->canStartNonIdleState()) {
        std::string nextAttack = chooseNextAttackState(enemy);
        if (!nextAttack.empty()) {
            enemy->requestState(nextAttack);
            cur = enemy->getCurrentStateName();
        }
    }
}

/** Resolves the fired events (if any) of the enemy on this frame. Removes the processed events from the buffer. */
void EnemyController::resolveEnemyEvents(const std::shared_ptr<Enemy>& enemy, std::vector<Player>& players, const std::vector<Enemy::FiredEvent>& events) {
    CULog("EnemyController: Resolving %zu events for enemy '%s'", events.size(), enemy->getId().c_str());
    for (const auto& fe : events) {
        switch (fe.def.type) {
            case EnemyLoader::EventType::DAMAGE:
                resolveDamageEvent(enemy, players, fe);
                break;
            default:
                CULog("EnemyController: Unhandled event type in state '%s' for enemy '%s'", fe.stateName.c_str(), enemy->getId().c_str());
                break;
        }
    }
}

/** Deals damage to the targeted players from a damage event. */
void EnemyController::resolveDamageEvent(const std::shared_ptr<Enemy>& enemy, std::vector<Player>& players, const Enemy::FiredEvent& fe) {

    int n = (int)players.size();
    if (n <= 0) {
        CULog("EnemyController: DAMAGE fired but players list is empty");
        return;
    }

    ensureTargetIndexInRange(players);

    int offset = fe.def.target; // int offset from JSON
    int victim = wrapIndex(_targetIndex + offset, n);

    float damage = fe.def.amount;
    players[victim].updateHealth(-damage);

    CULog("EnemyController: Enemy '%s' state '%s' DAMAGE %.2f -> Player[%d] (target=%d offset=%d)",
          enemy->getId().c_str(),
          fe.stateName.c_str(),
          damage,
          victim,
          _targetIndex,
          offset);
}

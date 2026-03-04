// EnemyController.cpp
#include "EnemyController.h"
#include <algorithm>

using namespace cugl;

/** Constructor */
EnemyController::EnemyController() {
    _rng.init(); // auto-seeded
}

/** Handles wrap-around if i goes over n. */
int EnemyController::wrapIndex(int i, int n) const {
    if (n <= 0) return -1;
    int r = i % n;
    return (r < 0) ? (r + n) : r;
}

/** Returns true if there are any living players. */
bool anyPlayersAlive(const std::vector<Player>& players) {
    for (const auto& p : players) {
        if (p.isAlive()) return true;
    }
    return false;
}

/** Upon entering idle state, this function possibly chooses a new target for the enemy. */
void EnemyController::maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy, std::vector<Player>& players) {
    float chance = enemy->getRetargetLikelihood();
    if (chance <= 0.0f) return;
    if (chance > 1.0f) chance = 1.0f;

    const int n = (int)players.size();
    if (n <= 0) {
        CULog("EnemyController: No players to retarget on idle entry");
        return;
    }
    std::vector<int> living;
    living.reserve(n);
    for (int i = 0; i < n; i++) {
        if (players[i].isAlive()) living.push_back(i);
    }

    // BASE CASE: All players dead
    if (living.empty()) {
        CULog("EnemyController: All players dead; cannot retarget");
        _targetIndex = -1;
        return;
    }

    // BASE CASE: If current target is dead or invalid, force it onto a living target (no probability)
    bool curValid = (_targetIndex >= 0 && _targetIndex < n && players[_targetIndex].isAlive());
    if (!curValid) {
        const int pick = (int)(_rng.getUint32() % (Uint32)living.size());
        CULog("EnemyController: current target Player[%d] is invalid/dead -> forced to Player[%d]", _targetIndex, living[pick]);
        _targetIndex = living[pick];
        return;
    }

    // Roll probability on whether to switch to a different target
    float r = (float)_rng.getFloat(); // [0,1)
    if (r >= chance) {
        CULog("EnemyController: Retain current target index %d (no retarget)", _targetIndex);
        return;
    }
    
    // If there are valid candidates choose one, otherwise keep the same target
    std::vector<int> candidates;
    candidates.reserve(living.size());
    for (int idx : living) {
        if (idx != _targetIndex) candidates.push_back(idx);
    }
    if (candidates.empty()) {
        CULog("EnemyController: Only one living target (Player[%d]); cannot switch", _targetIndex);
        return;
    }
    const int pick = (int)(_rng.getUint32() % (Uint32)candidates.size());
    CULog("EnemyController: switched target on idle entry from Player[%d] -> Player[%d]", _targetIndex, candidates[pick]);
    _targetIndex = candidates[pick];
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

    handleIdleEntryIfNeeded(prev, cur, enemy, players);

    // If idle and not locked out, pick an attack by tag and start it
    if (cur == "idle" && enemy->canStartNonIdleState() && anyPlayersAlive(players)) {
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

    int offset = fe.def.target; // int offset from JSON
    int victim = wrapIndex(_targetIndex + offset, n);
    
    // Victim was killed before event completed
    if (!players[victim].isAlive()) {
        CULog("EnemyController: Enemy '%s' state '%s' Player[%d] was already dead (target=%d offset=%d)",
              enemy->getId().c_str(),
              fe.stateName.c_str(),
              victim,
              _targetIndex,
              offset);
    } else {
        float damage = fe.def.amount;
        players[victim].updateHealth(-damage);

        CULog("EnemyController: Enemy '%s' state '%s' DAMAGE %.2f -> Player[%d] Health %.2f (target=%d offset=%d)",
              enemy->getId().c_str(),
              fe.stateName.c_str(),
              damage,
              victim,
              players[victim].getCurrentHealth(),
              _targetIndex,
              offset);
    }
}

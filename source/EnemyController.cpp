// EnemyController.cpp
#include "EnemyController.h"
#include "scenes/GameScene.h"
#include <algorithm>

using namespace cugl;

/** Constructor */
EnemyController::EnemyController() {
    _rng.init(); // auto-seeded
}

/** Calculates which direction (0-3) an enemy should face relative to a local player. 
 *  @param targetIndex The index of the player the enemy is targeting (0-3)
 *  @param localPlayerIndex The local player's index (0-3)
*/
int EnemyController::calculateDirection(int targetIndex, int localPlayerIndex) {
    // Wrap indices to valid range [0-3]
    int target = (targetIndex + 4) % 4;
    int local = (localPlayerIndex + 4) % 4;
    
    // Calculate relative offset from local player to target
    int direction = (target - local + 4) % 4;
    
    // Map offset to sprite row:
    // 0 = forward, 1 = right, 2 = back, 3 = left
    return direction;
}

/** Handles wrap-around if i goes over n. */
int EnemyController::wrapIndex(int i, int n) const {
    if (n <= 0) return -1;
    int r = i % n;
    return (r < 0) ? (r + n) : r;
}

/** Returns true if there are any living players. */
bool anyPlayersAlive(const std::vector<std::shared_ptr<Player>>& players) {
    for (const auto& p : players) {
        if (p->isAlive()) return true;
    }
    return false;
}

/** Upon entering idle state, this function possibly chooses a new target for the enemy. */
void EnemyController::maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy, std::vector<std::shared_ptr<Player>>& players) {
    // Don't retarget if enemy is currently stunned or loved.
    if (enemy->isStunned()) {
        CULog("[EnemyController] Target: retarget skipped because enemy '%s' is stunned", enemy->getId().c_str());
        return;
    }
    if (enemy->isLoved()) {
        CULog("[EnemyController] Target: retarget skipped because enemy '%s' is loved", enemy->getId().c_str());
        return;
    }

    // Don't retarget if currently in attack phase (prevent interruptions during active attacks)
    if (_animationRegistry && enemy->isInAttackPhase(*_animationRegistry)) {
        if (_debug) CULog("[EnemyController] Target: Player[%d] (Retained: in attack phase)", enemy->getTargetIndex());
        return;
    }
    
    float chance = enemy->getRetargetLikelihood();
    if (chance <= 0.0f) return;
    if (chance > 1.0f) chance = 1.0f;

    const int n = (int)players.size();
    if (n <= 0) {
        if (_debug) CULog("[EnemyController] Target: No players on idle entry");
        return;
    }
    std::vector<int> living;
    living.reserve(n);
    for (int i = 0; i < n; i++) {
        if (players[i]->isAlive()) living.push_back(i);
    }

    // BASE CASE: All players dead
    if (living.empty()) {
        if (_debug) CULog("[EnemyController] Target: None (All players dead)");
        enemy->setTargetIndex(-1);
        return;
    }

    // BASE CASE: If current target is dead or invalid, force it onto a living target (no probability)
    bool curValid = (enemy->getTargetIndex() >= 0 && enemy->getTargetIndex() < n && players[enemy->getTargetIndex()]->isAlive());
    if (!curValid) {
        const int pick = (int)(_rng.getUint32() % (Uint32)living.size());
        if (_debug) CULog("[EnemyController] Target: Player[%d] -> Player[%d] (Current target was invalid/dead)", enemy->getTargetIndex(), living[pick]);
        enemy->setTargetIndex(living[pick]);
        return;
    }

    // Roll probability on whether to switch to a different target
    float r = (float)_rng.getFloat(); // [0,1)
    if (r >= chance) {
        if (_debug) CULog("[EnemyController] Target: Player[%d] (Retained original target)", enemy->getTargetIndex());
        return;
    }
    
    // If there are valid candidates choose one, otherwise keep the same target
    std::vector<int> candidates;
    candidates.reserve(living.size());
    for (int idx : living) {
        if (idx != enemy->getTargetIndex()) candidates.push_back(idx);
    }
    if (candidates.empty()) {
        if (_debug) CULog("[EnemyController] Target: Player[%d] (Only living player)", enemy->getTargetIndex());
        return;
    }
    const int pick = (int)(_rng.getUint32() % (Uint32)candidates.size());
    if (_debug) CULog("[EnemyController] Target: Player[%d] -> Player[%d] (Retargeted on idle entry)", enemy->getTargetIndex(), candidates[pick]);
    enemy->setTargetIndex(candidates[pick]);
}

/** Checks whether the enemy has just entered idle on this frame. */
void EnemyController::handleIdleEntryIfNeeded(EnemyLoader::State prevState, EnemyLoader::State curState, const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players) {
    if (curState == EnemyLoader::State::IDLE && prevState != EnemyLoader::State::IDLE) {
        // Don't retarget immediately — hold current facing and turn after a short delay
        _pendingRetarget = true;
        _retargetTimer = IDLE_RETARGET_DELAY;
    } else if (curState != EnemyLoader::State::IDLE) {
        // Left idle before the timer fired — cancel
        _pendingRetarget = false;
    }
}

/** Chooses the next state tagged with "attack" for the enemy to enter. */
EnemyLoader::State EnemyController::chooseNextAttackState(const std::shared_ptr<Enemy>& enemy) {
    std::vector<EnemyLoader::State> attacks;

    attacks.push_back(EnemyLoader::State::ATTACK_1);
    attacks.push_back(EnemyLoader::State::ATTACK_2);
    attacks.push_back(EnemyLoader::State::ATTACK_3);

    if (attacks.empty()) { if (_debug) CULog("[EnemyController] Attack: No attack states available"); return EnemyLoader::State::IDLE; }

    int idx = (int)(_rng.getUint32() % (Uint32)attacks.size());
    EnemyLoader::State selectedAttack = attacks[idx];
    if (_debug) CULog("[EnemyController] State: '%s' (Attack)", enemy->getStates().at(selectedAttack).name.c_str());
    return selectedAttack;
}

void EnemyController::enterIdle(const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players) {
    if (_debug) CULog("[EnemyController] State: '%s' (Idle)", enemy->getId().c_str());
    enemy->requestState(EnemyLoader::State::IDLE);
    _pendingRetarget = true;
    _retargetTimer = IDLE_RETARGET_DELAY;
}

/** Main update loop for enemy controller. Handles state changes and attack events. */
void EnemyController::update(float dt, const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players) {

    EnemyLoader::State prev = enemy->getCurrentState();
    
    enemy->update(dt);

    auto events = enemy->takeFiredEvents();
    if (!events.empty()) {
        resolveEnemyEvents(enemy, players, events);
    }
    
    EnemyLoader::State cur = enemy->getCurrentState();
    if (cur != prev) { if (_debug) CULog("[EnemyController] State: '%s' -> '%s'", enemy->getStates().at(prev).name.c_str(), enemy->getStates().at(cur).name.c_str()); }

    handleIdleEntryIfNeeded(prev, cur, enemy, players);

    // Tick deferred retarget timer
    if (_pendingRetarget && cur == EnemyLoader::State::IDLE) {
        _retargetTimer -= dt;
        if (_retargetTimer <= 0.0f) {
            _pendingRetarget = false;
            maybeRetargetOnIdleEntry(enemy, players);
        }
    }

    // If idle and not locked out, pick an attack by tag and start it
    if (cur == EnemyLoader::State::IDLE && enemy->canStartNonIdleState() && anyPlayersAlive(players)) {
        if (shouldDefend(enemy)) {
            enemy->requestState(EnemyLoader::State::DEFENSE_MOVE);
        }
        else {
            EnemyLoader::State nextAttack = chooseNextAttackState(enemy);
            enemy->requestState(nextAttack);
            cur = enemy->getCurrentState();
        }
    }
}

/** Resolves the fired events (if any) of the enemy on this frame. Removes the processed events from the buffer. */
void EnemyController::resolveEnemyEvents(const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players, const std::vector<Enemy::FiredEvent>& events) {
    for (const auto& event : events) {
        switch (event.def.type) {
            case EnemyLoader::EventType::DAMAGE:
                resolveDamageEvent(enemy, players, event);
                break;
            case EnemyLoader::EventType::SIDE_MODIFIER:
                resolveSideMultiplierEvent(enemy, event);
                break;
            case EnemyLoader::EventType::HEAL:
                resolveHealEvent(enemy, event);
                break;
            default:
                if (_debug) CULog("[EnemyController] Event: Unhandled event type in state '%s' for enemy '%s'", enemy->getStates().at(event.state).name.c_str(), enemy->getId().c_str());
                break;
        }
    }
}

/** Deals damage to the targeted players from a damage event. */
void EnemyController::resolveDamageEvent(const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players, const Enemy::FiredEvent& fe) {

    int n = (int)players.size();
    if (n <= 0) {
        if (_debug) CULog("[EnemyController] Event: DAMAGE fired but players list is empty");
        return;
    }

    int offset = fe.def.target; // int offset from JSON
    int victim = wrapIndex(enemy->getTargetIndex() + offset, n);
    
    // Victim was killed before event completed
    if (!players[victim]->isAlive()) {
        if (_debug) CULog("[EnemyController] Event: Enemy '%s', state '%s', Player[%d] was already dead",
              enemy->getId().c_str(),
              enemy->getStates().at(fe.state).name.c_str(),
              victim);
    } else {
        float damage = fe.def.amount;
        players[victim]->updateHealth(-damage);

        if (_debug) CULog("[EnemyController] Event: Enemy '%s', state '%s', DAMAGE %.1f, Player[%d] Health -> %.1f",
              enemy->getId().c_str(),
              enemy->getStates().at(fe.state).name.c_str(),
              damage,
              victim,
              players[victim]->getCurrentHealth());
    }
}

/** Applies side modifiers to the boss based on a side modifier event 
 * @param enemy points to the enemy whose side data is being changed
 * @param event is event that was fired by the enemy AI that is meant to change the side data
*/
void EnemyController::resolveSideMultiplierEvent(const std::shared_ptr<Enemy>& enemy, const Enemy::FiredEvent& event) {
    enemy->setSideMultiplier(event.def.target, event.def.amount);
}

/** Applies a heal to the boss based on a heal event 
 * @param enemy points to the enemy that is being healed
 * @param event is event that was fired by the enemy AI that is meant to heal the boss
*/
void EnemyController::resolveHealEvent(const std::shared_ptr<Enemy>& enemy, const Enemy::FiredEvent& event) {
    //we can only heal if we haven't died yet
    if (enemy->getCurrentHealth() > 0) {
        enemy->updateHealth(event.def.amount);
    }
}

/** 
 * Determines whether the boss should enter a defensive state.
 * Returns true if a random chance roll succeeds, or if the enemy's defensive condition is met.
 * @param enemy the enemy used to evaluate whether the defense condition applies
 */
bool EnemyController::shouldDefend(const std::shared_ptr<Enemy>& enemy) {
    float random = _rng.getClosedFloat(0, 1);
    if (random <= enemy->getDefenseLikelihood()) {
        return true;
    }
    return enemy->shouldDefend();
}

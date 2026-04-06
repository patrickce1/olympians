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
bool anyPlayersAlive(const std::vector<std::shared_ptr<Player>>& players) {
    for (const auto& p : players) {
        if (p->isAlive()) return true;
    }
    return false;
}

/** Upon entering idle state, this function possibly chooses a new target for the enemy. */
void EnemyController::maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy, std::vector<std::shared_ptr<Player>>& players) {
    float chance = enemy->getRetargetLikelihood();
    if (chance <= 0.0f) return;
    if (chance > 1.0f) chance = 1.0f;

    const int n = (int)players.size();
    if (n <= 0) {
        CULog("[EnemyController] Target: No players on idle entry");
        return;
    }
    std::vector<int> living;
    living.reserve(n);
    for (int i = 0; i < n; i++) {
        if (players[i]->isAlive()) living.push_back(i);
    }

    // BASE CASE: All players dead
    if (living.empty()) {
        CULog("[EnemyController] Target: None (All players dead)");
        enemy->setTargetIndex(-1);
        return;
    }

    // BASE CASE: If current target is dead or invalid, force it onto a living target (no probability)
    bool curValid = (enemy->getTargetIndex() >= 0 && enemy->getTargetIndex() < n && players[enemy->getTargetIndex()]->isAlive());
    if (!curValid) {
        const int pick = (int)(_rng.getUint32() % (Uint32)living.size());
        CULog("[EnemyController] Target: Player[%d] -> Player[%d] (Current target was invalid/dead)", enemy->getTargetIndex(), living[pick]);
        enemy->setTargetIndex(living[pick]);
        return;
    }

    // Roll probability on whether to switch to a different target
    float r = (float)_rng.getFloat(); // [0,1)
    if (r >= chance) {
        CULog("[EnemyController] Target: Player[%d] (Retained original target)", enemy->getTargetIndex());
        return;
    }
    
    // If there are valid candidates choose one, otherwise keep the same target
    std::vector<int> candidates;
    candidates.reserve(living.size());
    for (int idx : living) {
        if (idx != enemy->getTargetIndex()) candidates.push_back(idx);
    }
    if (candidates.empty()) {
        CULog("[EnemyController] Target: Player[%d] (Only living player)", enemy->getTargetIndex());
        return;
    }
    const int pick = (int)(_rng.getUint32() % (Uint32)candidates.size());
    CULog("[EnemyController] Target: Player[%d] -> Player[%d] (Retargeted on idle entry)", enemy->getTargetIndex(), candidates[pick]);
    enemy->setTargetIndex(candidates[pick]);
}

/** Checks whether the enemy has just entered idle on this frame. */
void EnemyController::handleIdleEntryIfNeeded(EnemyLoader::State prevState, EnemyLoader::State curState, const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players) {
    if (curState == EnemyLoader::State::IDLE && prevState != EnemyLoader::State::IDLE) {
        maybeRetargetOnIdleEntry(enemy, players);
    }
}

/** Chooses the next state tagged with "attack" for the enemy to enter. */
EnemyLoader::State EnemyController::chooseNextAttackState(const std::shared_ptr<Enemy>& enemy) {
    std::vector<EnemyLoader::State> attacks;

    const auto& states = enemy->getStates();
    for (const auto& pair : states) {
        EnemyLoader::State state = pair.first;
        const auto& def = pair.second;

        if (def.tag == "attack") {
            attacks.push_back(state);
        }
    }

    if (attacks.empty()) { CULog("[EnemyController] Attack: No attack states available"); return EnemyLoader::State::IDLE; }

    int idx = (int)(_rng.getUint32() % (Uint32)attacks.size());
    CULog("[EnemyController] State: '%d' (Attack)", attacks[idx]);
    return attacks[idx];
}

void EnemyController::enterIdle(const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players) {
    CULog("[EnemyController] State: '%s' (Idle)", enemy->getId().c_str());
    enemy->requestState(EnemyLoader::State::IDLE);
    maybeRetargetOnIdleEntry(enemy, players);
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
    if (cur != prev) { CULog("[EnemyController] State: '%d' -> '%d'", prev, cur); }

    handleIdleEntryIfNeeded(prev, cur, enemy, players);

    // If idle and not locked out, pick an attack by tag and start it
    if (cur == EnemyLoader::State::IDLE && enemy->canStartNonIdleState() && anyPlayersAlive(players)) {
        if (shouldDefend(enemy)) {
            enemy->requestState(EnemyLoader::State::DEFENSE_MOVE);
        }
        else {
            EnemyLoader::State nextAttack = chooseNextAttackState(enemy);
            //TODO: figure out a guard here. Before was !nextAttack.empty()
            if (true) {
                enemy->requestState(nextAttack);
                cur = enemy->getCurrentState();
            }
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
            case EnemyLoader::EventType::HEAL:
                resolveHealEvent(enemy, event);
            default:
                CULog("[EnemyController] Event: Unhandled event type in state '%d' for enemy '%s'", event.state, enemy->getId().c_str());
                break;
        }
    }
}

/** Deals damage to the targeted players from a damage event. */
void EnemyController::resolveDamageEvent(const std::shared_ptr<Enemy>& enemy, std::vector<std::shared_ptr<Player>>& players, const Enemy::FiredEvent& fe) {

    int n = (int)players.size();
    if (n <= 0) {
        CULog("[EnemyController] Event: DAMAGE fired but players list is empty");
        return;
    }

    int offset = fe.def.target; // int offset from JSON
    int victim = wrapIndex(enemy->getTargetIndex() + offset, n);
    
    // Victim was killed before event completed
    if (!players[victim]->isAlive()) {
        CULog("[EnemyController] Event: Enemy '%s', state '%d', Player[%d] was already dead",
              enemy->getId().c_str(),
              fe.state,
              victim);
    } else {
        float damage = fe.def.amount;
        players[victim]->updateHealth(-damage);

        CULog("[EnemyController] Event: Enemy '%s', state '%d', DAMAGE %.1f, Player[%d] Health -> %.1f",
              enemy->getId().c_str(),
              fe.state,
              damage,
              victim,
              players[victim]->getCurrentHealth());
    }
}

void EnemyController::resolveSideMultiplierEvent(const std::shared_ptr<Enemy>& enemy, const Enemy::FiredEvent& event) {
    enemy->setSideMultiplier(event.def.target, event.def.amount);
}

void EnemyController::resolveHealEvent(const std::shared_ptr<Enemy>& enemy, const Enemy::FiredEvent& event) {
    //we can only heal if we haven't died yet
    if (enemy->getCurrentHealth() > 0) {
        enemy->updateHealth(event.def.amount);
    }
}

bool EnemyController::shouldDefend(const std::shared_ptr<Enemy>& enemy) {
    float random = _rng.getClosedFloat(0, 1);
    if (random <= enemy->getDefenseLikelihood()) {
        return true;
    }
    return enemy->shouldDefend();
}
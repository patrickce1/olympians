// EnemyController.cpp
#include "EnemyController.h"
#include <algorithm>

using namespace cugl;

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

EnemyController::EnemyController() {
    _rng.init(); // auto-seeded
}

int EnemyController::randomIndex(int n) {
    if (n <= 0) return -1;
    return (int)(_rng.getUint32() % (Uint32)n);
}

int EnemyController::wrapIndex(int i, int n) const {
    if (n <= 0) return -1;
    int r = i % n;
    return (r < 0) ? (r + n) : r;
}

void EnemyController::ensureTargetIndexInRange(const std::vector<Player>& players) {
    int n = (int)players.size();
    if (n <= 0) {
        _targetIndex = -1;
        return;
    }
    if (_targetIndex < 0 || _targetIndex >= n) {
        _targetIndex = randomIndex(n);
    }
}

void EnemyController::maybeRetargetOnIdleEntry(const std::vector<Player>& players) {
    float chance = clamp01(_targetSwitchChanceOnIdle);
    if (chance <= 0.0f) return;

    int n = (int)players.size();
    if (n <= 0) return;

    float r = (float)_rng.getFloat(); // [0,1)
    if (r < chance) {
        _targetIndex = randomIndex(n);
        CULog("EnemyController: switched target on idle entry -> Player[%d]", _targetIndex);
    }
}

void EnemyController::handleIdleEntryIfNeeded(const std::string& prevState, const std::string& curState, const std::vector<Player>& players) {
    if (curState == "idle" && prevState != "idle") {
        maybeRetargetOnIdleEntry(players);
    }
}

std::string EnemyController::chooseNextAttackState(std::shared_ptr<Enemy>& enemy) {
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

    if (attacks.empty()) return "";

    int idx = (int)(_rng.getUint32() % (Uint32)attacks.size());
    return attacks[idx];
}

void EnemyController::enterIdle(std::shared_ptr<Enemy>& enemy, std::vector<Player>& players) {
    enemy->requestState("idle");
    _lastStateSeen = "__forced_idle__";

    ensureTargetIndexInRange(players);
    maybeRetargetOnIdleEntry(players);
}

void EnemyController::update(float dt, std::shared_ptr<Enemy>& enemy, std::vector<Player>& players) {
    std::string prev = enemy->getCurrentStateName();

    enemy->update(dt);

    std::string cur = enemy->getCurrentStateName();

    ensureTargetIndexInRange(players);
    handleIdleEntryIfNeeded(prev, cur, players);

    // If idle and not locked out, pick an attack by tag and start it
    if (cur == "idle" && enemy->canStartNonIdleState()) {
        std::string nextAttack = chooseNextAttackState(enemy);
        if (!nextAttack.empty()) {
            enemy->requestState(nextAttack);
            cur = enemy->getCurrentStateName();
        }
    }

    auto events = enemy->takeFiredEvents();
    if (!events.empty()) {
        resolveEnemyEvents(enemy, players, events);
    }

    _lastStateSeen = enemy->getCurrentStateName();
}

void EnemyController::resolveEnemyEvents(std::shared_ptr<Enemy>& enemy, std::vector<Player>& players, const std::vector<Enemy::FiredEvent>& events) {
    for (const auto& fe : events) {
        switch (fe.def.type) {
            case EnemyLoader::EventType::DAMAGE:
                resolveDamageEvent(enemy, players, fe);
                break;
            default:
                CULog("EnemyController: Unhandled event type in state '%s'", fe.stateName.c_str());
                break;
        }
    }
}

void EnemyController::resolveDamageEvent(std::shared_ptr<Enemy>& enemy, std::vector<Player>& players, const Enemy::FiredEvent& fe) {
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

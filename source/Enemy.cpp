#include "Enemy.h"
#include <algorithm>

static float clampMinZero(float v) { return v < 0.0f ? 0.0f : v; }

Enemy::Enemy(const std::string& enemyId, const EnemyLoader& loader) {
    CUAssertLog(loader.has(enemyId), "Enemy ID not found: %s", enemyId.c_str());
    const EnemyLoader::EnemyDef& def = loader.get(enemyId);

    _enemyId = def.id;
    _name = def.name;
    _spritesheetPath = def.spritesheetPath;

    _maxHealth = def.maxHealth;
    _currentHealth = def.maxHealth;

    _shields = def.defaultShields;
    _states = def.states;

    CUAssertLog(_states.count("idle") > 0, "Enemy '%s' must define 'idle'", _enemyId.c_str()); // assert there is an idle state

    // always start idle
    enterState("idle");
    _attackLockout = 0.0f;
    _facing = Facing::DOWN;
}

const EnemyLoader::StateDef* Enemy::getCurrentStateDef() const {
    auto it = _states.find(_currentState);
    if (it == _states.end()) return nullptr;
    return &it->second;
}

bool Enemy::requestState(const std::string& stateName) {
    if (_states.count(stateName) == 0) return false;

    // While lockout is active, only allow idle.
    if (_attackLockout > 0.0f && stateName != "idle") return false;

    enterState(stateName);
    return true;
}

void Enemy::enterState(const std::string& stateName) {
    _currentState = stateName;
    _stateTime = 0.0f;
    _eventsFiredThisState = false;
}

void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;

    _stateTime += dt;
    _attackLockout = clampMinZero(_attackLockout - dt);
}

bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return false;
    if (_eventsFiredThisState) return false;
    return _stateTime >= st->buildUpTime;
}

void Enemy::fireEvents() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;

    for (const auto& ev : st->events) {
        FiredEvent fe;
        fe.def = ev;
        fe.target = ev.target;
        fe.facingAtFire = _facing;
        fe.stateName = st->name;
        _firedEvents.push_back(fe);
    }

    _eventsFiredThisState = true;
}

void Enemy::applyCooldown() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;

    _attackLockout = std::max(_attackLockout, st->cooldownTime);
}

std::string Enemy::getNextStateOrIdle() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return "idle";

    if (!st->nextState.empty() && _states.count(st->nextState) > 0)
        return st->nextState;

    return "idle";
}

void Enemy::transitionToNextState() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;
    enterState(st->nextState);
}

void Enemy::update(float dt) {

    tick(dt);

    if (readyToFire()) {
        fireEvents();
        applyCooldown();
        enterState(getNextStateOrIdle());
    }
}

std::vector<Enemy::FiredEvent> Enemy::takeFiredEvents() {
    std::vector<FiredEvent> out;
    out.swap(_firedEvents);
    return out;
}

void Enemy::applyDamage(float amount) {
    if (amount <= 0.0f) return;
    _currentHealth -= amount;
    if (_currentHealth < 0.0f) _currentHealth = 0.0f;
}

void Enemy::heal(float amount) {
    if (amount <= 0.0f) return;
    _currentHealth += amount;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
}

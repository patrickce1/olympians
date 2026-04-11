// Enemy.h
#ifndef __ENEMY_H__
#define __ENEMY_H__

#include <string>
#include <unordered_map>
#include <vector>
#include "EnemyLoader.h"

/**
 * Enemy runtime instance.
 * - Holds health + current state
 * - Runs state timing (buildUp -> fire events once -> nextState -> cooldown lockout)
 * - Queues fired events for EnemyController to resolve onto players
 *
 * Enemy does NOT choose behavior. External code calls requestState().
 */
class Enemy {
public:
    struct FiredEvent {
        EnemyLoader::EventDef def;   // type, target offset, amount, etc.
        std::string stateName;       // state that fired this event (debug)
    };

private:
    std::string _enemyId;
    std::string _name;
    std::string _spritesheetPath;

    float _maxHealth = 0.0f;
    float _currentHealth = 0.0f;

    std::unordered_map<std::string, EnemyLoader::StateDef> _states;

    std::string _currentState = "idle";
    float _stateTime = 0.0f;
    bool _eventsFiredThisState = false;

    // Blocks starting non-idle states while > 0
    float _attackLockout = 0.0f;
    float _retargetLikelihood = 0.0f;
    /** Remaining stun time in seconds. While positive, enemy attacks and retargeting are disabled. */
    float _stunDuration = 0.0f;

    std::vector<FiredEvent> _firedEvents;

public:
    Enemy() = default;
    
    bool init(const std::string& enemyId, const std::string& jsonPath);

    const std::string& getId() const { return _enemyId; }
    const std::string& getName() const { return _name; }
    const std::string& getSpritesheetPath() const { return _spritesheetPath; }

    float getMaxHealth() const { return _maxHealth; }
    float getCurrentHealth() const { return _currentHealth; }
    bool isAlive() const { return _currentHealth > 0.0f; }
    void setCurrentHealth(float health) { _currentHealth = health; }

    const std::string& getCurrentStateName() const { return _currentState; }
    const EnemyLoader::StateDef* getCurrentStateDef() const;

    float getAttackLockoutRemaining() const { return _attackLockout; }
    bool canStartNonIdleState() const { return _attackLockout <= 0.0f && !isStunned(); }
    float getRetargetLikelihood() const { return _retargetLikelihood; }
    void  setRetargetLikelihood(float v);
    /** Returns whether the enemy is currently stunned. */
    bool isStunned() const { return _stunDuration > 0.0f; }
    /** Returns the remaining stun duration in seconds. */
    float getStunDuration() const { return _stunDuration; }
    /** Applies or refreshes a stun, forcing the enemy idle and extending the remaining duration. */
    void applyStun(float duration);
    /** Overwrites local stun time from the host snapshot so remote clients mirror the authoritative state. */
    void syncStunDuration(float duration);
    
    // Expose state defs so controller can pick attacks by tag
    const std::unordered_map<std::string, EnemyLoader::StateDef>& getStates() const { return _states; }

    bool requestState(const std::string& stateName);
    void update(float dt);

    std::vector<FiredEvent> takeFiredEvents();

    // Positive heals, negative damages; clamps to [0, maxHealth]
    void updateHealth(float delta);

private:
    void enterState(const std::string& stateName);
    void tick(float dt);
    bool readyToFire() const;
    void fireEvents();
    void applyCooldown();
    std::string getNextStateOrIdle() const;
    /** Forces the enemy back to idle immediately, clearing the current state's progress. */
    void forceIdle();
};

#endif /* !__ENEMY_H__ */

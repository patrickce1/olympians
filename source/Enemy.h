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
    /** Remaining vulnerable time in seconds. While positive, incoming damage is multiplied. */
    float _vulnerableDuration = 0.0f;
    /** Active incoming damage multiplier while the enemy is vulnerable. */
    float _vulnerableMultiplier = 1.0f;

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
    /**
     * Applies or refreshes a stun, forcing the enemy idle and extending the remaining duration.
     *
     * @param duration  The stun time to apply, in seconds.
     */
    void applyStun(float duration);
    /**
     * Overwrites local stun time from the host snapshot so remote clients mirror the authoritative state.
     *
     * @param duration  The authoritative remaining stun time, in seconds.
     */
    void syncStunDuration(float duration);
    /** Returns whether the enemy is currently vulnerable. */
    bool isVulnerable() const { return _vulnerableDuration > 0.0f; }
    /** Returns the remaining vulnerable duration in seconds. */
    float getVulnerableDuration() const { return _vulnerableDuration; }
    /** Returns the current damage multiplier applied while vulnerable. */
    float getVulnerableMultiplier() const { return _vulnerableMultiplier; }
    /**
     * Applies a local authoritative vulnerability, extending the current timer and preserving the strongest multiplier.
     * @param multiplier  Damage multiplier for incoming damage
     * @param duration      Time this state will last
     */
    void applyVulnerable(float multiplier, float duration);
    /**
     * Overwrites local vulnerable state from the host snapshot so remote clients mirror the authoritative state.
     *
     * @param multiplier  The authoritative damage multiplier to apply while vulnerable.
     * @param duration    The authoritative remaining vulnerable time, in seconds.
     */
    void syncVulnerable(float multiplier, float duration);
    /** Clears runtime-only enemy combat effects such as stun and vulnerability. */
    void clearRuntimeEffects();
    
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

#ifndef __ENEMY_H__
#define __ENEMY_H__

#include <cugl/cugl.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "EnemyLoader.h"

/**
 * Enemy runtime instance.
 * - Holds health, facing, current state
 * - Runs state execution timing (buildUp -> fire -> next + cooldown lockout)
 * - Queues fired events for combat system to resolve
 *
 * AI chooses states by calling requestState().
 * Combat consumes events via takeFiredEvents().
 */
class Enemy {
public:

    enum class Facing { UP=0, RIGHT=1, DOWN=2, LEFT=3 };

    struct FiredEvent {
        EnemyLoader::EventDef def; // what to do (damage/effect/etc.)
        EnemyLoader::RelDir target; // relative direction (front/back/left/right)
        Facing facingAtFire; // facing snapshot at moment of firing
        std::string stateName; // which state fired this event (useful for debugging)
    };

private:
    std::string _enemyId;
    std::string _name;
    std::string _spritesheetPath;

    float _maxHealth = 0.0f;
    float _currentHealth = 0.0f;

    //  shields are stored but not used yet
    EnemyLoader::EnemyDef::Shields _shields;

    // State definitions (from loader)
    std::unordered_map<std::string, EnemyLoader::StateDef> _states;

    std::string _currentState = "idle";
    float _stateTime = 0.0f;
    bool _eventsFiredThisState = false;

    float _attackLockout = 0.0f;

    Facing _facing = Facing::DOWN;

    // buffer for fired events
    std::vector<FiredEvent> _firedEvents;

public:
    Enemy() = default;
    Enemy(const std::string& enemyId, const EnemyLoader& loader);

#pragma mark Accessors
    const std::string& getId() const { return _enemyId; }
    const std::string& getName() const { return _name; }
    const std::string& getSpritesheetPath() const { return _spritesheetPath; }

    float getMaxHealth() const { return _maxHealth; }
    float getCurrentHealth() const { return _currentHealth; }
    bool isAlive() const { return _currentHealth > 0.0f; }

    Facing getFacing() const { return _facing; }
    void setFacing(Facing f) { _facing = f; }

    const std::string& getCurrentStateName() const { return _currentState; }
    const EnemyLoader::StateDef* getCurrentStateDef() const;

    float getAttackLockoutRemaining() const { return _attackLockout; }
    bool canStartNonIdleState() const { return _attackLockout <= 0.0f; }

    const EnemyLoader::EnemyDef::Shields& getShields() const { return _shields; }

#pragma mark Gameplay
    /**
     * AI-facing: attempts to enter a new state.
     * - While lockout is active, only "idle" is allowed.
     */
    bool requestState(const std::string& stateName);

    /**
     * Ticks state execution:
     * enter state -> wait buildUpTime -> fire events -> go to nextState -> apply cooldown lockout
     */
    void update(float dt);

    /**
     * Returns and clears fired events since last call.
     */
    std::vector<FiredEvent> takeFiredEvents();

    void applyDamage(float amount);
    void heal(float amount);

private:
    void tick(float dt); // updates timers
    bool readyToFire() const; // checks buildUp vs stateTime
    void fireEvents(); // queues events and sets fired flag
    void applyCooldown(); // applies lockout from current state
    std::string getNextStateOrIdle() const; // validates nextState
    void transitionToNextState(); //  performs the transition
    void enterState(const std::string& stateName); // resets state execution vars
};

#endif /* !__ENEMY_H__ */

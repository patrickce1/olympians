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
        EnemyLoader::State state;       // state that fired this event (debug)
    };

protected:
    /* String to identify the boss */
    std::string _enemyId;
    /* Path to file where the sprite sheet is */
    std::string _spritesheetPath;

    /* Stores any custom data that was encoutered when loading the enemy
       Used by bosses that extend the enemy class to store data related to
       custom behavior */
    std::shared_ptr<cugl::JsonValue> _customData;
    
    /* The direction the boss is facing, AKA the player it is targetting*/
    int _targetIndex = -1;

    /* The maximum health of the boss */
    float _maxHealth = 0.0f;
    /* The current health of the boss */
    float _currentHealth = 0.0f;

    /* This maps the generic states from the enum to a StateDef that stores deeper info
       * about how the attack/defensive move impacts the field */
    std::unordered_map<EnemyLoader::State, EnemyLoader::StateDef> _states;
    
    //Stores the damage multipliers for each side. The sides are relative, so 0 would be direction boss facing
    std::unordered_map<int, float> _sideMultipliers; 

    EnemyLoader::State _currentState = EnemyLoader::State::IDLE;
    
    //How long we have been in this state
    float _stateTime = 0.0f;
    bool _eventsFiredThisState = false;

    // Blocks starting non-idle states while > 0
    float _attackLockout = 0.0f;
    float _retargetLikelihood = 0.0f;
    float _defenseLikelihood = 0.0f;

    std::vector<FiredEvent> _firedEvents;

public:
    /** The number of players in the game */
    static const int NUM_PLAYERS = 4;

    /** Default constructor, use init() to initialize */
    Enemy() = default;

    /** Initializes the enemy with the given id and json path, returns true if successful */
    bool virtual init(const std::string& enemyId, const std::string& jsonPath);

    /** Returns the unique id of this enemy */
    const std::string& getId() const { return _enemyId; }

    /** Returns the file path to the sprite sheet */
    const std::string& getSpritesheetPath() const { return _spritesheetPath; }

    /** Returns the maximum health of this enemy */
    float getMaxHealth() const { return _maxHealth; }

    /** Returns the current health of this enemy */
    float getCurrentHealth() const { return _currentHealth; }

    /** Returns true if the enemy is alive */
    bool isAlive() const { return _currentHealth > 0.0f; }

    /** Sets the current health of this enemy */
    void setCurrentHealth(float health) { _currentHealth = health; }

    /** Returns the index of the player this enemy is currently targeting */
    int getTargetIndex() const { return _targetIndex; }

    /** Sets the index of the player this enemy is currently targeting */
    void setTargetIndex(int index) { _targetIndex = index; }

    /** Returns the current state of this enemy */
    EnemyLoader::State getCurrentState() const { return _currentState; }

    /** Returns how long the enemy has been in the current state */
    float getStateTime() const { return _stateTime; }

    /** Sets how long the enemy has been in the current state */
    void setStateTime(float stateTime) { _stateTime = stateTime; }

    /** Returns the state definition for the current state */
    const EnemyLoader::StateDef* getCurrentStateDef() const;

    /** Returns how long until the enemy can start a new attack */
    float getAttackLockoutRemaining() const { return _attackLockout; }

    /** Returns true if the enemy is able to start a non-idle state */
    bool canStartNonIdleState() const { return _attackLockout <= 0.0f; }

    /** Returns the likelihood that the enemy will retarget on idle entry */
    float getRetargetLikelihood() const { return _retargetLikelihood; }

    /** Returns the likelihood that the enemy will use a defensive move */
    float getDefenseLikelihood() const { return _defenseLikelihood; }

    /** Sets the likelihood that the enemy will retarget on idle entry */
    void setRetargetLikelihood(float v);

    /** Sets the likelihood that the enemy will use a defensive move */
    void setDefenseLikelihood(float d) { _defenseLikelihood = d; }

    /* Checks if this enemy should use their defensive move
    This can and should be overwritten for each boss to have custom logic on when they decide to use their defensive move */
    bool virtual shouldDefend();

    /* Returns the multiplier data for the given absolute side index.
     * Index 0 corresponds to the side facing the host, regardless of the boss' direction.
     * This index is absolute, not relative to the boss' orientation.
     */
    float getSideMultiplier(int absoluteIndex);
    
    /** Lets you change the multipler value on the side equal to relativeIndex
     * @param relativeIndex is the side we want to change the multiplier for. 0 is the direction the boss is facing
     * @param multiplier the damage multiplier we want to apply to relativeIndex
     */
    void setSideMultiplier(int relativeIndex, float multiplier);
    
    /* Expose state defs so controller can pick attacks by tag */ 
    const std::unordered_map<EnemyLoader::State, EnemyLoader::StateDef>& getStates() const { return _states; }

    /** Returns true if successfully enters requested state. False and idle otherwise. */
    bool requestState(EnemyLoader::State state);

    /** Main update loop for enemy. Handles firing events, applying cooldown, transition to next state. */
    void virtual update(float dt);

    /** Return contents of current event buffer and clears it.*/
    std::vector<FiredEvent> takeFiredEvents();

    //Positive heals, negative damages; clamps to [0, maxHealth]
    void updateHealth(float delta);

    /* Handles taking damage and applying the side modifiers
     * Use this method instead of updateHealth() for appropriate damage multiplication 
     * @param damage is the amount of damage being done to the boss
     * @param playerIndex is the index that was assigned to the player by the host
    */
    void virtual takeDamage(float damage, int playerIndex);

    /** Immediately enters the state and resets timers. */
    void enterState(EnemyLoader::State state);

protected:
    /** Updates timers.*/
    void tick(float dt);

    /** Returns true if buildUp time has passed and events have not yet fired in this state.*/
    bool readyToFire() const;

    /** Fires events from this state, adding them to the events buffer.*/
    void fireEvents();

    /** Sets the cooldown timer based on the current state of the enemy. */
    void applyCooldown();

    /** Returns the next state if defined by current state or "idle" by default. */
    EnemyLoader::State getNextStateOrIdle() const;

    /** If the boss is currently in cooldown, it skips the cooldown */
    void skipCooldown();
};

#endif /* !__ENEMY_H__ */
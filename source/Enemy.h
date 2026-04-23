// Enemy.h
#ifndef __ENEMY_H__
#define __ENEMY_H__

#include <string>
#include <unordered_map>
#include <vector>
#include <cugl/cugl.h>
#include "EnemyLoader.h"

/**
* Forward declaration for type trait checking
* Full definition needed for unordered_map; include where needed 
*/
struct AnimationEntry;

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
    /** Unique identifier for this enemy (e.g., "cyclops", "cerberus") */
    std::string _enemyId;
    
    /** Path to the sprite sheet texture for this enemy */
    std::string _spritesheetPath;

    /** Custom game data loaded from enemies.json, available for derived classes to use */
    std::shared_ptr<cugl::JsonValue> _customData;
    
    /** The index of the player this enemy is currently targeting (0-3) */
    int _targetIndex = -1;

    /** Maximum health for this enemy */
    float _maxHealth = 0.0f;
    
    /** Current health for this enemy */
    float _currentHealth = 0.0f;

    /** Maps game states to their state definitions (buildup, cooldown, events, etc.) */
    std::unordered_map<EnemyLoader::State, EnemyLoader::StateDef> _states;
    
    /** Damage multipliers relative to boss facing direction (0 = facing direction, 1 = right side, etc.) */
    std::unordered_map<int, float> _sideMultipliers; 

    /** Current state of the enemy state machine */
    EnemyLoader::State _currentState = EnemyLoader::State::IDLE;
    
    /** Duration spent in the current state (seconds) */
    float _stateTime = 0.0f;
    
    /** Flag to ensure state events fire exactly once per state */
    bool _eventsFiredThisState = false;
    
    /** Current animation frame being displayed (0-indexed, set by GameScene) */
    int _currentAnimationFrame = 0;

    /** Remaining time the enemy cannot start new attacks (cooldown timeout) */
    float _attackLockout = 0.0f;
    
    /** Probability the enemy will retarget on idle entry (0.0 to 1.0) */
    float _retargetLikelihood = 0.0f;
    
    /** Probability the enemy will use their defensive move (0.0 to 1.0) */
    float _defenseLikelihood = 0.0f;

    /** Queue of events fired this update cycle, returned by takeFiredEvents() */
    std::vector<FiredEvent> _firedEvents;

public:
    /** The number of players in the game */
    static const int NUM_PLAYERS = 4;

    /** Default constructor, use init() to initialize */
    Enemy() = default;

    /** Initializes the enemy from JSON configuration without animation metadata.
     * @param enemyId The unique ID of the enemy to load (e.g., "cyclops")
     * @param jsonPath Path to enemies.json configuration file
     * @return true if initialization succeeds, false on error
     */
    bool virtual init(const std::string& enemyId, const std::string& jsonPath);
    
    /** Initializes the enemy with animation metadata from AssetManager.
     * This version uses smart caching to load animation registry only when needed.
     * Prefers this method when assets are available to ensure proper animation setup.
     * 
     * @param enemyId The unique ID of the enemy to load (e.g., "cyclops")
     * @param jsonPath Path to enemies.json configuration file
     * @param assets AssetManager containing enemyAnimations.json and other asset definitions
     * @return true if initialization succeeds, false on error
     */
    bool virtual init(const std::string& enemyId, const std::string& jsonPath, 
                     const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * TESTING ONLY: Clears the static loader initialization state.
     * Call this after all tests complete to allow the game to reinitialize the loader with animation metadata.
     * 
     * Usage:
     *   EnemyTests::runAll(...);
     *   Enemy::clearStaticLoaderForTesting();
     *   // Now game can initialize properly with animations
     */
    static void clearStaticLoaderForTesting();

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

    /** Sets how long the enemy has been in the current state.*/
    void setStateTime(float stateTime) { _stateTime = stateTime; }
    
    /** Returns the current animation frame being displayed (0-indexed within row) */
    int getCurrentAnimationFrame() const { return _currentAnimationFrame; }
    
    /** Sets the current animation frame being displayed. Called by GameScene each frame. */
    void setCurrentAnimationFrame(int frame) { _currentAnimationFrame = frame; }

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

    /**
     * Determines if this enemy should use their defensive move.
     * Can be overridden in derived boss classes for custom logic on when to defend.
     *
     * @return true if the enemy should activate their defensive move, false otherwise
     */
    bool virtual shouldDefend();

    /**
     * Returns the damage multiplier for a given absolute side index.
     * Index 0 corresponds to the side facing the host (east), regardless of the boss' direction.
     * 
     * @param absoluteIndex The absolute side index (0-3), where 0 is facing east
     * @return The damage multiplier for the given side
     */
    float getSideMultiplier(int absoluteIndex);
    
    /**
     * Sets the damage multiplier for a given relative side index.
     * Index 0 is the direction the boss is currently facing.
     * 
     * @param relativeIndex The side relative to boss direction (0 = facing direction)
     * @param multiplier The damage multiplier to apply (e.g., 0.5 for half damage, 2.0 for double)
     */
    void setSideMultiplier(int relativeIndex, float multiplier);
    
    /** Exposes all state definitions so EnemyController can query states by tag. */
    const std::unordered_map<EnemyLoader::State, EnemyLoader::StateDef>& getStates() const { return _states; }

    /**
     * Checks if the enemy is currently in an attack phase (post-buildup) for its current animation.
     * Used to prevent state changes (like retargeting) during the attack wind-up and execution.
     * 
     * @param animationRegistry  Map of animation IDs to animation metadata entries
     * @return true if in attack phase, false if in buildup phase or no animation data
     */
    bool isInAttackPhase(const std::unordered_map<std::string, class AnimationEntry>& animationRegistry) const;

    /** Returns true if successfully enters requested state. False and idle otherwise. */
    bool requestState(EnemyLoader::State state);

    /** Main update loop for enemy. Handles firing events, applying cooldown, transition to next state. */
    void virtual update(float dt);

    /** Return contents of current event buffer and clears it.*/
    std::vector<FiredEvent> takeFiredEvents();

    /**
     * Updates the enemy's health by the given delta amount.
     * Positive values heal, negative values damage. Health is clamped to [0, maxHealth].
     * 
     * @param delta The health change amount (positive heals, negative damages)
     */
    void updateHealth(float delta);

    /**
     * Handles damage to this enemy with side-relative damage multipliers.
     * Applies the appropriate damage multiplier based on which side the attacking player is on,
     * then updates health. Use this method instead of updateHealth() for proper damage scaling.
     * 
     * Can be overridden in derived boss classes for custom damage handling.
     * 
     * @param damage The base damage amount (before multipliers)
     * @param playerIndex The index of the attacking player (determines side multiplier)
     */
    void virtual takeDamage(float damage, int playerIndex);

    /** Immediately enters the state and resets timers. */
    void enterState(EnemyLoader::State state);
    
    /** Returns the next state if defined by current state or "idle" by default. */
    EnemyLoader::State getNextStateOrIdle() const;

protected:
    /** Debug boolean. Set to false to prevent debug statements */
    bool _debug = false;

    /** Initializes this enemy instance from an enemy definition.
     * Sets up state machine, health, side multipliers, and AI parameters.
     * 
     * @param def The enemy definition to initialize from
     * @return true if initialization succeeds, false if required state missing
     */
    bool initializeFromDef(const EnemyLoader::EnemyDef& def);

    /** Updates timers.*/
    void tick(float dt);

    /** Returns true if the animation has completed and events have not yet fired in this state.
     * For animated states, waits until buildUpTime + attack frame duration.
     * For non-animated states, fires at buildUpTime immediately. */
    bool readyToFire() const;

    /** Fires events from this state, adding them to the events buffer.*/
    void fireEvents();

    /** Sets the cooldown timer based on the current state of the enemy. */
    void applyCooldown();

    /** If the boss is currently in cooldown, it skips the cooldown */
    void skipCooldown();
};

#endif /* !__ENEMY_H__ */
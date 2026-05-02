// Enemy.h
#ifndef __ENEMY_H__
#define __ENEMY_H__

#include <array>
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
    /** The number of players in the game */
    static constexpr int NUM_PLAYERS = 4;

    struct FiredEvent {
        EnemyLoader::EventDef def;   // type, target offset, amount, etc.
        EnemyLoader::State state;       // state that fired this event (debug)
    };

protected:
    /** Debug boolean. Set to false to prevent debug statements */
    bool _debug = false;

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
    
    /** Effective damage multipliers relative to boss facing direction. */
    std::array<float, NUM_PLAYERS> _sideMultipliers{};
    
    /** Base side multipliers authored by enemy behavior events. */
    std::array<float, NUM_PLAYERS> _baseSideMultipliers{};

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
    
    /** Remaining stun time in seconds. While positive, enemy combat timers are frozen in place. */
    float _stunDuration = 0.0f;
    
    /** Remaining love time in seconds. While positive, enemy attacks and retargeting are disabled. */
    float _loveDuration = 0.0f;
    
    /** Remaining vulnerable time for each relative side in seconds. */
    std::array<float, NUM_PLAYERS> _vulnerableDurations{};
    
    /** Active vulnerability multiplier for each relative side. */
    std::array<float, NUM_PLAYERS> _vulnerableSideMultipliers{};
    
    /** Probability the enemy will use their defensive move (0.0 to 1.0) */
    float _defenseLikelihood = 0.0f;

    /** Queue of events fired this update cycle, returned by takeFiredEvents() */
    std::vector<FiredEvent> _firedEvents;

public:
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

    /** Sets how long the enemy has been in the current state */
    void setStateTime(float stateTime) { _stateTime = stateTime; }
    
    /** Returns the current animation frame being displayed (0-indexed within row) */
    int getCurrentAnimationFrame() const { return _currentAnimationFrame; }
    
    /** Sets the current animation frame being displayed. Called by GameScene each frame. */
    void setCurrentAnimationFrame(int frame) { _currentAnimationFrame = frame; }

    /** Returns the state definition for the current state */
    const EnemyLoader::StateDef* getCurrentStateDef() const;

    /** Returns how long until the enemy can start a new attack */
    float getAttackLockoutRemaining() const { return _attackLockout; }
    
    /** Returns whether the enemy is currently stunned. */
    bool isStunned() const { return _stunDuration > 0.0f; }
    
    /** Returns the remaining stun duration in seconds. */
    float getStunDuration() const { return _stunDuration; }

    /**
     * Applies or refreshes a stun without changing the enemy's current state.
     * Will NOT apply if the enemy is in the attack phase of an animation.
     * @param duration  The stun time to apply, in seconds.
     */
    void applyStun(float duration);

    /** Returns true when the current state is in the attack animation phase. */
    bool isInAttackAnimationPhase() const;

    /**
     * Forces the enemy into the specified attack state, bypassing normal AI
     * state transitions. Intended for tutorial triggers.
     *
     * @param attackState  The attack state to transition into (e.g. ATTACK_1).
     */
    void forceAttack(EnemyLoader::State attackState);

    /**
     * Forces the enemy into the specified defense state, bypassing normal AI
     * state transitions. Intended for tutorial triggers.
     *
     * @param defenseState  The defense state to transition into (e.g. DEFENSE_MOVE).
     */
    void forceDefense(EnemyLoader::State defenseState);
    
    /**
     * Overwrites local stun time from the host snapshot so remote clients mirror the authoritative state.
     *
     * @param duration  The authoritative remaining stun time, in seconds.
     */
    void syncStunDuration(float duration);
    
    /** Returns whether the enemy is currently loved. */
    bool isLoved() const { return _loveDuration > 0.0f; }
    
    /** Returns the remaining love duration in seconds. */
    float getLoveDuration() const { return _loveDuration; }
    
    /**
     * Applies or refreshes a love, forcing the enemy idle and extending the remaining duration.
     *
     * @param duration  The love time to apply, in seconds.
     */
    void applyLove(float duration);
    
    /**
     * Overwrites local love time from the host snapshot so remote clients mirror the authoritative state.
     *
     * @param duration  The authoritative remaining love time, in seconds.
     */
    void syncLoveDuration(float duration);
    
    /**
     * Returns whether any relative side of the enemy is currently vulnerable.
     *
     * @return true if at least one side has a positive vulnerable timer.
     */
    bool isVulnerable() const;
    
    /**
     * Returns the longest remaining vulnerable duration across all sides.
     *
     * @return The maximum remaining vulnerable time in seconds.
     */
    float getVulnerableDuration() const;
    
    /**
     * Returns the strongest active vulnerable multiplier across all sides.
     *
     * @return The highest active vulnerable multiplier, or 1.0f if none are active.
     */
    float getVulnerableMultiplier() const;
    
    /**
     * Returns the remaining vulnerable duration for a relative side.
     *
     * @param relativeIndex The relative side index to query.
     * @return The remaining vulnerable time for that side in seconds.
     */
    float getVulnerableDurationForSide(int relativeIndex) const;
    
    /**
     * Returns the vulnerable multiplier for a relative side.
     *
     * @param relativeIndex The relative side index to query.
     * @return The vulnerable multiplier for that side, or 1.0f if inactive.
     */
    float getVulnerableMultiplierForSide(int relativeIndex) const;
    
    /**
     * Returns the authoritative vulnerable durations for all sides.
     *
     * @return A per-side array of remaining vulnerable times in seconds.
     */
    std::array<float, NUM_PLAYERS> getVulnerableDurations() const { return _vulnerableDurations; }
    
    /**
     * Returns the authoritative vulnerable multipliers for all sides.
     *
     * @return A per-side array of active vulnerable multipliers.
     */
    std::array<float, NUM_PLAYERS> getVulnerableSideMultipliers() const { return _vulnerableSideMultipliers; }
    
    /**
     * Applies vulnerability to the side hit by the given player.
     *
     * @param multiplier  Damage multiplier to apply to the struck side.
     * @param duration    Time this state will last, in seconds.
     * @param playerIndex The attacking player's slot index.
     */
    void applyVulnerable(float multiplier, float duration, int playerIndex);

    /**
     * Applies the same vulnerability to all relative sides of the enemy.
     *
     * @param multiplier  Damage multiplier to apply to each side.
     * @param duration    Time this state will last, in seconds.
     * @return true if at least one side was updated, false if the request was ignored.
     */
    bool applyVulnerableToAllSides(float multiplier, float duration);
    
    /**
     * Overwrites local vulnerable state from the host snapshot so remote clients mirror the authoritative state.
     *
     * @param multipliers The authoritative per-side vulnerable multipliers.
     * @param durations   The authoritative per-side vulnerable durations, in seconds.
     */
    void syncVulnerable(const std::array<float, NUM_PLAYERS>& multipliers,
                        const std::array<float, NUM_PLAYERS>& durations);
    
    /** Clears runtime-only enemy combat effects such as stun, love, and vulnerability. */
    void clearRuntimeEffects();

    /** Returns true if the enemy is able to start a non-idle state */
    bool canStartNonIdleState() const { return _attackLockout <= 0.0f && !isLoved() && !isStunned(); }

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

    /** Returns true if successfully enters requested state. False and idle otherwise.
     *
     * @param state   The requested state.
     * @return True if successfully enters requested state. False otherwise.
     */
    bool requestState(EnemyLoader::State state);

    /** Main update loop for enemy. Handles firing events, applying cooldown, transition to next state.
     *
     * @param dt  The elapsed time since the previous frame, in seconds.
     */
    void virtual update(float dt);

    /**
     * Advances the enemy's state machine and attack lockout by the given amount,
     * without affecting any effect timers (stun, love, vulnerable).
     * Use this instead of a fake dt when you want to speed up state transitions
     * while leaving effect durations intact.
     *
     * @param amount  The time to advance, in seconds.
     */
    void advanceStateTime(float amount);

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
    /** Initializes this enemy instance from an enemy definition.
     * Sets up state machine, health, side multipliers, and AI parameters.
     * 
     * @param def The enemy definition to initialize from
     * @return true if initialization succeeds, false if required state missing
     */
    bool initializeFromDef(const EnemyLoader::EnemyDef& def);

    /** Updates enemy effects timers.
     *
     * @param dt  The elapsed time since the previous frame, in seconds.
     */
    void tick(float dt);

    /** Returns true if the animation has completed and events have not yet fired in this state.
     * For animated states, waits until buildUpTime + attack frame duration.
     * For non-animated states, fires at buildUpTime immediately. */
    bool readyToFire() const;

    /** Fires events from this state, adding them to the events buffer.*/
    void fireEvents();

    /** Sets the cooldown timer based on the current state of the enemy. */
    void applyCooldown();
    
    /** Forces the enemy back to idle immediately, clearing the current state's progress. */
    void forceIdle();
};

#endif /* !__ENEMY_H__ */

// EnemyController.h
#ifndef __ENEMY_CONTROLLER_H__
#define __ENEMY_CONTROLLER_H__

#include <string>
#include <vector>
#include <cugl/cugl.h>

#include "Enemy.h"
#include "Player.h"

// Forward declarations
struct AnimationEntry;

/**
 * EnemyController
 * - Maintains a current target index into players
 * - Optional retarget on entering idle (chance 0..1)
 * - Chooses next attack while idle based on StateDef.tag == "attack"
 * - Resolves fired enemy events onto players
 *
 * Targeting rule for DAMAGE:
 *   victimIndex = (targetIndex + event.targetOffset) mod N
 */
class EnemyController {
public:
    EnemyController();
    
    /** Enable or disable automatic attack selection. Tutorial Specific */
    void setAttacksEnabled(bool enabled) { _attacksEnabled = enabled; }
    
    void enterIdle(const std::shared_ptr<Enemy>& enemy,
                   std::vector<std::shared_ptr<Player>>& players);

    void update(float dt,
                const std::shared_ptr<Enemy>& enemy,
                std::vector<std::shared_ptr<Player>>& players);

    /** Checks if a scramble event was fired after update() was called
        * Resets the boolean after this is called. It should be called every frame */
    bool didFireScrambleEvent();

    /**
     * Calculates which direction (0-3) an enemy should face relative to a local player.
     * Maps relative position between target and local player to cardinal directions.
     *
     * Formula: (targetIndex - localPlayerIndex + 4) % 4
     *   Direction 0: Forward (facing directly from local player's perspective)
     *   Direction 1: Right
     *   Direction 2: Back
     *   Direction 3: Left
     *
     * @param targetIndex       The index of the player the enemy is targeting (0-3)
     * @param localPlayerIndex  The local player's index (0-3)
     * @return                  Direction 0-3 representing sprite sheet row to display
     */
    static int calculateDirection(int targetIndex, int localPlayerIndex);
    
    /**
     * Sets the animation registry reference for attack phase checking.
     * Called by GameScene during initialization to enable guards against state changes during attacks.
     * 
     * @param registry  Pointer to animation registry map (must outlive this controller)
     */
    void setAnimationRegistry(const std::unordered_map<std::string, class AnimationEntry>* registry) { 
        _animationRegistry = registry; 
    }

private:
    /** Debug boolean. Set to false to prevent debug statements */
    bool _debug = false;

    /** Keeps track of if a scramble event was fired this frame. Should be reset after being extracted */
    bool _scrambleFired = false;

    /** Random number generator for decision making. */
    cugl::Random _rng;
    
    /** Represents whether the boss can attack*/
    bool _attacksEnabled = true;

    /** Seconds to wait in idle before turning to face the new target. */
    static constexpr float IDLE_RETARGET_DELAY = 0.5f;

    /** True while waiting to retarget; cleared when the timer fires or idle is exited. */
    bool _pendingRetarget = false;

    /** Countdown to the deferred retarget. */
    float _retargetTimer = 0.0f;

    /** Reference to animation registry for attack phase detection during retarget guards. */
    const std::unordered_map<std::string, class AnimationEntry>* _animationRegistry = nullptr;

private:
    int randomIndex(int n);
    int wrapIndex(int i, int n) const;

    /** Checks whether the enemy has just entered idle on this frame. */
    void handleIdleEntryIfNeeded(EnemyLoader::State prevState,
                                 EnemyLoader::State curState,
                                 const std::shared_ptr<Enemy>& enemy,
                                 std::vector<std::shared_ptr<Player>>& players);

    /** Upon entering idle state, this function possibly chooses a new target for the enemy. */
    void maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy,
                                  std::vector<std::shared_ptr<Player>>& players);

    /** Chooses the next state tagged with "attack" for the enemy to enter. */
    EnemyLoader::State chooseNextAttackState(const std::shared_ptr<Enemy>& enemy);

    /**
     * Determines whether the boss should enter a defensive state.
     * Returns true if a random chance roll succeeds, or if the enemy's defensive condition is met.
     * @param enemy the enemy used to evaluate whether the defense condition applies
     */
    bool shouldDefend(const std::shared_ptr<Enemy>& enemy);

    void resolveEnemyEvents(const std::shared_ptr<Enemy>& enemy,
                            std::vector<std::shared_ptr<Player>>& players,
                            const std::vector<Enemy::FiredEvent>& events);
    
    /** Deals damage to the targeted players from a damage event. */
    void resolveDamageEvent(const std::shared_ptr<Enemy>& enemy,
                          std::vector<std::shared_ptr<Player>>& players,
                            const Enemy::FiredEvent& fe);
    
    /** Applies side modifiers to the boss based on a side modifier event
     * @param enemy points to the enemy whose side data is being changed
     * @param event is event that was fired by the enemy AI that is meant to change the side data
     */    
    void resolveSideMultiplierEvent(const std::shared_ptr<Enemy>& enemy,
        const Enemy::FiredEvent& fe);
    
    /** Applies a heal to the boss based on a heal event
     * @param enemy points to the enemy that is being healed
     * @param event is event that was fired by the enemy AI that is meant to heal the boss
     */
    void resolveHealEvent(const std::shared_ptr<Enemy>& enemy,
        const Enemy::FiredEvent& fe);

    /**
     * Resolves a vine event fired by the enemy.
     *
     * Selects a target player and applies a Gaia vine bind to a randomly chosen
     * side (left or right) using the corresponding applyVine function.
     * If the selected side is already bound, the vine effect refreshes the timer
     * instead of stacking.
     *
     * @param enemy   The enemy that fired the vine event
     * @param players The list of active player instances
     * @param event   The fired vine event to resolve
     */
    void resolveVineEvent(const std::shared_ptr<Enemy>& enemy,
        std::vector<std::shared_ptr<Player>>& players,
        const Enemy::FiredEvent& event);
};

#endif /* __ENEMY_CONTROLLER_H__ */

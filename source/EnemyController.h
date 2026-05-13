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
class Cerberus;

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

    /**
     * Forces the enemy into IDLE and schedules a deferred retarget.
     *
     * @param enemy    The enemy to idle.
     * @param players  All player instances (forwarded to retarget logic when timer fires).
     */
    void enterIdle(const std::shared_ptr<Enemy>& enemy,
                   std::vector<std::shared_ptr<Player>>& players);

    /**
     * Main update loop. Ticks the enemy's state machine, resolves any fired events onto
     * players, and (when idle and unlocked) selects the next attack or defense state.
     * Also handles deferred retargeting and locks the Cerberus victim for single-head attacks.
     *
     * @param dt       Elapsed time in seconds since the last call.
     * @param enemy    The enemy being updated.
     * @param players  All player instances.
     */
    void update(float dt,
                const std::shared_ptr<Enemy>& enemy,
                std::vector<std::shared_ptr<Player>>& players);

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
    bool _debug = true;

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
    int wrapIndex(int i, int n) const;

    /**
     * Resolves a relative target offset into an absolute player index.
     *
     * @param enemy        The enemy whose targetIndex is used as the base.
     * @param players      All player instances.
     * @param targetOffset Offset from the enemy's current target (0 = primary target).
     * @return             Absolute player index in [0, n), or -1 if players is empty.
     */
    int computeVictim(const std::shared_ptr<Enemy>& enemy,
                      const std::vector<std::shared_ptr<Player>>& players,
                      int targetOffset) const;

    /**
     * Applies Cerberus head-knock redirect logic to a victim index.
     *
     * If a victim was locked at attack-entry time (via Cerberus::lockVictim), that value
     * is returned directly — a head recovering during the build-up phase must not
     * silently retarget the hit to a different player than the animation shows.
     *
     * Otherwise, for single-head attacks (ATTACK_2, ATTACK_3): redirects to an alternate
     * living head if the primary victim's head is currently knocked; returns -1 if no
     * alternate is available. For multi-head attacks: returns -1 (skip) for any knocked
     * head position.
     *
     * @param cerberus  The Cerberus instance.
     * @param victim    Absolute player slot of the intended victim.
     * @param state     The attack state currently resolving events.
     * @return          Final victim slot to use, or -1 if the hit should be skipped.
     */
    int cerberusRedirectVictim(const std::shared_ptr<Cerberus>& cerberus,
                               int victim,
                               EnemyLoader::State state) const;

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
     * Starts the corrosive debuff on the targeted player (Cerberus only).
     * No-ops if the enemy is not a Cerberus instance. Applies cerberusRedirectVictim
     * so a knocked head is handled consistently with the damage event.
     *
     * @param enemy    The enemy firing the event (cast to Cerberus internally).
     * @param players  All player instances.
     * @param fe       The fired CORROSIVE event containing debuff parameters.
     */
    void resolveCorrosiveEvent(const std::shared_ptr<Enemy>& enemy,
                               std::vector<std::shared_ptr<Player>>& players,
                               const Enemy::FiredEvent& fe);
};

#endif /* __ENEMY_CONTROLLER_H__ */

#ifndef __CERBERUS__
#define __CERBERUS__
#include "../Enemy.h"
#include <cugl/cugl.h>

/**
 * Cerberus boss subclass.
 *
 * Adds three behaviours on top of the base Enemy:
 *   - Head knocked: each of the three heads (main, right, left) accumulates damage
 *     from its corresponding side. When a head's accumulated damage exceeds the knock
 *     threshold it is knocked, plays a downed animation, and blocks incoming attacks
 *     from that side. It recovers with a full threshold reset after knockedDuration seconds.
 *   - Life steal (passive): a fraction of all incoming player damage is immediately
 *     converted to healing, making sustained burst damage critical to overcome.
 *   - Frantic mode: once health falls below configurable thresholds, IDLE cooldowns
 *     shorten and idle animations speed up so Cerberus attacks more frequently.
 *     Both thresholds stack independently.
 */
class Cerberus : public Enemy {
private:
    /** Accumulates elapsed time between corrosive inventory drain ticks. */
    float _corrosiveDrainAccum = 0.0f;

    /** Seconds between successive corrosive inventory drain ticks. */
    static constexpr float CORROSIVE_DRAIN_INTERVAL = 1.0f;


    /**
     * Per-head state tracking for the knock mechanic.
     * Indexed 0=main/front, 1=right, 2=left. The back position has no physical head.
     */
    struct CerberusHead {
        bool  knocked          = false; ///< True while the head is downed and cannot deal damage.
        float knockedTimer     = 0.0f;  ///< Seconds remaining until this head recovers.
        float knockedThreshold = 0.0f;  ///< Damage budget remaining before this head is knocked.
    };

    CerberusHead _heads[3];

    /** Maximum knock threshold; restored on recovery. Loaded from customData "knockedThreshold". */
    float _maxKnockedThreshold = 100.0f;

    /** Seconds a knocked head stays downed before recovering. Loaded from customData "knockedDuration". */
    float _knockedDuration = 4.0f;

    /** Damage-equivalent HP per second the knock threshold regenerates while a head is active.
     *  Loaded from customData "knockedThresholdRegen". */
    float _knockedThresholdRegen = 10.0f;

    /** Absolute HP below which the first frantic tier activates.
     *  Computed as maxHealth × frantic1Threshold fraction from customData. */
    float _frantic1Threshold = 0.0f;

    /** Absolute HP below which the second frantic tier activates. Stacks with the first tier. */
    float _frantic2Threshold = 0.0f;

    /** Additional idle-time multiplier contributed by each active frantic tier.
     *  Loaded from customData "franticRate". */
    float _franticRate = 0.0f;

    /** True when the corrosive debuff is currently draining the targeted player's inventory. */
    bool _corrosiveActive = false;

    /** Seconds remaining on the active corrosive debuff. */
    float _corrosiveTimer = 0.0f;

    /** Player slot currently afflicted by the corrosive debuff, or -1 if none. */
    int _corrosiveTarget = -1;

    /**
     * Reads all boss-specific configuration from _customData and initializes head thresholds.
     * Called by both init() overloads after the base Enemy is fully set up.
     */
    void loadCustomData();

    /**
     * Marks the head at the given array index as knocked and starts its recovery timer.
     *
     * @param headArrayIndex  Index into _heads[] (0=main, 1=right, 2=left).
     */
    void knockHead(int headArrayIndex);

    /**
     * Clears the knocked state for the head at the given array index and resets its threshold.
     *
     * @param headArrayIndex  Index into _heads[] (0=main, 1=right, 2=left).
     */
    void unKnockHead(int headArrayIndex);

public:
    Cerberus() {}

    /**
     * Initializes Cerberus without asset manager support.
     *
     * @param enemyId   The unique enemy ID (should be "cerberus").
     * @param jsonPath  Path to enemies.json.
     * @return True if initialization succeeds, false on error.
     */
    bool init(const std::string& enemyId, const std::string& jsonPath) override;

    /**
     * Initializes Cerberus with animation metadata from the AssetManager.
     *
     * @param enemyId   The unique enemy ID (should be "cerberus").
     * @param jsonPath  Path to enemies.json.
     * @param assets    AssetManager containing enemyAnimations.json.
     * @return True if initialization succeeds, false on error.
     */
    bool init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) override;

    /**
     * Per-frame update. Ticks knocked timers, regenerates knock thresholds, ticks the
     * corrosive debuff, and accelerates the IDLE cooldown when frantic tiers are active.
     *
     * @param dt  Elapsed time in seconds since the last update.
     */
    void update(float dt) override;

    /**
     * Handles incoming player damage. Heals Cerberus by the life-steal fraction, reduces
     * the struck head's knock threshold, then delegates to Enemy::takeDamage for
     * side-multiplier application and health reduction.
     *
     * @param damage       Raw damage before side multipliers are applied.
     * @param playerIndex  Slot index of the attacking player.
     */
    void takeDamage(float damage, int playerIndex) override;

    /** Placeholder: drains one item from the corrosive target's inventory. Not yet implemented. */
    void applyCorrosive();

    /** Returns true if the corrosive debuff is currently active on any player. */
    bool isCorrosiveActive() const { return _corrosiveActive; }

    /** Damage multiplier applied when a player strikes any non-back head while all three heads are knocked simultaneously. */
    static constexpr float ALL_HEADS_KNOCKED_MULTIPLIER = 5.0f;

    /** Returns true if all three heads (main, right, left) are simultaneously knocked. */
    bool allHeadsKnocked() const {
        return _heads[0].knocked && _heads[1].knocked && _heads[2].knocked;
    }

    /** Blocks the defense state while any head is knocked — the drain shield requires all heads active. */
    bool canEnterDefenseState() const override {
        return !_heads[0].knocked && !_heads[1].knocked && !_heads[2].knocked;
    }

    /**
     * Returns the cumulative animation speed multiplier contributed by active frantic tiers.
     * Returns 1.0 at full health; each crossed threshold adds _franticRate to the result.
     *
     * @return Speed multiplier >= 1.0.
     */
    float getFranticSpeedMultiplier() const {
        float speedMultiplier = 1.0f;
        if (getCurrentHealth() < _frantic1Threshold) speedMultiplier += _franticRate;
        if (getCurrentHealth() < _frantic2Threshold) speedMultiplier += _franticRate;
        return speedMultiplier;
    }

    /**
     * Returns true if the head facing the given absolute player slot is currently knocked.
     *
     * Converts the absolute slot to a relative position (0=main, 1=right, 2=back, 3=left)
     * using the current target index. The back position (relativeSlot == 2) has no physical
     * head and always returns false.
     *
     * @param playerSlot  Absolute slot index (0-3) of the player whose side to check.
     * @return True if that side's head is knocked, false otherwise.
     */
    bool isHeadKnocked(int playerSlot) const {
        int relativeSlot = (playerSlot - getTargetIndex() + 4) % 4;
        if (relativeSlot == 2) return false;  // back position has no physical head
        int headArrayIndex = (relativeSlot == 3) ? 2 : relativeSlot;
        return _heads[headArrayIndex].knocked;
    }

    /**
     * For attack_3 redirect: finds the absolute player slot of the first non-knocked side
     * head (checks right then left). Returns -1 if both side heads are knocked.
     *
     * @param targetIndex  The enemy's current target player slot.
     * @return Absolute player slot of an available side head, or -1 if none exists.
     */
    int getAlternateKnockedHead(int targetIndex) const {
        if (!_heads[1].knocked) return (targetIndex + 1) % 4;  // right head
        if (!_heads[2].knocked) return (targetIndex + 3) % 4;  // left head
        return -1;
    }
};
#endif // __CERBERUS__

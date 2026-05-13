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
 *   - Corrosive spit: the Venom Spit attack (attack_2) applies a corrosive debuff to the
 *     targeted player, draining one item per interval for CORROSIVE_DURATION seconds.
 *   - Frantic mode: once health falls below configurable thresholds, IDLE cooldowns
 *     shorten and idle animations speed up so Cerberus attacks more frequently.
 *     Both thresholds stack independently.
 */
class Cerberus : public Enemy {
private:
    /** Accumulates elapsed time between corrosive inventory drain ticks. */
    float _corrosiveDrainAccum = 0.0f;

    /** Active drain interval; set by startCorrosive, falls back to CORROSIVE_DRAIN_INTERVAL. */
    float _corrosiveDrainInterval = CORROSIVE_DRAIN_INTERVAL;

    /** Base fade duration per item for the corrosive animation. */
    float _corrosiveFadeDuration = 0.9f;

    /** ±fraction of _corrosiveFadeDuration applied randomly per item (e.g. 0.3 = ±30%). */
    float _corrosiveFadeVariance = 0.3f;

    /** Maximum number of items corroded per hit (0 = no limit). */
    int _corrosiveMaxAffected = 0;

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

    /** Set by knockHead(); consumed once by GameScene to play the knock sound. */
    bool _headKnockSoundPending = false;

    /** True when the corrosive debuff is currently draining the targeted player's inventory. */
    bool _corrosiveActive = false;

    /** Seconds remaining on the active corrosive debuff. */
    float _corrosiveTimer = 0.0f;

    /** Player slot currently afflicted by the corrosive debuff, or -1 if none. */
    int _corrosiveTarget = -1;

    /** Player slot locked in at single-head attack entry (ATTACK_2/3); -1 means use live head state. */
    int _lockedVictim = -1;

    /** Set each drain tick; consumed once by GameScene to remove one item from the target. */
    bool _shouldDrain = false;

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
    /** Seconds between successive corrosive inventory drain ticks. */
    static constexpr float CORROSIVE_DRAIN_INTERVAL = 1.0f;

    /** Duration in seconds that the corrosive debuff lasts. */
    static constexpr float CORROSIVE_DURATION = 20.0f;

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
     * Per-frame update. Ticks knocked timers, regenerates knock thresholds, triggers
     * corrosive on spit attack entry, ticks the corrosive debuff, and accelerates
     * the IDLE cooldown when frantic tiers are active.
     *
     * @param dt  Elapsed time in seconds since the last update.
     */
    void update(float dt) override;

    /**
     * Handles incoming player damage. Reduces the struck head's knock threshold and delegates
     * to Enemy::takeDamage for side-multiplier application and health reduction.
     *
     * @param damage       Raw damage before side multipliers are applied.
     * @param playerIndex  Slot index of the attacking player.
     */
    void takeDamage(float damage, int playerIndex) override;

    /**
     * Applies the corrosive debuff to a player for the specified duration.
     * GameScene should poll shouldDrainItem() each frame to remove items.
     *
     * @param playerIndex  Slot index of the player to afflict.
     * @param duration     How long the debuff lasts in seconds.
     * @param interval     Seconds between drain ticks (0 = use CORROSIVE_DRAIN_INTERVAL default).
     */
    void startCorrosive(int playerIndex, float duration, float interval = 0.0f,
                        float fadeDuration = 0.0f, float fadeVariance = 0.0f,
                        int maxAffected = 0);

    /** Ends the corrosive effect early (e.g., when the player runs out of items). */
    void endCorrosive();

    /** Locks the victim player slot for the current single-head attack so the target
     *  doesn't shift if a head recovers between attack entry and the damage frame. */
    void lockVictim(int playerSlot) { _lockedVictim = playerSlot; }

    /** Returns the locked victim slot, or -1 if none is set. */
    int getLockedVictim() const { return _lockedVictim; }

    /** Clears the locked victim (call when the attack ends and Cerberus returns to idle). */
    void clearLockedVictim() { _lockedVictim = -1; }

    /**
     * Returns true (and clears the flag) if a corrosive drain tick fired this frame.
     * GameScene should call this once per frame and drain one item when it returns true.
     */
    bool shouldDrainItem();

    /** Returns true if the corrosive debuff is currently active on any player. */
    bool isCorrosiveActive() const { return _corrosiveActive; }

    /** Returns the active drain interval in seconds. */
    float getCorrosiveDrainInterval() const { return _corrosiveDrainInterval; }

    /** Returns the base fade duration for corrosive item animations. */
    float getCorrosiveFadeDuration() const { return _corrosiveFadeDuration; }

    /** Returns the ±variance fraction applied randomly per item (e.g. 0.3 = ±30%). */
    float getCorrosiveFadeVariance() const { return _corrosiveFadeVariance; }

    /** Returns the max items corroded per hit (0 = no limit). */
    int getCorrosiveMaxAffected() const { return _corrosiveMaxAffected; }

    /** Returns the player slot currently afflicted by corrosive, or -1 if none. */
    int getCorrosiveTarget() const { return _corrosiveTarget; }

    /** Returns true (and clears the flag) if a head was knocked since the last call. */
    bool consumeHeadKnockSound() {
        bool pending = _headKnockSoundPending;
        _headKnockSoundPending = false;
        return pending;
    }

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
     * @param playerSlot  Absolute slot index (0-3) of the player whose side to check.
     */
    bool isHeadKnocked(int playerSlot) const {
        int relativeSlot = (playerSlot - getTargetIndex() + 4) % 4;
        if (relativeSlot == 2) return false;  // back position has no physical head
        int headArrayIndex = (relativeSlot == 3) ? 2 : relativeSlot;
        return _heads[headArrayIndex].knocked;
    }

    /**
     * For attack redirect: finds the absolute player slot of the first non-knocked side
     * head (checks right then left). Returns -1 if both side heads are knocked.
     *
     * @param targetIndex  The enemy's current target player slot.
     */
    int getAlternateKnockedHead(int targetIndex) const {
        if (!_heads[1].knocked) return (targetIndex + 1) % 4;  // right head
        if (!_heads[2].knocked) return (targetIndex + 3) % 4;  // left head
        return -1;
    }
};
#endif // __CERBERUS__

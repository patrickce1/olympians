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
 *   - Corrosive spit: the Venom Spit attack (attack_2) corrodes items in the targeted
 *     player's inventory in one shot; each corroded item fades out and is removed.
 *   - Frantic mode: once hit count reaches configurable thresholds, IDLE cooldowns
 *     shorten and idle animations speed up so Cerberus attacks more frequently.
 *     Both thresholds stack independently.
 */
class Cerberus : public Enemy {
private:
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

    /** Hit count at which the first frantic tier activates. */
    int _frantic1Threshold = 0;

    /** Hit count at which the second frantic tier activates. Stacks with the first tier. */
    int _frantic2Threshold = 0;

    /** Additional idle-time multiplier contributed by each active frantic tier.
     *  Loaded from customData "franticRate". */
    float _franticRate = 0.0f;

    /** Set by knockHead(); consumed once by GameScene to play the knock sound. */
    bool _headKnockSoundPending = false;

    /** Player slot targeted by the corrosive attack, or -1 if none pending. */
    int _corrosiveTarget = -1;

    /**
     * Player slot committed at the start of a single-head attack (ATTACK_2/3).
     * -1 means no lock is active and cerberusRedirectVictim evaluates head state live.
     * Set by lockVictim() in EnemyController when the attack is chosen; cleared on
     * IDLE entry so the next attack re-evaluates from scratch.
     */
    int _lockedVictim = -1;

    /** Set when the corrosive attack fires; consumed once by GameScene to apply the one-shot drain. */
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

    /** Default constructor. */
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
     * Marks the corrosive attack as pending for the given player (one-shot).
     * GameScene polls shouldDrainItem() once per frame; the flag is consumed on the
     * first call after markCorrosive() so the drain fires exactly once per attack.
     *
     * @param playerIndex  Slot index of the player to corrode.
     * @param fadeDuration Base fade-out duration per item (0 = use default 0.9s).
     * @param fadeVariance ±fraction applied randomly to fadeDuration per item (0 = use default 0.3).
     * @param maxAffected  Maximum number of items corroded per hit (0 = no limit).
     */
    void markCorrosive(int playerIndex, float fadeDuration = 0.0f,
                       float fadeVariance = 0.0f, int maxAffected = 0);

    /**
     * Locks in the victim player slot for the current single-head attack.
     * Call this when the attack state is chosen so the damage frame cannot
     * silently retarget to the front head if a knocked head recovers mid-buildup.
     *
     * @param playerSlot  Absolute player slot (0–3) to lock as the attack target.
     */
    void lockVictim(int playerSlot) { _lockedVictim = playerSlot; }

    /**
     * Returns the locked victim player slot, or -1 if no lock is currently set.
     *
     * @return Locked player slot (0–3), or -1.
     */
    int getLockedVictim() const { return _lockedVictim; }

    /**
     * Clears the locked victim so the next attack re-evaluates head state live.
     * Called by EnemyController when Cerberus returns to IDLE.
     */
    void clearLockedVictim() { _lockedVictim = -1; }

    /**
     * Returns true (and clears the flag) if a corrosive drain is pending this frame.
     * GameScene calls this once per frame; the flag is consumed after one true return.
     */
    bool shouldDrainItem();

    /** Returns the player slot targeted by the pending corrosive drain, or -1 if none. */
    int getCorrosiveTarget() const { return _corrosiveTarget; }

    /** Returns the base fade duration per item for the corrosive animation. */
    float getCorrosiveFadeDuration() const { return _corrosiveFadeDuration; }

    /** Returns the ±variance fraction applied randomly per item fade. */
    float getCorrosiveFadeVariance() const { return _corrosiveFadeVariance; }

    /** Returns the max items corroded per hit (0 = no limit). */
    int getCorrosiveMaxAffected() const { return _corrosiveMaxAffected; }

    /**
     * Returns true (and clears the flag) if a head was knocked since the last call.
     * Used by GameScene to play the head-knock sound exactly once per knock event.
     *
     * @return True if a head knock occurred since the last call; false otherwise.
     */
    bool consumeHeadKnockSound() {
        bool pending = _headKnockSoundPending;
        _headKnockSoundPending = false;
        return pending;
    }

    /** Damage multiplier applied when a player strikes any non-back head while all three heads are knocked simultaneously. */
    static constexpr float ALL_HEADS_KNOCKED_MULTIPLIER = 5.0f;

    /**
     * Returns true if all three heads (main, right, left) are simultaneously knocked.
     * Used to detect the vulnerability window for the 5× damage multiplier.
     *
     * @return True if all three heads are currently in the knocked state.
     */
    bool allHeadsKnocked() const {
        return _heads[0].knocked && _heads[1].knocked && _heads[2].knocked;
    }

    /**
     * Blocks the defense state while any head is knocked.
     * The defense state requires all heads to be active to enter.
     *
     * @return True if no heads are currently knocked; false if any head is downed.
     */
    bool canEnterDefenseState() const override {
        return !_heads[0].knocked && !_heads[1].knocked && !_heads[2].knocked;
    }

    /**
     * Returns the cumulative animation speed multiplier contributed by active frantic tiers.
     * Returns 1.0 at full health; each crossed threshold adds _franticRate to the result.
     *
     * @return Speed multiplier >= 1.0; higher values mean faster idle animations.
     */
    float getFranticSpeedMultiplier() const {
        float speedMultiplier = 1.0f;
        if (_frantic1Threshold > 0 && getHitCount() >= _frantic1Threshold) speedMultiplier += _franticRate;
        if (_frantic2Threshold > 0 && getHitCount() >= _frantic2Threshold) speedMultiplier += _franticRate;
        return speedMultiplier;
    }

    /**
     * Returns true if the head facing the given absolute player slot is currently knocked.
     *
     * @param playerSlot  Absolute slot index (0-3) of the player whose side to check.
     * @return            True if that head is currently downed; false if active or back position.
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
     * @return             Absolute player slot of an available side head, or -1 if none.
     */
    int getAlternateKnockedHead(int targetIndex) const {
        if (!_heads[1].knocked) return (targetIndex + 1) % 4;  // right head
        if (!_heads[2].knocked) return (targetIndex + 3) % 4;  // left head
        return -1;
    }

    /** Returns whether head i (0=main, 1=right, 2=left) is currently knocked. */
    bool isHeadKnockedByIndex(int i) const { return _heads[i].knocked; }

    /** Returns the remaining recovery timer for head i (0=main, 1=right, 2=left). */
    float getHeadKnockedTimer(int i) const { return _heads[i].knockedTimer; }

    /**
     * Directly sets the knocked state for head i from an authoritative network snapshot.
     * Does not trigger side-multiplier changes (those are synced separately).
     *
     * @param i       Head array index (0=main, 1=right, 2=left).
     * @param knocked Whether the head is downed.
     * @param timer   Seconds remaining until recovery.
     */
    void syncHeadKnockedState(int i, bool knocked, float timer) {
        _heads[i].knocked = knocked;
        _heads[i].knockedTimer = timer;
    }
};
#endif // __CERBERUS__

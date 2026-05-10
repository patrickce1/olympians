#ifndef __CERBERUS__
#define __CERBERUS__
#include "../Enemy.h"
#include <cugl/cugl.h>

/**
 * Cerberus boss subclass.
 *
 * Adds two behaviours on top of the base Enemy:
 *   - Head knocked: each head can be knocked by sustained damage from one side.
 *     When knocked, the head plays a downed animation and does not deal damage
 *     to the player on that side. Recovers after knockedDuration seconds.
 *   - Life steal (passive): a fraction of all damage dealt to Cerberus is
 *     converted into healing, making sustained burst damage critical.
 */
class Cerberus : public Enemy {
private:
    /** Fraction of damage converted to healing (from customData) */
    float _lifeStealPercent = 0.0f;

    /** Accumulates elapsed time between corrosive inventory drains */
    float _corrosiveDrainAccum = 0.0f;

    /** seconds between inventory drains */
    static constexpr float CORROSIVE_DRAIN_INTERVAL = 1.0f;

    struct CerberusHead {
        bool  knocked          = false;
        float knockedTimer     = 0.0f;
        float knockedThreshold = 0.0f;  // counts down as the side takes damage
    };

    /** Per-head knocked state: 0=main/front, 1=right, 2=left. Back position has no head. */
    CerberusHead _heads[3];

    /** Starting threshold loaded from customData ("knockedThreshold") */
    float _maxKnockedThreshold = 50.0f;

    /** How long a head stays knocked (from customData "knockedDuration") */
    float _knockedDuration = 4.0f;

    /** HP/sec the threshold regenerates when a head is not knocked */
    float _knockedThresholdRegen = 10.0f;

    /** True when the corrosive debuff is active on the targeted player */
    bool _corrosiveActive = false;

    /** Remaining duration of the corrosive debuff in seconds */
    float _corrosiveTimer = 0.0f;

    /** Player slot index currently afflicted by the corrosive debuff */
    int _corrosiveTarget = -1;

    void knockHead(int playerSlot);
    void unKnockHead(int playerSlot);

public:
    Cerberus() {}

    /**
     * Initializes Cerberus without asset manager support.
     *
     * @param enemyId   The unique enemy ID (should be "cerberus")
     * @param jsonPath  Path to enemies.json
     * @return true if initialization succeeds, false on error
     */
    bool init(const std::string& enemyId, const std::string& jsonPath) override;

    /**
     * Initializes Cerberus with animation metadata from the AssetManager.
     *
     * @param enemyId   The unique enemy ID (should be "cerberus")
     * @param jsonPath  Path to enemies.json
     * @param assets    AssetManager containing enemyAnimations.json
     * @return true if initialization succeeds, false on error
     */
    bool init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) override;

    /**
     * Per-frame update. Ticks down knocked timers and corrosive
     * debuff timer, clearing them when they expire.
     *
     * @param dt  Elapsed time in seconds since the last update
     */
    void update(float dt) override;

    /**
     * Applies damage with life steal. A defined percent of the raw damage value is
     * added back as healing before side multipliers are applied.
     *
     * @param damage       Raw damage before side multipliers
     * @param playerIndex  Slot index of the attacking player
     */
    void takeDamage(float damage, int playerIndex) override;

    void applyCorrosive();

    /** Returns true if the corrosive debuff is currently active */
    bool isCorrosiveActive() const { return _corrosiveActive; }

    /**
     * Returns true if the head facing the given absolute player slot is knocked.
     * Maps: relPos4 0=main→_heads[0], 1=right→_heads[1], 2=back(no head)→false, 3=left→_heads[2].
     */
    bool isHeadKnocked(int playerSlot) const {
        int relPos4 = (playerSlot - getTargetIndex() + 4) % 4;
        if (relPos4 == 2) return false;  // back position, no head exists
        int headIdx = (relPos4 == 3) ? 2 : relPos4;
        return _heads[headIdx].knocked;
    }

    /**
     * For attack_3: finds a non-knocked side head to redirect damage to.
     * Checks right (_heads[1]) and left (_heads[2]), returns the absolute
     * player slot of the first non-knocked one, or -1 if both are knocked.
     */
    int getAlternateKnockedHead(int targetIndex) const {
        if (!_heads[1].knocked) return (targetIndex + 1) % 4;  // right
        if (!_heads[2].knocked) return (targetIndex + 3) % 4;  // left
        return -1;
    }
};
#endif // __CERBERUS__

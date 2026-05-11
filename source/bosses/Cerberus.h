#ifndef __CERBERUS__
#define __CERBERUS__
#include "../Enemy.h"
#include <cugl/cugl.h>

/**
 * Cerberus boss subclass.
 *
 * Adds two behaviours on top of the base Enemy:
 *   - Head stun: each of Cerberus's 3 heads can be individually stunned.
 *     Stunning all 3 simultaneously triggers a full stun, playing the
 *     heads-lowered animation and leaving Cerberus vulnerable.
 *   - Corrosive spit (Attack #3): applies a lasting corrosive debuff to
 *     the targeted player's inventory — tokens fade (lower opacity),
 *     the inventory displays a corrosive outline, and the player cannot
 *     receive or pass items for the duration.
 */
class Cerberus : public Enemy {
private:
    /** Accumulates elapsed time between corrosive inventory drains */
    float _corrosiveDrainAccum;

    /** Tracks which of the 3 heads are currently stunned */
    bool _headStunned[3];
    
    /** Tracks how much long each head is stunned for. Should be 0 if not stunned */
    float _headStunTimer[3];
    
    /** Current health of each of Cerberus's 3 heads */
    float _headHealth[3];

    /** Max health of each head, loaded from JSON */
    float _headMaxHealth[3];

    /** True when all 3 heads are stunned simultaneously. Cerberus is disabled */
    bool _isFullyStunned;

    /** True when the corrosive debuff is active on the targeted player */
    bool _corrosiveActive;

    /** True if the boss should try to drain an item from the inventory during corrosive.*/
    bool _shouldDrain;

    /** Remaining duration of the corrosive debuff in seconds */
    float _corrosiveTimer;

    /** Player slot index currently afflicted by the corrosive debuff */
    int _corrosiveTarget = -1;

    /** Previous state for detecting state transitions */
    EnemyLoader::State _previousState;

public:
    
    /** seconds between inventory drains. i.e. how long in time for the next item to start corroding */
    static constexpr float CORROSIVE_DRAIN_INTERVAL = 3.0f;

    /** Duration in seconds that a head stays stunned after being hit */
    static constexpr float HEAD_STUN_DURATION = 3.0f;

    /** Duration in seconds that the corrosive debuff lasts */
    static constexpr float CORROSIVE_DURATION = 20.0f;
    
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
     * Per-frame update. Ticks down the full stun timer and corrosive
     * debuff timer, clearing them when they expire. Also triggers corrosive
     * when ATTACK_3 state begins.
     *
     * @param dt  Elapsed time in seconds since the last update
     */
    void update(float dt) override;

    /**
     * Applies damage to the boss from the given playerIndex. 
     * If we have used our defensive attack (lifesteal), we instead heal by 0.8(damage)
     *
     * @param damage       Raw damage before side multipliers
     * @param playerIndex  Slot index of the attacking player
     */
    void takeDamage(float damage, int playerIndex) override;

    /**
     * Registers a stun on one of Cerberus's heads. If all 3 heads become
     * stunned simultaneously, triggers a full stun and plays the
     * heads-lowered animation.
     *
     * @param headIndex  Which head was stunned (0, 1, or 2)
     */
    void stunHead(int headIndex);

    /**
     * Clears the stun state on a single head, called when the stun
     * duration on that head expires.
     *
     * @param headIndex  Which head to unstun (0, 1, or 2)
     */
    void unstunHead(int headIndex);



    /**Determines if corrosive should drain an item for the player.
     * Makes sure there are items left to take, and sufficent time (CORROSIVE_DRAIN_INTERVAL) has passed.
     * 
     * @return true if the draining should be active. False otherwise.
     * */
    bool shouldDrainItem();

    /**
     * Applies the corrosive debuff to a player for the specified duration.
     * GameScene will fade tokens, show the corrosive outline, and block
     * item passing for the debuff duration.
     *
     * @param playerIndex  Slot index of the player to afflict
     * @param duration     How long the debuff lasts in seconds
     */
    void startCorrosive(int playerIndex, float duration);

    /** Ends the corrosive effect early (e.g., when player runs out of items) */
    void endCorrosive();

    /** Returns true if Cerberus is currently in a full stun */
    bool isFullyStunned() const { return _isFullyStunned; }

    /** Returns true if the corrosive debuff is currently active */
    bool isCorrosiveActive() const { return _corrosiveActive; }

    /** Returns the player index currently afflicted by corrosive, or -1 if none */
    int getCorrosiveTarget() const { return _corrosiveTarget; }
};
#endif // __CERBERUS__

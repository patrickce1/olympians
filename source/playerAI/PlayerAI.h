#ifndef __PLAYER_AI_H__
#define __PLAYER_AI_H__

#include <cugl/cugl.h>
#include <unordered_set>
#include "../Player.h"
#include "../Enemy.h"
#include "../items/ItemController.h"
#include "../items/ItemDatabase.h"

/**
 * Unified AI controller for all difficulty levels.
 *
 * PlayerAI drives a bot-controlled player using a weighted-random FSM.
 * Difficulty is expressed through a single _decisionMultiplier in [0, 1]:
 *
 *   0.0  — weakest (slow, ignores rarity, 50/50 attack-support, heals only near-dead)
 *   1.0  — strongest (fast, rarity-aware, house-matched weights, heals early)
 *
 * Four parameters are scaled by the multiplier:
 *
 *   thinkInterval    — how often the AI acts. Ranges from 3s (worst) to 1s (best).
 *   healThreshold    — health ratio at which a teammate forces a heal override.
 *                      Ranges from 0.2 (worst) to 0.8 (best).
 *   houseTypeWisdom  — how closely attack/support weights match the AI's house
 *                      ratios from houses.json. At 0, weights are 50/50.
 *                      At 1, they mirror the house exactly.
 *   rarityWisdom     — probability scale for passing unowned rare/divine items.
 *                      At 0, no rarity awareness. At 1, full pass chances apply
 *                      (rarePassChance for rares, divinePassChance for divines).
 *                      Items the AI originally received are always used, never passed.
 *
 * The multiplier is set by GameState based on the selected boss and the
 * player's accumulated XP (from SavedDataManager).
 *
 * Support and pass actions target only the left or right neighbor of the
 * controlled player, as set by Player::setLeftPlayer / setRightPlayer.
 */
class PlayerAI : public Player {
public:

    /**
     * The finite state machine states.
     *
     * IDLE    - No action; waiting for the next think cycle.
     * ATTACK  - Using an attack item on the enemy.
     * SUPPORT - Using a support item to heal a neighbor.
     * PASS    - Passing an item to an adjacent neighbor.
     */
    enum class State {
        IDLE,
        ATTACK,
        SUPPORT,
        PASS
    };

protected:
    /** Debug boolean. Set to false to suppress debug output. */
    bool _debug = true;

    /** The current FSM state of this AI controller. */
    State _state = State::IDLE;

    /** Accumulator tracking elapsed time since the last think cycle. */
    float _thinkTimer = 0.0f;

    // ── Raw JSON floor/ceiling values ──────────────────────────────────────

    /**
     * Fastest think interval in seconds (best AI). Loaded from JSON.
     * The AI will act no more often than this.
     */
    float _thinkIntervalMin;

    /**
     * Slowest think interval in seconds (worst AI). Loaded from JSON.
     * The AI will act no less often than this.
     */
    float _thinkIntervalMax;

    /**
     * Minimum heal threshold (worst AI — only heals nearly dead teammates).
     * Loaded from JSON.
     */
    float _healThresholdMin;

    /**
     * Maximum heal threshold (best AI — heals teammates early).
     * Loaded from JSON.
     */
    float _healThresholdMax;

    /** Minimum rare pass probability (worst AI). Loaded from JSON. */
    float _rarePassChanceMin;
    
    /** Maximum rare pass probability (best AI). Loaded from JSON. */
    float _rarePassChanceMax;

    /** Minimum divine pass probability (worst AI). Loaded from JSON. */
    float _divinePassChanceMin;
    
    /** Maximum divine pass probability (best AI). Loaded from JSON. */
    float _divinePassChanceMax;
    
    /**
     * Minimum pass probability for a gaia_rock item selected during actSupport()
     * (worst AI). At this floor the AI still passes the rock most of the time
     * rather than harming a teammate. Loaded from JSON.
     */
    float _rockPassChanceMin;

    /**
     * Maximum pass probability for a gaia_rock item selected during actSupport()
     * (best AI). Loaded from JSON.
     */
    float _rockPassChanceMax;
    
    // ── Runtime interpolated values ────────────────────────────────────────

    /**
     * Current think interval in seconds, interpolated between
     * _thinkIntervalMax (worst) and _thinkIntervalMin (best).
     */
    float _thinkInterval;

    /**
     * Current heal threshold, interpolated between _healThresholdMin and
     * _healThresholdMax by _decisionMultiplier.
     */
    float _healThreshold;

    /**
     * Current attack weight used in the weighted random roll.
     * At multiplier=0 this is 0.5. At multiplier=1 it equals the house's
     * attack ratio from houses.json.
     */
    float _attackWeight = 0.5f;

    /**
     * Current support weight used in the weighted random roll.
     * At multiplier=0 this is 0.5. At multiplier=1 it equals the house's
     * support ratio from houses.json.
     */
    float _supportWeight = 0.5f;

    /**
     * Effective rare pass probability this session.
     * Equals _rarePassChance * _decisionMultiplier.
     */
    float _effectiveRarePassChance = 0.0f;

    /**
     * Effective divine pass probability this session.
     * Equals _divinePassChance * _decisionMultiplier.
     */
    float _effectiveDivinePassChance = 0.0f;
    
    /**
     * Current probability that the AI passes a gaia_rock instead of using it
     * on a teammate during actSupport(). Interpolated between _rockPassChanceMin
     * and _rockPassChanceMax by _decisionMultiplier.
     */
    float _effectiveRockPassChance = 0.5f;

    /**
     * Scales AI decision quality from 0.0 (easiest) to 1.0 (hardest).
     * Set via setDecisionMultiplier() by GameState before the round begins.
     * Drives interpolation of all four wisdom parameters.
     */
    float _decisionMultiplier;

    /**
     * The set of item IDs originally spawned for this AI.
     * Populated via onItemSpawned(). Any inventory item whose ID is absent
     * is treated as received via a pass and is eligible for rarity-based
     * pass checks in evaluate().
     */
    std::unordered_set<ItemInstance::ItemId> _ownedItemIds;

    /**
     * Item ID selected for passing during evaluate() when a rarity-pass
     * fires. Consumed and cleared in actPass(). Zero when unset.
     */
    ItemInstance::ItemId _pendingPassItemId = 0;

    /** Read-only reference to the item database for item type lookups. */
    const ItemDatabase* _db = nullptr;
    
    /**
     * The preferred target for the pending rarity-pass, set during evaluate()
     * alongside _pendingPassItemId. Points to the neighbor who owns the item,
     * or nullptr if neither neighbor owns it (in which case actPass() picks
     * randomly). Cleared in actPass() after the pass resolves.
     */
    Player* _pendingPassTarget = nullptr;

public:

    /**
     * Constructs a PlayerAI, forwarding all arguments to the Player base constructor.
     *
     * @param houseId       The house ID as it appears in houses.json.
     * @param playerNumber  The assigned player slot number.
     * @param playerName    Display name for this player.
     * @param loader        The HouseLoader used to populate stats.
     */
    PlayerAI(const std::string& houseId,
             int playerNumber,
             const std::string& playerName,
             const HouseLoader& loader)
        : Player(houseId, playerNumber, playerName, loader) {}

    virtual ~PlayerAI() = default;

    /**
     * Initializes the AI controller with an item database and JSON config.
     *
     * Reads the "playerAI" block from playerAI.json, loading thinkInterval
     * min/max, healThreshold min/max, rarePassChance, and divinePassChance.
     * Calls applyDecisionMultiplier() to derive initial runtime values from
     * the current _decisionMultiplier (default 0.0 = easiest).
     *
     * @param db    Read-only reference to the item database for item lookups.
     * @param path  Path to the playerAI.json config file.
     * @return true if all required fields were loaded successfully.
     */
    bool init(const ItemDatabase& db, const std::string& path);

    /**
     * Steps the AI forward by one frame.
     *
     * Accumulates dt into the think timer. Once the timer exceeds
     * _thinkInterval, evaluates game context and dispatches to the
     * appropriate action (attack, support, or pass). Resets the timer
     * after each think cycle.
     *
     * @param dt     Time elapsed since the last frame in seconds.
     * @param enemy  The current enemy, used as the attack target.
     * @param items  The ItemController, used to execute item actions.
     */
    void update(float dt, Enemy& enemy, ItemController& items);

    /**
     * Sets the AI difficulty multiplier and re-derives all runtime
     * decision parameters.
     *
     * Should be called by GameState before the round begins, and whenever
     * the boss or XP-based difficulty changes. Safe to call at any time.
     * Reads the house's attack/support ratio from the item database's house
     * multipliers to drive houseTypeWisdom interpolation.
     *
     * @param multiplier  Difficulty in [0, 1]. Clamped to that range.
     *                    0 = easiest, 1 = hardest.
     */
    void setDecisionMultiplier(float multiplier);

    /**
     * Returns the current decision multiplier.
     *
     * @return The active difficulty multiplier in [0, 1].
     */
    float getDecisionMultiplier() const { return _decisionMultiplier; }

    /**
     * Records an item ID as originally spawned for this AI.
     *
     * Must be called by the ItemController or GameScene immediately after
     * giving this player a new item via giveRandomItem() or giveItemByID().
     * Must NOT be called for items received via a pass from another player,
     * so those are correctly treated as unowned in evaluate().
     *
     * @param itemId  The ItemId of the newly spawned item to mark as owned.
     */
    void onItemSpawned(ItemInstance::ItemId itemId);

    /**
     * Returns the current FSM state.
     *
     * @return The active State enum value.
     */
    State getState() const { return _state; }

    /** Returns true — this player is always AI-controlled. */
    bool isAI() const override { return true; }

private:

    /**
     * Re-derives all runtime parameters from _decisionMultiplier.
     *
     * - thinkInterval: interpolated from _thinkIntervalMax (worst) to
     *   _thinkIntervalMin (best), since a lower interval = faster acting.
     * - healThreshold: interpolated from _healThresholdMin to _healThresholdMax.
     * - attackWeight / supportWeight: interpolated from 0.5/0.5 (flat) toward
     *   the house's own attack/support ratio from houses.json.
     * - effectiveRarePassChance / effectiveDivinePassChance: scaled linearly
     *   from 0 to their configured maximums by _decisionMultiplier.
     *
     * Called automatically by setDecisionMultiplier(). Not intended for
     * external use.
     */
    void applyDecisionMultiplier();

    /**
     * Evaluates game context and returns the state the AI should transition to.
     *
     * First checks for a forced heal override: if any neighbor's health is
     * below _healThreshold and the AI has a support item, returns SUPPORT
     * immediately regardless of the weighted roll.
     *
     * Then checks for unowned divine items (divinePassChance), then unowned
     * rare items (rarePassChance). If a rarity-pass fires, sets
     * _pendingPassItemId and returns PASS. Only one pass-check fires per cycle.
     *
     * If no override triggers, performs a weighted random roll between ATTACK
     * and SUPPORT using _attackWeight and _supportWeight. Returns PASS if
     * neither is viable, or IDLE if inventory is empty.
     *
     * @param enemy  The current enemy.
     * @return The State the AI should transition to this think cycle.
     */
    State evaluate(const Enemy& enemy);

    /**
     * Returns whether the AI has at least one attack item in its inventory.
     *
     * @return true if any inventory item has type ItemDef::Type::Attack.
     */
    bool canAttack() const;

    /**
     * Returns whether the AI has a support item AND at least one neighbor
     * is alive and below _healThreshold.
     *
     * @return true if a support item exists and a valid heal target is available.
     */
    bool canSupport() const;

    /**
     * Collects all attack items from inventory and uses a random one on the enemy.
     * Does nothing if no attack items are found.
     *
     * @param enemy  The enemy to attack.
     * @param items  The ItemController used to resolve the item action.
     */
    void actAttack(Enemy& enemy, ItemController& items);

    /**
     * Finds the most injured neighbor below _healThreshold and uses a random
     * support item on them. Does nothing if no valid target or support item exists.
     *
     * @param items  The ItemController used to resolve the item action.
     */
    void actSupport(ItemController& items);

    /**
     * Passes an item to a random alive neighbor (left or right).
     *
     * If _pendingPassItemId was set by evaluate() this cycle, that specific
     * item is passed and its ID is removed from _ownedItemIds. Falls back to
     * a random item if the pending item is not found. Clears _pendingPassItemId
     * after resolving. Does nothing if inventory is empty or no alive neighbor
     * exists.
     */
    void actPass();
    
    /**
     * Returns whether this player originally received this item via spawn
     * (as opposed to receiving it via a pass from another player).
     *
     * Used by neighboring AI players during rarity-pass logic to determine
     * who an unowned item should be returned to.
     *
     * @param itemId  The ItemId to check.
     * @return true if this player's _ownedItemIds contains the given ID.
     */
    bool ownsItem(ItemInstance::ItemId itemId) const {
        return _ownedItemIds.count(itemId) > 0;
    }
    
    /**
     * Returns whether the AI has at least one support item in its inventory,
     * regardless of neighbor health. Used by evaluate() to include SUPPORT
     * in the weighted roll independently of whether a target currently qualifies.
     *
     * @return true if any inventory item has type ItemDef::Type::Support.
     */
    bool hasSupportItem() const;
};

#endif /* __PLAYER_AI_H__ */

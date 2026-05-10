//  Player.h
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <cugl/cugl.h>
#include <string>
#include <vector>
#include "HouseLoader.h"
#include "items/ItemInstance.h"
#include "items/ItemDatabase.h"
#include "Enemy.h"
#include <algorithm>
#include <type_traits>

/**
 * Model Class representing the Player
 */
class Player {
private:
    
    /** The inventory of the player stored as a vector of ItemInstance objects*/
    std::vector<ItemInstance> _inventory;

    /** Debug boolean. Set to false to prevent debug statements */
    bool _debug = false;

    /** The player number of this user**/
    int _playerNumber;
    /** The player name*/
    std::string _playerName;
    /** The house name for the house the player is from*/
    std::string _houseId;
    /** The max health of the player*/
    float _maxHealth;
    /** The current health of the house and player*/
    float _currentHealth;
    /** The spritesheet correlating to this house*/
    std::string _spritesheetPath;
    /** The list of the house special abilities*/
    std::vector<std::string> _specialAbilities;
    /** The player to the left of this player, or nullptr if none */
    Player* _leftPlayer = nullptr;
    /** The player to the right of this player, or nullptr if none */
    Player* _rightPlayer = nullptr;
    /** Runtime fixed-mitigation shield state */
    bool _hasShield = false;
    /** The amount of health the shield has left */
    float _shieldHealth = 0.0f;
    /** The time left before the shield expires */
    float _shieldDuration = 0.0f;
    /** Set to true when the shield absorbs incoming damage; cleared by GameScene after playing the block sound */
    bool _shieldAbsorbedDamage = false;
    /** Runtime percentage-mitigation barrier state */
    bool _hasBarrier = false;
    /** The percentage damage that will be mitigated */
    float _barrierMultiplier = 1.0f;
    /** The time left before the barrier expires */
    float _barrierDuration = 0.0f;
    /** Runtime heal-over-time state. */
    bool _hasRegen = false;
    /** The total healing still left to apply over the remaining regen duration. */
    float _regenAmountRemaining = 0.0f;
    /** The time left before the regen expires. */
    float _regenDuration = 0.0f;
    /** Number of prior mallet uses recorded for this player this round. */
    int _malletUseCount = 0;

public:
    /**
     *Creates a player instance given a house ID
     * @param houseId         The ID of the house as appears in the JSON
     */
    Player(const std::string& houseId, int playerNumber,
           const std::string& playerName,
           const HouseLoader& loader);
    
    /**
     * Discards the player and releases all resources
     */
    ~Player() {};
    
#pragma mark Properties
    /**
     * Returns the inventory of the player
     */
    const std::vector<ItemInstance>& getInventory() const { return _inventory;}
    
    /**
     * Returns whether player's inventory is not empty
     */
    bool hasItems() const { return !_inventory.empty(); }
    
    /**
     * Returns the player number
     */
    int getPlayerNumber() const { return _playerNumber; }
    
    /**
     * Returns the player name
     */
    std::string getPlayerName() const { return _playerName; }
    
    /**
     * Return the name of the house
     */
    std::string getHouseName() const { return _houseId; }
    
    /**
     * Returns whether the player's house is female (for sound effect purposes)
     */
    bool isFemaleHouse() const {
        std::string house = getHouseName();
        return house == "athena" || house == "aphrodite" || house == "demeter";
    }

    /**
     * Return the max health of the house/player
     */
    float getMaxHealth() const { return _maxHealth; }
    
    /**
     * Return the current health of the player
     */
    float getCurrentHealth() const { return _currentHealth; }

    /** Returns whether a shield is currently armed on this player. */
    bool hasShield() const { return _hasShield; }

    /** Returns true and clears the flag if the shield absorbed damage this hit. */
    bool consumeShieldAbsorbedDamage() { bool didAbsorb = _shieldAbsorbedDamage; _shieldAbsorbedDamage = false; return didAbsorb; }
    
    /** Returns the current fixed mitigation value. */
    float getShieldHealth() const { return _shieldHealth; }
    
    /** Returns the remaining shield duration. */
    float getShieldDuration() const { return _shieldDuration; }
    
    /** Returns whether a barrier is currently armed on this player. */
    bool hasBarrier() const { return _hasBarrier; }
    
    /** Returns the current barrier multiplier. */
    float getBarrierMultiplier() const { return _barrierMultiplier; }
    
    /** Returns the remaining barrier duration. */
    float getBarrierDuration() const { return _barrierDuration; }

    /** Returns whether a regen effect is currently active on this player. */
    bool hasRegen() const { return _hasRegen; }

    /** Returns the total healing still left to apply for the active regen. */
    float getRegenAmountRemaining() const { return _regenAmountRemaining; }

    /** Returns the remaining regen duration. */
    float getRegenDuration() const { return _regenDuration; }

    /**
     * Returns the number of prior mallet uses recorded for this player this round.
     *
     * @return The number of completed mallet uses tracked for this player in the current round.
     */
    int getMalletUseCount() const { return _malletUseCount; }

    /*Setter for current health*/
    void setCurrentHealth(float health) { _currentHealth = health; }

    /**
     * Overwrites runtime support-effect state from the authoritative host snapshot.
     *
     * @param shieldHealth  The fixed damage amount blocked by the active shield.
     * @param shieldDuration    The remaining shield duration in seconds.
     * @param barrierMultiplier The active barrier damage multiplier.
     * @param barrierDuration   The remaining barrier duration in seconds.
     * @param regenAmountRemaining The remaining total healing to apply from regen.
     * @param regenDuration   The remaining regen duration in seconds.
     */
    void syncRuntimeEffects(float shieldHealth, float shieldDuration, float barrierMultiplier,
        float barrierDuration, float regenAmountRemaining, float regenDuration) {
        _hasShield = shieldDuration > 0.0f;
        _shieldHealth = _hasShield ? shieldHealth : 0.0f;
        _shieldDuration = _hasShield ? shieldDuration : 0.0f;
        _hasBarrier = barrierDuration > 0.0f;
        _barrierMultiplier = _hasBarrier ? barrierMultiplier : 1.0f;
        _barrierDuration = _hasBarrier ? barrierDuration : 0.0f;
        _hasRegen = regenDuration > 0.0f && regenAmountRemaining > 0.0f;
        _regenAmountRemaining = _hasRegen ? regenAmountRemaining : 0.0f;
        _regenDuration = _hasRegen ? regenDuration : 0.0f;
    }
    
    /**
     * Returns the path of the spritesheet for the house
     */
    std::string getSpritesheetPath() const { return _spritesheetPath; }
    
    /**
     * Returns the list of the house special ability items
     */
    std::vector<std::string> getSpecialAbilities() const { return _specialAbilities; }
    
    /**
     * Returns the player to the left of this player, or nullptr if none.
     */
    Player* getLeftPlayer() const { return _leftPlayer; }

    /**
     * Returns the player to the right of this player, or nullptr if none.
     */
    Player* getRightPlayer() const { return _rightPlayer; }
    
    /**
     * Returns whether this player is AI or not.
     */
    virtual bool isAI() const { return false; }
    
#pragma mark Gameplay
    /**
     * Updates the current health of the player by the given delta.
     * Positive values heal, negative values deal damage.
     * Health is clamped between 0 and maxHealth.
     * @param delta     The amount to change health by
     */
    
    void updateHealth(float delta);

    /**
     * Applies a shield to this player. Replaces any existing shield.
     *
     * @param mitigation  The amount of damage the shield blocks
     * @param duration      How long the shield will stay up for
     */
    void applyShield(float mitigation, float duration);

    /**
     * Applies a timed percentage-mitigation barrier to this player.
     *
     * @param multiplier  The percentage multiplier for incoming damage.
     * @param duration      How long the barrier will stay up for
     */
    void applyBarrier(float multiplier, float duration);

    /**
     * Applies a timed heal-over-time effect to this player.
     *
     * Current `regenDuration` and `regenAmountRemaining` are completely
     * overridden when this function is called when this player already has active regen.
     *
     * @param amount    The total healing to apply over the full duration.
     * @param duration  How long the regen lasts.
     */
    void applyRegen(float amount, float duration);
    
    /**
     * Advances this player's active runtime support effects by the elapsed frame time.
     *
     * Both shield and barrier durations are reduced by `dt` and clamped to `0.0f` so
     * they never become negative. When a shield timer reaches zero, the shield is marked
     * inactive, any remaining flat damage absorption is cleared, and an expiration log is
     * emitted. When a barrier timer reaches zero, the barrier is marked inactive and its
     * damage multiplier is restored to the neutral `1.0f` value.
     *
     * @param dt  The elapsed time since the previous frame, in seconds.
     */
    void updateEffects(float dt);

    /** Clears runtime-only combat effects. */
    void clearRuntimeEffects();

    /** Clears round-scoped item-use state. */
    void clearItemUseState() { _malletUseCount = 0; }

    /**
     * Overwrites the authoritative mallet use count replicated from the host.
     *
     * @param useCount The host-replicated number of completed mallet uses for this player.
     */
    void setMalletUseCount(int useCount) { _malletUseCount = std::max(0, useCount); }

    /**
     * Computes the final magnitude of an item use after house, affinity, and item-specific bonuses.
     *
     * @param def The item definition being resolved.
     * @param db The item database that provides multiplier metadata.
     * @return The resolved magnitude for this player and item.
     */
    float resolveItemMagnitude(const ItemDef& def, const ItemDatabase& db) const;

    /**
     * Records any round-scoped state advance caused by consuming the given item.
     *
     * @param def The item definition that was just consumed.
     */
    void recordItemUse(const ItemDef& def);

    /**
     * Adds an item to the player's inventory.
     * @param item      The item to add
     */
    void addItem(const ItemInstance& item);
    
    /**
     * Returns whether the player is alive or not
     */
    bool isAlive() const;
    
    /**
     * Uses the inventory item with the given id on a player target.
     *
     * Support items heal the target using the resolved item magnitude, while any
     * configured item effects are dispatched through the effect system. The item
     * is removed from inventory once used.
     *
     * @param itemId  The inventory instance id to consume
     * @param target  The player that receives the item's healing and effects
     * @param db           The item database used to resolve the item definition
     * @return       The applied base magnitude, or -1.0f if the item id or item
     *         definition cannot be found
     */
    float useItemById(ItemInstance::ItemId itemId, Player& target, const ItemDatabase& db);

    /**
     * Uses the inventory item with the given id on an enemy target.
     *
     * Attack items damage the target using the resolved item magnitude, while
     * any configured item effects are dispatched through the effect system. The
     * item is removed from inventory once used.
     *
     * @param itemId  The inventory instance id to consume
     * @param target  The enemy that receives the item's damage and effects
     * @param db           The item database used to resolve the item definition
     * @return       The applied base magnitude, or -1.0f if the item id or item definition cannot be found
     */
    float useItemById(ItemInstance::ItemId itemId, Enemy& target, const ItemDatabase& db);
    /**
     * Removes all items from the player's inventory.
     *
     * Called when the scene resets to ensure no stale items persist
     * into the next round. Any widgets synced from this inventory
     * will be removed on the next syncInventoryWidgets() call.
     */
    void clearInventory() {
        _inventory.clear();
    }
    
    /**
     * Removes an item from the player's inventory by item id.
     * @param itemId    The item to remove
     */
    void removeItemById(ItemInstance::ItemId itemId);
    
    /**
     * Sets the player to the left of this player.
     * @param player    The player to the left, or nullptr to clear.
     */
    void setLeftPlayer(Player* player) { _leftPlayer = player; }

    /**
     * Sets the player to the right of this player.
     * @param player    The player to the right, or nullptr to clear.
     */
    void setRightPlayer(Player* player) { _rightPlayer = player; }
    
    /**
     * Sets the display name of the player.
     * Used when a networked player joins before selecting a house,
     * so their name can be shown in the lobby without reconstructing
     * the Player object.
     *
     * @param name  The display name to assign.
     */
    void setPlayerName(const std::string& name) { _playerName = name; }

    /**
     * Sets the player number (slot index) of this player.
     * Called after a scramble to keep the slot index
     * consistent with the player's new position in the ring.
     *
     * @param number  The new zero-based slot index to assign.
     */
    void setPlayerNumber(int number) { _playerNumber = number; }
    
};
#endif /* !__PLAYER_H__ */

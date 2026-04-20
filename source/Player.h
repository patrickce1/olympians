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
#include <type_traits>

/**
 * Model Class representing the Player
 */
class Player {
private:
    
    /** The inventory of the player stored as a vector of ItemInstance objects*/
    std::vector<ItemInstance> _inventory;
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
    /** Runtime percentage-mitigation barrier state */
    bool _hasBarrier = false;
    /** The percentage damage that will be mitigated */
    float _barrierMultiplier = 1.0f;
    /** The time left before the barrier expires */
    float _barrierDuration = 0.0f;

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

    /*Setter for current health*/
    void setCurrentHealth(float health) { _currentHealth = health; }

    /**
     * Overwrites runtime support-effect state from the authoritative host snapshot.
     *
     * @param shieldHealth  The fixed damage amount blocked by the active shield.
     * @param shieldDuration    The remaining shield duration in seconds.
     * @param barrierMultiplier The active barrier damage multiplier.
     * @param barrierDuration   The remaining barrier duration in seconds.
     */
    void syncRuntimeEffects(float shieldHealth, float shieldDuration, float barrierMultiplier,
        float barrierDuration) {
        _hasShield = shieldDuration > 0.0f;
        _shieldHealth = _hasShield ? shieldHealth : 0.0f;
        _shieldDuration = _hasShield ? shieldDuration : 0.0f;
        _hasBarrier = barrierDuration > 0.0f;
        _barrierMultiplier = _hasBarrier ? barrierMultiplier : 1.0f;
        _barrierDuration = _hasBarrier ? barrierDuration : 0.0f;
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
    
};
#endif /* !__PLAYER_H__ */

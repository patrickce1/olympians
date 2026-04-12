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
    float _shieldMitigation = 0.0f;
    float _shieldDuration = 0.0f;
    /** Runtime percentage-mitigation barrier state */
    bool _hasBarrier = false;
    float _barrierMultiplier = 1.0f;
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
    float getShieldMitigation() const { return _shieldMitigation; }
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

    /** Applies a timed fixed-mitigation shield to this player. */
    void applyShield(float mitigation, float duration);

    /** Applies a timed percentage-mitigation barrier to this player. */
    void applyBarrier(float multiplier, float duration);
    void updateEffects(float dt);

    /** Clears runtime-only combat effects. */
    void clearRuntimeEffects() {
        _shieldDuration = 0.0f;
        _hasBarrier = false;
    }

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
     * Uses an item from the player's inventory on a player target.
     *
     * Matching support items always apply their resolved heal amount first, then
     * layer any configured support effects on top. Attack items used on a player
     * remain a mismatch and return 0 after consumption.
     *
      * @param itemId  The inventory instance id to consume
     * @return resolved base heal magnitude for matching support items, 0 if consumed
     *         but no matching target type, -1 on failure
     */
    template <typename T>
    float useItemById(ItemInstance::ItemId itemId, T& target, const ItemDatabase& db) {
        return -1.0f;
    }
    float useItemById(ItemInstance::ItemId itemId, Player& target, const ItemDatabase& db);

    /**
     * Uses an item from the player's inventory on an enemy target.
     *
     * Matching attack items always apply their resolved damage first, then layer
     * any configured enemy-facing effects on top. Support items used on an enemy
     * remain a mismatch and return 0 after consumption.
     *
     * @param itemId  The inventory instance id to consume
     * @return resolved base damage magnitude for matching attack items, 0 if consumed
     *         but no matching target type, -1 on failure
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

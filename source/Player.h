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
    /** The house name for the house the player is using*/
    std::string _houseId;
    /** The house that the house hails from*/
    int _house;
    /** The max health of the house and player*/
    float _maxHealth;
    /** The current health of the house and player*/
    float _currentHealth;
    /** The  ability class of the house*/
    HouseLoader::AbilityClass _abilityClass;
    /** The spritesheet correlating to this house*/
    std::string _spritesheetPath;
    /** The list of the house special abilities*/
    std::vector<std::string> _specialAbilities;
    /** The player to the left of this player, or nullptr if none */
    Player* _leftPlayer = nullptr;
    /** The player to the right of this player, or nullptr if none */
    Player* _rightPlayer = nullptr;
    
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
     * Return the max health of the house/player
     */
    float getMaxHealth() const { return _maxHealth; }
    
    /**
     * Return the current health of the house/player
     */
    float getCurrentHealth() const { return _currentHealth; }

    /*Setter for current health*/
    void setCurrentHealth(float health) { _currentHealth = health; }
    
    /**
     * Returns the ability class of the house
     */
    HouseLoader::AbilityClass getAbilityClass() const { return _abilityClass; }
    
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
     * Adds an item to the player's inventory.
     * @param item      The item to add
     */
    void addItem(const ItemInstance& item);
    
    /**
     * Returns whether the player is alive or not
     */
    bool isAlive() const;
    
    /**
     * Uses  an item from the player's inventory by item id.
     * @param item    The item to use
     * @return true if the item was found and used, false otherwise
     */
    template <typename T>
    bool useItemById(ItemInstance::ItemId itemId, T& target, const ItemDatabase& db) {
        for (auto item = _inventory.begin(); item != _inventory.end(); ++item) {
            if (item->getId() == itemId) {
                std::shared_ptr<ItemDef> def = db.getDef(item->getDefId());
                if (!def) {
                    CULogError("Player: could not find ItemDef for defId '%s'", item->getDefId().c_str());
                    return false;
                }

                if constexpr (std::is_same<T, Player>::value) {
                    if (def->getType() == ItemDef::Type::Support) {
                        target.updateHealth(def->getEffectiveValue());
                    }
                }
                else if constexpr (std::is_same<T, Enemy>::value) {
                    if (def->getType() == ItemDef::Type::Attack) {
                        target.updateHealth(-def->getEffectiveValue());
                    }
                }

                _inventory.erase(item);
                return true;
            }
        }
        return false;
    }
    
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
     * Removes  an item from the player's inventory by item id.
     * @param item    The item to remove
     * @param target    The target to apply the item to (Player or Enemy)
     * @return true if the item was found and removed, false otherwise
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
    
};
#endif /* !__PLAYER_H__ */


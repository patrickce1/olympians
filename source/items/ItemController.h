#ifndef __ITEM_CONTROLLER_H__
#define __ITEM_CONTROLLER_H__
#include <cstddef>
#include <unordered_map>
#include <cugl/cugl.h>
#include <vector>
#include "ItemDatabase.h"
#include "ItemInstance.h"
#include "../Player.h"

class ItemController {
private:
    // Instance of the item Database
    ItemDatabase _itemDb;
    // Instance of the item ID generator
    ItemInstance::IdGenerator _idGen;
    // The item interval that determines how long you have to wait till receiving another item
    float _itemInterval = 0.0f;
    // The timer value each player starts a round with
    float _itemTimerStart = 0.0f;
    // Per-player item timers keyed by stable player slot number
    std::unordered_map<int, float> _itemTimers;
    // Maximum number of items allowed for timer-based spawning
    std::size_t _maxInventorySpawnItems = 5;

public:
    ItemController() = default;

    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::string& jsonKey = "items");

    /**
     * Update timers and hand out an item when item interval is ready
     *
     * @param dt  Time elapsed
     * @param player   The player to give the item to
     */
    void update(float dt, Player* player);

    /**
     * Resets round-scoped spawn state.
     */
    void reset();

    /**
     * Give a random item to the player, only if the player has no more
     * than 5 items in their inventory.
     *
     * @param player   The player to give the item to
     */
    void giveRandomItem(Player* player);

    /**
     * Restricts divine item rolls to items whose houseAffinity matches one of the given
     * house IDs. Call once after the player roster is known. Forwards to ItemDatabase.
     *
     * @param houseIds  Player house ID strings (e.g. "zeus", "poseidon")
     */
    void setActiveHouses(const std::vector<std::string>& houseIds) {
        _itemDb.setActiveHouses(houseIds);
    }

    /*
    * Retrieves the item database
    * @return the item database
    */
    const ItemDatabase& getDatabase() const { return _itemDb; }
    
    /**
     * Creates an item instance from a definition ID and adds it to the player's inventory.
     * Returns the unique ID of the created item for tracking purposes.
     * 
     * @param player      The player receiving the item
     * @param itemDefId   The definition ID of the item to create
     * @return            The ItemId of the newly created item, or 0 if creation failed
     */
    ItemInstance::ItemId giveItemByID(Player* player, const std::string& itemDefId);
};

#endif // __ITEM_CONTROLLER_H__

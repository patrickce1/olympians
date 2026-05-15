#ifndef __ITEM_CONTROLLER_H__
#define __ITEM_CONTROLLER_H__
#include <cstddef>
#include <cstdint>
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
    // Active host-authoritative item interval override from frenzy
    float _frenzyItemInterval = 0.0f;
    // Remaining duration of the active frenzy item interval override
    float _frenzyDuration = 0.0f;
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
     * @param blockSpawn  If true, advances the timer but suppresses item spawning
     */
    void update(float dt, Player* player, bool blockSpawn = false);

    /**
     * Advances timed item-spawn controller effects.
     *
     * @param dt Time elapsed in seconds.
     */
    void updateEffects(float dt);

    /**
     * Resets round-scoped spawn state.
     */
    void reset();

    /**
     * Applies a timed frenzy override to item spawning.
     *
     * @param itemInterval New item spawn interval for the duration.
     * @param duration Duration of the override in seconds.
     */
    void applyFrenzy(float itemInterval, float duration);

    /**
     * Synchronizes frenzy state from the host.
     *
     * @param itemInterval Host-authoritative frenzy item interval.
     * @param duration Remaining host-authoritative frenzy duration.
     */
    void syncFrenzy(float itemInterval, float duration);

    /**
     * Returns the currently effective item spawn interval.
     *
     * @return The frenzy interval while active, otherwise the default item interval.
     */
    float getEffectiveItemInterval() const;

    /**
     * Returns the host-authoritative frenzy interval, or 0 when inactive.
     *
     * @return The current frenzy item interval in seconds.
     */
    float getFrenzyItemInterval() const { return _frenzyItemInterval; }

    /**
     * Returns the remaining frenzy duration in seconds.
     *
     * @return The remaining frenzy duration in seconds.
     */
    float getFrenzyDuration() const { return _frenzyDuration; }

    /**
     * Returns whether a frenzy item interval override is active.
     *
     * @return True when frenzy has a positive interval and remaining duration.
     */
    bool hasFrenzy() const { return _frenzyDuration > 0.0f && _frenzyItemInterval > 0.0f; }

    /**
     * Gives a random item to the player.
     *
     * Normal timer spawns respect the max inventory cap. During frenzy, the cap
     * is ignored so the temporary faster spawn rate can continue adding items.
     *
     * @param player The player to give the item to.
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

    /**
     * Redefines qualifying item instances for the forge effect without changing item IDs or slots.
     * Common items become random rare items; rare items may become random divine items.
     *
     * @param player        The player whose existing inventory should be transformed.
     * @param divineChance  Chance in [0, 1] that each rare item upgrades to divine.
     * @param seed          Host-authoritative seed used for deterministic local rolls.
     * @return              Number of item instances redefined.
     */
    int applyForgeEffect(Player* player, float divineChance, std::uint32_t seed);
};

#endif // __ITEM_CONTROLLER_H__

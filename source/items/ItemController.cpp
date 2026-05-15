#include "ItemController.h"
#include <algorithm>

using namespace cugl;

bool ItemController::init(const std::shared_ptr<AssetManager>& assets,
                          const std::string& jsonKey) {

    std::string path = "json/items.json";
    std::string housesPath = "json/houses.json";

    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        CULog("ItemController: failed to open %s", path.c_str());
        return false;
    }

    auto itemsJson = reader->readJson();
    if (!itemsJson) {
        CULog("ItemController: failed to parse %s", path.c_str());
        return false;
    }

    // the item array
    auto itemArray = itemsJson->get("items");
    if (!itemArray || !itemArray->isArray()) {
        CULog("ItemController: items.json missing 'items' array");
        return false;
    }

    // Create Item Database
    if (!_itemDb.loadFromJson(itemsJson)) {
        CULog("ItemController: failed to load item database");
        return false;
    }

    auto houseReader = cugl::JsonReader::alloc(housesPath);
    if (!houseReader) {
        CULog("ItemController: failed to open %s", housesPath.c_str());
        return false;
    }

    auto housesJson = houseReader->readJson();
    if (!housesJson) {
        CULog("ItemController: failed to parse %s", housesPath.c_str());
        return false;
    }

    if (!_itemDb.loadHouseMultipliersFromJson(housesJson)) {
        CULog("ItemController: failed to load house multipliers");
        return false;
    }

    // Read itemInterval from JSON if present, otherwise return error
    if (itemsJson->has("itemInterval") && itemsJson->get("itemInterval")->isNumber()) {
        _itemInterval = itemsJson->get("itemInterval")->asFloat();
    } else {
        CULogError("No item interval was specified");
    }

    // Read itemTimerStart from JSON if present, otherwise return error
    if (itemsJson->has("itemTimerStart") && itemsJson->get("itemTimerStart")->isNumber()) {
        _itemTimerStart = itemsJson->get("itemTimerStart")->asFloat();
    } else {
        CULogError("No item timer start was specified");
    }

    // Read maxInventorySpawnItems from JSON if present, otherwise keep default
    if (itemsJson->has("maxInventorySpawnItems") && itemsJson->get("maxInventorySpawnItems")->isNumber()) {
        int parsedCap = itemsJson->getInt("maxInventorySpawnItems");
        if (parsedCap >= 0) {
            _maxInventorySpawnItems = static_cast<std::size_t>(parsedCap);
        } else {
            CULogError("maxInventorySpawnItems must be non-negative. Using default cap %zu", _maxInventorySpawnItems);
        }
    } else {
        CULog("ItemController: maxInventorySpawnItems not specified. Using default cap %zu", _maxInventorySpawnItems);
    }

    _idGen.startGame(ItemInstance::IdGenerator::randomGameId());
    _itemDb.setStartingPointWithTime();
    reset();

    return true;
}

/**
 * Resets round-scoped spawn state.
 */
void ItemController::reset() {
    _itemTimers.clear();
    _frenzyItemInterval = 0.0f;
    _frenzyDuration = 0.0f;
}

/**
 * Advances timed item-spawn controller effects.
 *
 * @param dt Time elapsed in seconds.
 */
void ItemController::updateEffects(float dt) {
    if (_frenzyDuration <= 0.0f) {
        return;
    }

    _frenzyDuration = std::max(0.0f, _frenzyDuration - dt);
    if (_frenzyDuration <= 0.0f) {
        _frenzyItemInterval = 0.0f;
    }
}

/**
 * Applies a timed frenzy override to item spawning.
 *
 * @param itemInterval New item spawn interval for the duration.
 * @param duration Duration of the override in seconds.
 */
void ItemController::applyFrenzy(float itemInterval, float duration) {
    _frenzyItemInterval = std::max(0.0f, itemInterval);
    _frenzyDuration = std::max(0.0f, duration);
}

/**
 * Synchronizes frenzy state from the host.
 *
 * @param itemInterval Host-authoritative frenzy item interval.
 * @param duration Remaining host-authoritative frenzy duration.
 */
void ItemController::syncFrenzy(float itemInterval, float duration) {
    applyFrenzy(itemInterval, duration);
}

/**
 * Returns the currently effective item spawn interval.
 *
 * @return The frenzy interval while active, otherwise the default item interval.
 */
float ItemController::getEffectiveItemInterval() const {
    return hasFrenzy() ? _frenzyItemInterval : _itemInterval;
}

/**
 * Update timers and hand out an item when item interval is ready
 *
 * @param dt  Time elapsed
 * @param player   The player to give the item to
 */
void ItemController::update(float dt, Player* player, bool blockSpawn) {
    const float itemInterval = getEffectiveItemInterval();
    if (!player || itemInterval <= 0.0f) {
        return;
    }

    auto [it, inserted] = _itemTimers.emplace(player->getPlayerNumber(), _itemTimerStart);
    float& itemTimer = it->second;
    itemTimer += dt;

    if (blockSpawn) return;

    while (itemTimer >= itemInterval) {
        itemTimer -= itemInterval;
        giveRandomItem(player);
    }
}

/**
 * Gives a random item to the player.
 *
 * Normal timer spawns respect the max inventory cap. During frenzy, the cap
 * is ignored so the temporary faster spawn rate can continue adding items.
 *
 * @param player The player to give the item to.
 */
void ItemController::giveRandomItem(Player* player) {
    if (!player) {
        CULog("[ItemController] Player is null");
        return;
    }

    // Check if player is alive
    if (!player->isAlive()) {
        CULog("[ItemController] Player is not alive");
        return;
    }

    // Check if player has too many items, unless frenzy is overriding spawn rules.
    if (!hasFrenzy() && player->getInventory().size() >= _maxInventorySpawnItems) {
        CULog("[ItemController] Spawn skipped: inventory size %zu is at or above cap %zu",
              player->getInventory().size(), _maxInventorySpawnItems);
        return;
    }

    // Gets defId of the random item generated
    std::string itemDefId = _itemDb.rollRandomDefId();
    if (itemDefId.empty()) {
        CULog("[ItemController] DefId is empty");
        return;
    }

    // Creates instance of generated defId
    auto itemInstance = _itemDb.createInstance(itemDefId, _idGen.next());
    if (!itemInstance) {
        CULog("[ItemController] Failed to create itemInstance");
        return;
    }

    player->addItem(*itemInstance);
}

/**
 * Gives a specific item to the player by its definition ID.
 * Used when a passed item needs to be added to a player's inventory,
 * since the item definition ID is what travels over the network.
 *
 * Unlike giveRandomItem(), this method bypasses both the inventory cap check
 * and the alive status check, since a passed item should always be deliverable
 * to its recipient regardless of whether they're dead. (Dead players can't use
 * items, but they can receive passes to hold.)
 *
 * @param player    The player receiving the item.
 * @param itemDefId The definition ID string of the item to give.
 * @return          The ItemId of the newly created item, or 0 if creation failed.
 */
ItemInstance::ItemId ItemController::giveItemByID(Player* player, const std::string& itemDefId) {
    if (!player) {
        CULog("[ItemController] giveItemById: player is null");
        return 0;
    }

    if (itemDefId.empty()) {
        CULog("[ItemController] giveItemById: itemDefId is empty");
        return 0;
    }

    ItemInstance::ItemId itemId = _idGen.next();
    auto itemInstance = _itemDb.createInstance(itemDefId, itemId);
    if (!itemInstance) {
        CULog("[ItemController] giveItemById: failed to create instance for defId '%s'", itemDefId.c_str());
        return 0;
    }

    player->addItem(*itemInstance);
    return itemId;
}

/**
 * Redefines qualifying item instances for the forge effect without changing item IDs or slots.
 * Common items become random rare items; rare items may become random divine items.
 *
 * @param player        The player whose existing inventory should be transformed.
 * @param divineChance  Chance in [0, 1] that each rare item upgrades to divine.
 * @param seed          Host-authoritative seed used for deterministic local rolls.
 * @return              Number of item instances redefined.
 */
int ItemController::applyForgeEffect(Player* player, float divineChance, std::uint32_t seed) {
    if (!player) {
        return 0;
    }

    cugl::Random rng;
    rng.initWithSeed(static_cast<Uint64>(seed));

    int redefinedCount = 0;
    auto& inventory = const_cast<std::vector<ItemInstance>&>(player->getInventory());
    for (ItemInstance& item : inventory) {
        auto currentDef = _itemDb.getDef(item.getDefId());
        if (!currentDef) {
            continue;
        }

        std::string replacementDefId;
        if (currentDef->getRarity() == ItemDef::Rarity::Common) {
            replacementDefId = _itemDb.rollRandomDefId(ItemDef::Rarity::Rare, rng);
        } else if (currentDef->getRarity() == ItemDef::Rarity::Rare && rng.getRightOpenDouble(0.0, 1.0) < divineChance) {
            replacementDefId = _itemDb.rollRandomDefId(ItemDef::Rarity::Divine, rng);
        }

        if (replacementDefId.empty() || replacementDefId == item.getDefId()) {
            continue;
        }

        if (item.setDefId(replacementDefId)) {
            redefinedCount++;
        }
    }

    return redefinedCount;
}

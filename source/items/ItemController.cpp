#include "ItemController.h"

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
        _itemTimer = itemsJson->get("itemTimerStart")->asFloat();
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

    return true;
}

// Update the item timer and hand out a card
void ItemController::update(float dt, Player* player) {
    _itemTimer += dt;

    while (_itemTimer >= _itemInterval) {
        _itemTimer -= _itemInterval;
        giveRandomItem(player);
    }
}

// Hand out an item to the player
void ItemController::giveRandomItem(Player* player) {
    // Check if player is alive
    if (!player->isAlive()) {
        CULog("[ItemController] Player is not alive");
        return;
    }

    // Check if player has too many items
    if (player->getInventory().size() >= _maxInventorySpawnItems) {
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
 * Unlike giveRandomItem(), this method bypasses the inventory cap check
 * since a passed item should always be deliverable to its recipient.
 *
 * @param player    The player receiving the item.
 * @param itemDefId The definition ID string of the item to give.
 */
void ItemController::giveItemByID(Player* player, const std::string& itemDefId) {
    if (!player->isAlive()) {
        CULog("[ItemController] Player is not alive");
        return;
    }

    if (itemDefId.empty()) {
        CULog("[ItemController] giveItemById: itemDefId is empty");
        return;
    }

    auto itemInstance = _itemDb.createInstance(itemDefId, _idGen.next());
    if (!itemInstance) {
        CULog("[ItemController] giveItemById: failed to create instance for defId '%s'", itemDefId.c_str());
        return;
    }

    player->addItem(*itemInstance);
}


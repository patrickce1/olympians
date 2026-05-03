#include "ItemDatabase.h"
#include <cctype>

using namespace cugl;

/**
 * Normalizes a house ID by trimming whitespace and converting to lowercase.
 * Enables case-insensitive house lookups regardless of JSON formatting.
 *
 * @param houseID  The house ID string to normalize
 * @return the normalized ID (trimmed and lowercased)
 */
static std::string normalizeHouseID(std::string houseID) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    houseID.erase(houseID.begin(), std::find_if(houseID.begin(), houseID.end(), notspace));
    houseID.erase(std::find_if(houseID.rbegin(), houseID.rend(), notspace).base(), houseID.end());
    std::transform(houseID.begin(), houseID.end(), houseID.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return houseID;
}

/**
 * Clamps a value to the range [0.0, 1.0].
 * Used internally to validate multiplier slider values.
 *
 * @param value  The value to clamp
 * @return the clamped value (0.0 if value < 0.0, 1.0 if value > 1.0, otherwise value)
 */
static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/** Clears items from buckets and reinitializes them; buckets contain items of the corresponding rarity */
void ItemDatabase::clearBuckets() {
    _bucketsByRarity.clear();
    _bucketsByRarity[ItemDef::Rarity::Common]    = Bucket();
    _bucketsByRarity[ItemDef::Rarity::Rare]      = Bucket();
    _bucketsByRarity[ItemDef::Rarity::Divine]    = Bucket();
    _bucketsByRarity[ItemDef::Rarity::Special]   = Bucket();
}

/** Clears buckets and the item database collection */
void ItemDatabase::clear() {
    _defs.clear();
    _houseMultipliers.clear();
    clearBuckets();
    _activeHouses.clear();
    _filteredDivineBucket = Bucket();
    _hasActiveHouseFilter = false;
}

/** Seed options; seed acts as the starting point for the RNG. The game's seed is generated at random, so it is unlikely
 that two parties in the same local network generate the same seed and therefore the same RNG pattern. */

/** Deterministic seed (recommended for host) */
void ItemDatabase::setStartingPoint(std::uint64_t seed) {
    _rng.initWithSeed((Uint64)seed);
    _rngReady = true;
}

/** Time-based seed (okay for local) */
void ItemDatabase::setStartingPointWithTime() {
    _rng.init();
    _rngReady = true;
}

/** Returns the definition of the given defId */
std::shared_ptr<ItemDef> ItemDatabase::getDef(const std::string& defId) const {
    auto itemDef = _defs.find(defId);
    return (itemDef == _defs.end() ? nullptr : itemDef->second);
}

/** Creates an instance of the item with the given defId, and the instance is given id */
std::shared_ptr<ItemInstance> ItemDatabase::createInstance(const std::string& defId,
                                                           ItemInstance::ItemId id) const {
    if (!getDef(defId)) return nullptr;
    return ItemInstance::alloc(defId, id);
}

/** In case of a reset, reset to fallback values */
void ItemDatabase::resetRarityWeights() {
    _rarityWeights.clear();
    _rarityWeights[ItemDef::Rarity::Common]    = 0.45;
    _rarityWeights[ItemDef::Rarity::Rare]      = 0.40;
    _rarityWeights[ItemDef::Rarity::Divine]    = 0.15;
    // Fallbacks already sum to 1.0, so normalization is a no-op for them
}

/** Load rarity weights from a JSON and normalize so they sum to 1.0 */
void ItemDatabase::loadRarityWeights(const std::shared_ptr<JsonValue>& json) {
    resetRarityWeights();

    if (!json || !json->isObject()) return;
    if (!json->has("rarityWeights")) return;
    if (!json->get("rarityWeights")->isObject()) return;

    auto rarityWeightsJson = json->get("rarityWeights");

    auto loadOne = [&](const char* key, ItemDef::Rarity rarity) {
        if (rarityWeightsJson->has(key) && rarityWeightsJson->get(key)->isNumber()) {
            double weight = rarityWeightsJson->get(key)->asDouble();
            _rarityWeights[rarity] = (weight > 0.0) ? weight : 0.0;
        }
    };

    loadOne("common",    ItemDef::Rarity::Common);
    loadOne("rare",      ItemDef::Rarity::Rare);
    loadOne("divine",    ItemDef::Rarity::Divine);
    loadOne("special",   ItemDef::Rarity::Special);

    // Normalize so weights sum to 1.0 — values can be any positive numbers in JSON
    double total = 0.0;
    for (const auto& rarityWeight : _rarityWeights) total += rarityWeight.second;
    if (total > 0.0) {
        for (auto& rarityWeight : _rarityWeights) rarityWeight.second /= total;
    }
}

/** Add item with the given defId to the corresponding bucket with effectiveWeight
 *  Total = sum of effective weights of all the defIds addet to the bucket
 *  Prefix = cummulative sum array of effective weights of added defIds
 *  Each entry prefix[i] = sum of effectiveWeights[0..i]. Given i is the number of defIds entered.
 */
void ItemDatabase::addToBucket(Bucket& bucket, const std::string& defId, double effectiveWeight) {
    // Only include spawnable items (effWeight > 0)
    if (effectiveWeight <= 0.0) return;

    bucket.defIds.push_back(defId);
    bucket.total += effectiveWeight;
    bucket.prefix.push_back(bucket.total);
}

/**
 * Rolls a random item defID from the given bucket using weighted random selection.
 *
 * A random value is sampled uniformly in [0, total) and binary-searched
 * against the bucket's prefix sum array to select an item proportional
 * to its weight. Items with higher weights are more likely to be selected.
 *
 * @param bucket    The bucket to roll from
 * @return the defId of the selected item, or "" if the bucket is empty
 */
std::string ItemDatabase::rollFromBucket(const Bucket& bucket) {
    if (bucket.defIds.empty() || bucket.total <= 0.0) return "";

    if (!_rngReady) {
        // Lazy seed if user forgot to seed explicitly
        const_cast<ItemDatabase*>(this)->setStartingPointWithTime();
    }

    // Pick a random value in [0, total) — this is our "dart throw" into the weight space
    double randVal = _rng.getRightOpenDouble(0.0, bucket.total);

    // Binary search the prefix sum array for the first entry greater than r (std::upper_bound).
    auto bucketItem = std::upper_bound(bucket.prefix.begin(), bucket.prefix.end(), randVal);
    
    // The index of that entry corresponds to the selected item, since each
    // prefix[i] marks the upper boundary of item i's weight range.
    std::size_t prefixIdx = (std::size_t)std::distance(bucket.prefix.begin(), bucketItem);
    
    // Clamp to valid range as a safety measure against floating point edge cases
    if (prefixIdx >= bucket.defIds.size()) prefixIdx = bucket.defIds.size() - 1;

    return bucket.defIds[prefixIdx];
}

/**
* Loads item defs from JSON.
* Expected schema: { "items": [ { ...ItemDef... }, ... ] }
*/
bool ItemDatabase::loadFromJson(const std::shared_ptr<JsonValue>& json) {
    clear();

    if (!json || !json->isObject()) return false;
    if (!json->has("items") || !json->get("items")->isArray()) return false;

    clearBuckets();
    loadRarityWeights(json);

    if (_rarityWeights.empty()) return false;

    auto itemArray = json->get("items");
    for (int itemIndex = 0; itemIndex < itemArray->size(); itemIndex++) {
        auto itemEntry = itemArray->get(itemIndex);
        auto itemDef = ItemDef::alloc(itemEntry);
        if (!itemDef) {
            CULog("ItemDatabase: failed to parse item at index %d", itemIndex);
            continue;
        }

        const std::string& defID = itemDef->getId();
        if (_defs.find(defID) != _defs.end()) {
            CULog("ItemDatabase: duplicate item id '%s' (index %d). Overwriting.", defID.c_str(), itemIndex);
        }
        _defs[defID] = itemDef;

        // Add to the per-rarity bucket weighted by the item's own weight field
        addToBucket(_bucketsByRarity[itemDef->getRarity()], defID, (double)itemDef->getWeight());
    }

    return !_defs.empty();
}

/**
 * Loads house multiplier data from a JSON object.
 * Expected schema: { "houses": [ { "id": string, "attack": number?, "support": number?, "affinityBonus": number? }, ... ] }
 * Missing slider values default to 0.0, clamped to [0.0, 1.0]. Missing affinityBonus defaults to 1.5.
 *
 * @param json  The JSON object to parse
 * @return true if at least one house was successfully parsed, false otherwise
 */
bool ItemDatabase::loadHouseMultipliersFromJson(const std::shared_ptr<JsonValue>& json) {
    _houseMultipliers.clear();

    // Validate root object and "houses" array exist
    if (!json || !json->isObject()) return false;
    auto houses = json->get("houses");
    if (!houses || !houses->isArray()) return false;

    // Process each house entry in the array
    for (int houseIndex = 0; houseIndex < houses->size(); ++houseIndex) {
        auto houseEntry = houses->get(houseIndex);
        // Skip if entry is not an object
        if (!houseEntry || !houseEntry->isObject()) {
            continue;
        }
        // Skip if "id" field is missing or not a string
        if (!houseEntry->has("id") || !houseEntry->get("id")->isString()) {
            continue;
        }

        // Normalize the house ID (trim whitespace, lowercase) for case-insensitive lookup
        const std::string houseID = normalizeHouseID(houseEntry->getString("id", ""));
        if (houseID.empty()) {
            continue;
        }

        HouseMultipliers multipliers;

        // Helper lambda to safely read slider values: if field is missing or invalid, use fallback;
        // otherwise, clamp the value to [0.0, 1.0] to enforce slider bounds
        auto readSlider = [&](const char* key, float fallback) {
            if (!houseEntry->has(key) || !houseEntry->get(key)->isNumber()) {
                return fallback;
            }
            float value = houseEntry->getFloat(key);
            return clamp01(value);
        };

        // Load attack and support sliders with default of 0.0 if missing
        multipliers.attack = readSlider("attack", 0.0f);
        multipliers.support = readSlider("support", 0.0f);

        // Load affinityBonus: if missing or invalid, default to 1.5; if present but <= 0, also default to 1.5
        if (houseEntry->has("affinityBonus") && houseEntry->get("affinityBonus")->isNumber()) {
            multipliers.affinityBonus = houseEntry->getFloat("affinityBonus");
            if (multipliers.affinityBonus <= 0.0f) {
                multipliers.affinityBonus = 1.5f;
            }
        } else {
            multipliers.affinityBonus = 1.5f;
        }

        // Store multipliers in the map keyed by normalized house ID
        _houseMultipliers[houseID] = multipliers;
    }

    // Return true if at least one house was successfully parsed
    return !_houseMultipliers.empty();
}

/**
 * Retrieves house multipliers by house ID.
 * Performs case-insensitive lookup via house ID normalization.
 *
 * @param houseID  The house ID to look up
 * @return pointer to the HouseMultipliers struct, or nullptr if not found
 */
const ItemDatabase::HouseMultipliers* ItemDatabase::getHouseMultipliers(const std::string& houseID) const {
    const std::string normalizedHouseID = normalizeHouseID(houseID);
    auto multipliersIterator = _houseMultipliers.find(normalizedHouseID);
    if (multipliersIterator == _houseMultipliers.end()) {
        return nullptr;
    }
    return &multipliersIterator->second;
}

/**
 * Filters the divine bucket to items whose houseAffinity is in _activeHouses (or None).
 * Called automatically by setActiveHouses().
 */
void ItemDatabase::rebuildFilteredDivineBucket() {
    _filteredDivineBucket = Bucket();
    if (!_hasActiveHouseFilter) return;

    auto bucket = _bucketsByRarity.find(ItemDef::Rarity::Divine);
    if (bucket == _bucketsByRarity.end()) return;

    for (const auto& defId : bucket->second.defIds) {
        auto def = getDef(defId);
        if (!def) continue;
        ItemDef::House affinity = def->getHouseAffinity();
        if (affinity == ItemDef::House::None || _activeHouses.count(affinity) > 0) {
            addToBucket(_filteredDivineBucket, defId, (double)def->getWeight());
        }
    }
}

/**
 * Sets the active player houses used to filter divine item rolls.
 * Rebuilds the filtered divine bucket immediately.
 */
void ItemDatabase::setActiveHouses(const std::vector<std::string>& houseIds) {
    _activeHouses.clear();
    for (const auto& id : houseIds) {
        ItemDef::House house = ItemDef::houseFromString(id, ItemDef::House::None);
        if (house != ItemDef::House::None) {
            _activeHouses.insert(house);
        }
    }
    _hasActiveHouseFilter = !_activeHouses.empty();
    rebuildFilteredDivineBucket();
}

/**
 * Two-phase weighted roll:
 *   Phase 1 — pick a rarity tier using the normalized _rarityWeights.
 *   Phase 2 — pick an item from that tier's bucket using per-item weights.
 *
 * Falls back to any non-empty tier if the selected tier has no items.
 */
std::string ItemDatabase::rollRandomDefId() {
    if (!_rngReady) {
        const_cast<ItemDatabase*>(this)->setStartingPointWithTime();
    }

    static const ItemDef::Rarity rarityOrder[] = {
        ItemDef::Rarity::Common,
        ItemDef::Rarity::Rare,
        ItemDef::Rarity::Divine
    };

    // Phase 1: pick a tier
    double roll = _rng.getRightOpenDouble(0.0, 1.0);
    double cumulative = 0.0;
    ItemDef::Rarity selected = rarityOrder[0];
    for (auto rarity : rarityOrder) {
        auto rarityWeight = _rarityWeights.find(rarity);
        if (rarityWeight == _rarityWeights.end()) continue;
        cumulative += rarityWeight->second;
        selected = rarity;
        if (roll < cumulative) break;
    }

    // Phase 2: pick an item from the selected tier.
    // For divine, use the house-filtered bucket if a filter is active.
    if (selected == ItemDef::Rarity::Divine && _hasActiveHouseFilter) {
        if (!_filteredDivineBucket.defIds.empty()) {
            return rollFromBucket(_filteredDivineBucket);
        }
        // Filter is active but no divine items match the active houses — skip divine entirely
        // and fall through to the fallback below.
    } else {
        auto bucketIt = _bucketsByRarity.find(selected);
        if (bucketIt != _bucketsByRarity.end() && !bucketIt->second.defIds.empty()) {
            return rollFromBucket(bucketIt->second);
        }
    }

    // Fallback: selected tier is empty — try other tiers in order
    for (auto rarity : rarityOrder) {
        auto bucket = _bucketsByRarity.find(rarity);
        if (bucket != _bucketsByRarity.end() && !bucket->second.defIds.empty()) {
            return rollFromBucket(bucket->second);
        }
    }
    return "";
}

/** Weighted roll within a specific rarity bucket (probably not needed) */
std::string ItemDatabase::rollRandomDefId(ItemDef::Rarity rarity) {
    auto bucket = _bucketsByRarity.find(rarity);
    if (bucket == _bucketsByRarity.end()) return "";
    return rollFromBucket(bucket->second);
}

/** Just for potential usage */
std::vector<std::string> ItemDatabase::getAllDefIds() const {
    std::vector<std::string> out;
    out.reserve(_defs.size());
    for (auto& kv : _defs) out.push_back(kv.first);
    return out;
}

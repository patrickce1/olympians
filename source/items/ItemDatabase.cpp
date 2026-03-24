#include "ItemDatabase.h"
#include <cctype>

using namespace cugl;

static std::string normalizeHouseId(std::string s) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/** Clears items from buckets and reinitializes them; buckets contain items of the corresponding rarity */
void ItemDatabase::clearBuckets() {
    _allDefIds = Bucket();
    _bucketsByRarity.clear();
    _bucketsByRarity[ItemDef::Rarity::Common]    = Bucket();
    _bucketsByRarity[ItemDef::Rarity::Rare]      = Bucket();
    _bucketsByRarity[ItemDef::Rarity::Divine]    = Bucket();
}

/** Clears buckets and the item database collection */
void ItemDatabase::clear() {
    _defs.clear();
    _houseScaling.clear();
    clearBuckets();
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
}

/** Load rarity weights from a JSON */
void ItemDatabase::loadRarityWeights(const std::shared_ptr<JsonValue>& json) {
    resetRarityWeights();

    if (!json || !json->isObject()) return;
    if (!json->has("rarityWeights")) return;
    if (!json->get("rarityWeights")->isObject()) return;

    auto rw = json->get("rarityWeights");

    if (rw->has("Uncommon") || rw->has("Epic") || rw->has("Legendary")) {
        _rarityWeights.clear();
        return;
    }

    auto loadOne = [&](const char* key, ItemDef::Rarity rarity) {
        if (rw->has(key) && rw->get(key)->isNumber()) {
            double w = rw->get(key)->asDouble();
            if (w < 0.0) w = 0.0;
            _rarityWeights[rarity] = w;
        }
    };

    loadOne("Common",    ItemDef::Rarity::Common);
    loadOne("Rare",      ItemDef::Rarity::Rare);
    loadOne("Divine",    ItemDef::Rarity::Divine);
}

/** Returns the probability weight of the given rarity */
double ItemDatabase::rarityBaseWeight(ItemDef::Rarity r) const {
    auto rarity = _rarityWeights.find(r);
    if (rarity != _rarityWeights.end()) return rarity->second;
    return 1.0;
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

    auto arr = json->get("items");
    for (int i = 0; i < arr->size(); i++) {
        auto entry = arr->get(i);
        auto def = ItemDef::alloc(entry);
        if (!def) {
            CULog("ItemDatabase: failed to parse item at index %d", i);
            continue;
        }

        const std::string& id = def->getId();
        if (_defs.find(id) != _defs.end()) {
            CULog("ItemDatabase: duplicate item id '%s' (index %d). Overwriting.", id.c_str(), i);
        }
        _defs[id] = def;

        // Rarity-driven spawn weights
        double w = rarityBaseWeight(def->getRarity());

        // Add to spawn buckets if spawnable
        addToBucket(_allDefIds, id, w);
        addToBucket(_bucketsByRarity[def->getRarity()], id, w);
    }

    // Note: _defs may be non-empty even if _allDefs is empty (e.g. all weights 0)
    return !_defs.empty();
}

bool ItemDatabase::loadHouseScalingFromJson(const std::shared_ptr<JsonValue>& json) {
    _houseScaling.clear();

    if (!json || !json->isObject()) return false;
    auto houses = json->get("houses");
    if (!houses || !houses->isArray()) return false;

    for (int i = 0; i < houses->size(); ++i) {
        auto entry = houses->get(i);
        if (!entry || !entry->isObject()) {
            continue;
        }
        if (!entry->has("id") || !entry->get("id")->isString()) {
            continue;
        }

        const std::string id = normalizeHouseId(entry->getString("id", ""));
        if (id.empty()) {
            continue;
        }

        HouseScaling scaling;

        auto readSlider = [&](const char* key, float fallback) {
            if (!entry->has(key) || !entry->get(key)->isNumber()) {
                return fallback;
            }
            float value = entry->getFloat(key);
            return clamp01(value);
        };

        scaling.attack = readSlider("attack", 0.0f);
        scaling.support = readSlider("support", 0.0f);
        scaling.utility = readSlider("utility", 0.0f);

        if (entry->has("affinityBonus") && entry->get("affinityBonus")->isNumber()) {
            scaling.affinityBonus = entry->getFloat("affinityBonus");
            if (scaling.affinityBonus <= 0.0f) {
                scaling.affinityBonus = 1.5f;
            }
        } else {
            scaling.affinityBonus = 1.5f;
        }

        _houseScaling[id] = scaling;
    }

    return !_houseScaling.empty();
}

const ItemDatabase::HouseScaling* ItemDatabase::getHouseScaling(const std::string& houseId) const {
    auto it = _houseScaling.find(houseId);
    if (it == _houseScaling.end()) {
        return nullptr;
    }
    return &it->second;
}

/** Rarity-driven weighted roll across all spawnable items */
std::string ItemDatabase::rollRandomDefId() {
    return rollFromBucket(_allDefIds);
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

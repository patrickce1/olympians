#include "ItemDatabase.h"

using namespace cugl;

/** Clears items from buckets and reinitializes them; buckets contain items of the corresponding rarity */
void ItemDatabase::clearBuckets() {
    _all = Bucket();
    _byRarity.clear();
    _byRarity[ItemDef::Rarity::Common]    = Bucket();
    _byRarity[ItemDef::Rarity::Uncommon]  = Bucket();
    _byRarity[ItemDef::Rarity::Rare]      = Bucket();
    _byRarity[ItemDef::Rarity::Epic]      = Bucket();
    _byRarity[ItemDef::Rarity::Legendary] = Bucket();
    _byRarity[ItemDef::Rarity::Divine]    = Bucket();
}

void ItemDatabase::clear() {
    _defs.clear();
    clearBuckets();
}

void ItemDatabase::seed(std::uint64_t s) {
    _rng.initWithSeed((Uint64)s);
    _rngReady = true;
}

void ItemDatabase::seedWithTime() {
    _rng.init();
    _rngReady = true;
}

std::shared_ptr<ItemDef> ItemDatabase::getDef(const std::string& defId) const {
    auto it = _defs.find(defId);
    return (it == _defs.end() ? nullptr : it->second);
}

std::shared_ptr<ItemInstance> ItemDatabase::createInstance(const std::string& defId,
                                                           ItemInstance::ItemId id) const {
    if (!getDef(defId)) return nullptr;
    return ItemInstance::alloc(defId, id);
}


double ItemDatabase::rarityBaseWeight(ItemDef::Rarity r) {
    // Change below to alter rarity weights
    switch (r) {
        case ItemDef::Rarity::Common:    return 0.45;
        case ItemDef::Rarity::Uncommon:  return 0.40;
        case ItemDef::Rarity::Rare:      return 0.08;
        case ItemDef::Rarity::Epic:      return 0.04;
        case ItemDef::Rarity::Legendary: return 0.02;
        case ItemDef::Rarity::Divine:    return 0.01;
        default:                         return 1.00;
    }
}

void ItemDatabase::addToBucket(Bucket& bucket, const std::string& defId, double effWeight) {
    // Only include spawnable items (effWeight > 0)
    if (effWeight <= 0.0) return;

    bucket.ids.push_back(defId);
    bucket.total += effWeight;
    bucket.prefix.push_back(bucket.total);
}

std::string ItemDatabase::rollFromBucket(const Bucket& bucket) {
    if (bucket.ids.empty() || bucket.total <= 0.0) return "";

    if (!_rngReady) {
        // Lazy seed if user forgot to seed explicitly
        const_cast<ItemDatabase*>(this)->seedWithTime();
    }

    // r in [0, total)
    double r = _rng.getRightOpenDouble(0.0, bucket.total);

    // first prefix > r
    auto it = std::upper_bound(bucket.prefix.begin(), bucket.prefix.end(), r);
    std::size_t idx = (std::size_t)std::distance(bucket.prefix.begin(), it);
    if (idx >= bucket.ids.size()) idx = bucket.ids.size() - 1;

    return bucket.ids[idx];
}

bool ItemDatabase::loadFromJson(const std::shared_ptr<JsonValue>& json) {
    clear();

    if (!json || !json->isObject()) return false;
    if (!json->has("items") || !json->get("items")->isArray()) return false;

    clearBuckets();

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
        double w = effectiveWeight(*def);

        // Add to spawn buckets if spawnable
        addToBucket(_all, id, w);
        addToBucket(_byRarity[def->getRarity()], id, w);
    }

    // Note: _defs may be non-empty even if _all is empty (e.g. all weights 0)
    return !_defs.empty();
}

/** Roll from all items */
std::string ItemDatabase::rollRandomDefId() {
    return rollFromBucket(_all);
}

/** Roll within a  rarity */
std::string ItemDatabase::rollRandomDefId(ItemDef::Rarity rarity) {
    auto it = _byRarity.find(rarity);
    if (it == _byRarity.end()) return "";
    return rollFromBucket(it->second);
}

/** Just for potential usage */
std::vector<std::string> ItemDatabase::getAllDefIds() const {
    std::vector<std::string> out;
    out.reserve(_defs.size());
    for (auto& kv : _defs) out.push_back(kv.first);
    return out;
}

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


void ItemDatabase::resetRarityWeights() {
    _rarityWeights.clear();
    _rarityWeights[ItemDef::Rarity::Common]    = 1.00;
    _rarityWeights[ItemDef::Rarity::Uncommon]  = 0.60;
    _rarityWeights[ItemDef::Rarity::Rare]      = 0.30;
    _rarityWeights[ItemDef::Rarity::Epic]      = 0.12;
    _rarityWeights[ItemDef::Rarity::Legendary] = 0.05;
}

void ItemDatabase::loadRarityWeights(const std::shared_ptr<JsonValue>& json) {
    resetRarityWeights();

    if (!json || !json->isObject()) return;
    if (!json->has("rarityWeights")) return;
    if (!json->get("rarityWeights")->isObject()) return;

    auto rw = json->get("rarityWeights");

    auto loadOne = [&](const char* key, ItemDef::Rarity rarity) {
        if (rw->has(key) && rw->get(key)->isNumber()) {
            double w = rw->get(key)->asDouble();
            if (w < 0.0) w = 0.0;
            _rarityWeights[rarity] = w;
        }
    };

    loadOne("Common",    ItemDef::Rarity::Common);
    loadOne("Uncommon",  ItemDef::Rarity::Uncommon);
    loadOne("Rare",      ItemDef::Rarity::Rare);
    loadOne("Epic",      ItemDef::Rarity::Epic);
    loadOne("Legendary", ItemDef::Rarity::Legendary);
}


double ItemDatabase::rarityBaseWeight(ItemDef::Rarity r) const {
    auto it = _rarityWeights.find(r);
    if (it != _rarityWeights.end()) return it->second;
    return 1.0;
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
        double w = rarityBaseWeight(def->getRarity());

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

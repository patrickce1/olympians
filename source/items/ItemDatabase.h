#ifndef __ITEM_DATABASE_H__
#define __ITEM_DATABASE_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>

#include "ItemDef.h"
#include "ItemInstance.h"

/**
 * Stores all ItemDef definitions and provides fast lookup + random selection.
 *
 * ItemDatabase is pure "definitions + selection" (no ownership, no inventory).
 */
class ItemDatabase {
private:
    struct Bucket {
        std::vector<std::string> ids;
        std::vector<double> prefix;
        double total = 0.0;
    };
    
    struct RarityHash {
        std::size_t operator()(ItemDef::Rarity r) const noexcept {
            return static_cast<std::size_t>(r);
        }
    };
    
    std::unordered_map<std::string, std::shared_ptr<ItemDef>> _defs;
    
    Bucket _all;
    std::unordered_map<ItemDef::Rarity, Bucket, RarityHash> _byRarity;
    
    cugl::Random _rng;
    bool _rngReady = false;
    
private:
    void clearBuckets();

    static double rarityBaseWeight(ItemDef::Rarity r);
    static double effectiveWeight(const ItemDef& def) {
        return rarityBaseWeight(def.getRarity());
    }

    void addToBucket(Bucket& bucket, const std::string& defId, double effWeight);
    std::string rollFromBucket(const Bucket& bucket);
    
public:
    ItemDatabase() = default;
    ~ItemDatabase() = default;

    void clear();

    /** Seed options */
    /** Deterministic seed (recommended for host) */
    void seed(std::uint64_t s);
    /** Time-based seed (okay for local) */
    void seedWithTime();
    
    /**
    * Loads item defs from JSON.
    * Expected schema: { "items": [ { ...ItemDef... }, ... ] }
    */
    bool loadFromJson(const std::shared_ptr<cugl::JsonValue>& json);
    
    std::shared_ptr<ItemDef> getDef(const std::string& defId) const;
    
    std::shared_ptr<ItemInstance> createInstance(const std::string& defId,
                                                     ItemInstance::ItemId id) const;
    
    /** Rarity-driven weighted roll across all spawnable items */
    std::string rollRandomDefId();
    
    /** Weighted roll within a specific rarity bucket (probably not needed) */
    std::string rollRandomDefId(ItemDef::Rarity rarity);
    
    /** Serializable option */
    std::vector<std::string> getAllDefIds() const;
};

#endif // __ITEM_DATABASE_H__

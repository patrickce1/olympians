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
    /** Struct to contain defId objects and enable rolling a random defId from the bucket with weights */
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
    
    /** Collection of ItemDef defs based on their defIds */
    std::unordered_map<std::string, std::shared_ptr<ItemDef>> _defs;
    
    /** Bucket to contain all defIds so that they may be rolled */
    Bucket _all;
    std::unordered_map<ItemDef::Rarity, Bucket, RarityHash> _byRarity;
    
    /** Random value holder */
    cugl::Random _rng;
    /** Only needed if we forget to generate a random seed manually */
    bool _rngReady = false;
    
private:
    void clearBuckets();

    /** Returns the probability weight of the given rarity */
    static double rarityBaseWeight(ItemDef::Rarity r);
    
    /** Encapsulates weight system in the cpp file */
    static double effectiveWeight(const ItemDef& def) {
        return rarityBaseWeight(def.getRarity());
    }

    /** Add item with the given defId to the corresponding bucket with effWeight */
    void addToBucket(Bucket& bucket, const std::string& defId, double effWeight);
    
    /** Roll a random item from the given bucket */
    std::string rollFromBucket(const Bucket& bucket);
    
public:
    ItemDatabase() = default;
    ~ItemDatabase() = default;

    /** Clears buckets and the item database collection */
    void clear();

    /** Seed options; seed acts as the starting point for the RNG. The game's seed is generated at random, so it is unlikely
     that two parties in the same local network generate the same seed and therefore the same RNG pattern. */
    /** Deterministic seed (recommended for host) */
    void seed(std::uint64_t s);
    /** Time-based seed (okay for local) */
    void seedWithTime();
    
    /**
    * Loads item defs from JSON.
    * Expected schema: { "items": [ { ...ItemDef... }, ... ] }
    */
    bool loadFromJson(const std::shared_ptr<cugl::JsonValue>& json);
    
    /** Returns the definition of the given defId */
    std::shared_ptr<ItemDef> getDef(const std::string& defId) const;
    
    /** Creates an instance of the item with the given defId, and the instance is given id */
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

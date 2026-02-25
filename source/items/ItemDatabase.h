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
        std::vector<std::string> defIds;
        std::vector<double> prefix;
        double total = 0.0;
    };
    
    /** Struct to hash rarity probability weights stored in maps  */
    struct RarityHash {
        std::size_t operator()(ItemDef::Rarity r) const noexcept {
            return static_cast<std::size_t>(r);
        }
    };
    
    /** Collection of ItemDef defs based on their defIds */
    std::unordered_map<std::string, std::shared_ptr<ItemDef>> _defs;
    
    /** Bucket to contain all defIds so that they may be rolled */
    Bucket _allDefIds;
    
    /** The collection of buckets categorized by rarity */
    std::unordered_map<ItemDef::Rarity, Bucket, RarityHash> _byRarity;
    
    /** Data-driven rarity weights */
    std::unordered_map<ItemDef::Rarity, double, RarityHash> _rarityWeights;
    
    /** Random value holder */
    cugl::Random _rng;
    
    /** Only needed if we forget to generate a random seed manually */
    bool _rngReady = false;
    
private:
    /** Clears items from buckets and reinitializes them; buckets contain items of the corresponding rarity */
    void clearBuckets();

    /** In case of a reset, reset to fallback values */
    void resetRarityWeights();
    
    /** Load rarity weights from a JSON */
    void loadRarityWeights(const std::shared_ptr<cugl::JsonValue>& json);
    
    /** Returns the probability weight of the given rarity */
    double rarityBaseWeight(ItemDef::Rarity r) const;

    /** Add item with the given defId to the corresponding bucket with effectiveWeight
     *  Total = sum of effective weights of all the defIds addet to the bucket
     *  Prefix = cummulative sum array of effective weights of added defIds
     *  Each entry prefix[i] = sum of effectiveWeights[0..i]. Given i is the number of defIds entered.
     */
    void addToBucket(Bucket& bucket, const std::string& defId, double effectiveWeight);
    
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
    std::string rollFromBucket(const Bucket& bucket);
    
public:
    ItemDatabase() = default;
    ~ItemDatabase() = default;

    /** Clears buckets and the item database collection */
    void clear();

    /** Seed options; seed acts as the starting point for the RNG. The game's seed is generated at random, so it is unlikely
     that two parties in the same local network generate the same seed and therefore the same RNG pattern. */
    
    /** Deterministic seed (recommended for host) */
    void setStartingPoint(std::uint64_t seed);
    
    /** Time-based seed (okay for local) */
    void setStartingPointWithTime();
    
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

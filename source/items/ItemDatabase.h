#ifndef __ITEM_DATABASE_H__
#define __ITEM_DATABASE_H__
#include <cugl/cugl.h>
#include <unordered_map>
#include <unordered_set>
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
public:
    /**
     * Per-house multipliers applied at item use time.
     *
     * attack/support are additive sliders in [0,1] and are used as
     * baseValue * (1 + slider). affinityBonus is an extra multiplier that only
     * applies for rare/divine items when houseAffinity matches the player's house.
     */
    struct HouseMultipliers {
        float attack = 0.0f;
        float support = 0.0f;
        float affinityBonus = 1.5f;
    };

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

    /** Struct to hash house enums stored in sets/maps */
    struct HouseHash {
        std::size_t operator()(ItemDef::House h) const noexcept {
            return static_cast<std::size_t>(h);
        }
    };
    
    /** Collection of ItemDef defs based on their defIds */
    std::unordered_map<std::string, std::shared_ptr<ItemDef>> _defs;

    /** Runtime per-house multipliers keyed by normalized house ID */
    std::unordered_map<std::string, HouseMultipliers> _houseMultipliers;

    /** Per-rarity item buckets; each bucket holds items weighted by their individual weight field */
    std::unordered_map<ItemDef::Rarity, Bucket, RarityHash> _bucketsByRarity;

    /** Normalized rarity tier weights (always sum to 1.0 after loading) */
    std::unordered_map<ItemDef::Rarity, double, RarityHash> _rarityWeights;

    /** Set of player house enums currently in the game; empty means no filter is active */
    std::unordered_set<ItemDef::House, HouseHash> _activeHouses;

    /** Pre-filtered divine bucket containing only items whose houseAffinity is in _activeHouses
     *  (or None). Rebuilt whenever setActiveHouses() is called. */
    Bucket _filteredDivineBucket;

    /** True once setActiveHouses() has been called with at least one valid house */
    bool _hasActiveHouseFilter = false;

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
    
    /** Rebuilds _filteredDivineBucket from the divine rarity bucket, keeping only items
     *  whose houseAffinity is in _activeHouses (or House::None). No-ops if no filter is set. */
    void rebuildFilteredDivineBucket();

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

    /** Rolls from a bucket using the caller-provided RNG. */
    std::string rollFromBucket(const Bucket& bucket, cugl::Random& rng) const;
    
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
    
    /**
     * Restricts divine item rolls to items whose houseAffinity matches one of the given house IDs.
     * Items with House::None affinity are always included. Call once after the player roster is
     * known; the filter persists until clear() is called.
     *
     * @param houseIds  Player house ID strings (e.g. "zeus", "poseidon"). Unrecognized strings
     *                  are silently ignored. Pass an empty vector to clear the filter.
     */
    void setActiveHouses(const std::vector<std::string>& houseIds);

    /** Rarity-driven weighted roll across all spawnable items */
    std::string rollRandomDefId();
    
    /** Weighted roll within a specific rarity bucket (probably not needed) */
    std::string rollRandomDefId(ItemDef::Rarity rarity);

    /** Weighted roll within a specific rarity bucket and a defined rng seed.
     *
     * @param rarity  The rarity bucket to roll from.
     * @param rng  The predefined rng seed to roll with.
     * @return The string of the rolled item defId.
     */
    std::string rollRandomDefId(ItemDef::Rarity rarity, cugl::Random& rng) const;
    
    /** Serializable option */
    std::vector<std::string> getAllDefIds() const;

    /** Load house multipliers from parsed houses JSON root. */
    bool loadHouseMultipliersFromJson(const std::shared_ptr<cugl::JsonValue>& json);

    /** Returns house multipliers for a house ID if present; nullptr otherwise. */
    const HouseMultipliers* getHouseMultipliers(const std::string& houseID) const;
};

#endif // __ITEM_DATABASE_H__

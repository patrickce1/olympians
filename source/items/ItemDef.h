#ifndef __ITEM_DEF_H__
#define __ITEM_DEF_H__
#include <cugl/cugl.h>
#include <string>
#include <vector>

/**
 * Immutable, data-driven definition of an item type.
 *
 * Think: "Lightning Bolt" as a template (name, icon key, description, tuning params).
 * Does NOT represent a specific spawned item in a player's inventory.
 */

class ItemDef {
public:
    enum class Type : uint8_t {
        Attack,
        Support
    };
    enum class Rarity : uint8_t {
        Common,
        Rare,
        Divine
    };
    enum class House : uint8_t {
        Zeus,
        Poseidon,
        Hades,
        Demeter,
        Ares,
        Athena,
        Aphrodite,
        Hephaestus,
        Hermes,
        None
    };
    /** Data-driven utility effect categories that items may apply. */
    enum class EffectType : uint8_t {
        Shield,
        Barrier,
        Stun,
        Vulnerable
    };

    /**
     * Serialized tuning values for one item effect.
     *
     * `multiplier` is used by effects such as barrier and vulnerable, while
     * `mitigation` is used by shield. `duration` is the lifetime in seconds for
     * timed effects.
     */
    struct Effect {
        /** The effect category to apply. */
        EffectType type = EffectType::Shield;
        /** Scalar tuning value used by barrier and vulnerable effects. */
        float multiplier = 1.0f;
        /** Flat damage reduction used by shield effects. */
        float mitigation = 0.0f;
        /** Duration in seconds for timed effects. */
        float duration = 0.0f;
    };

private:
    /* Unique key */
    std::string _id;
    
    /* Name to be displayed */
    std::string _name;
    
    /* Description of the item */
    std::string _description;
    
    /* PLACEHOLDER FOR REPRESENTING ITEM TEXTURE/ICON */
    std::string _iconKey;
    
    /* Type of item (e.g. Attack, Support) */
    Type _type;
    
    /* Rarity of item */
    Rarity _rarity;
    
    /* Base value of item before house multipliers are applied */
    float _baseValue = 1.0f;

    /* House affinity tag used for rare/divine affinity bonus matching */
    House _houseAffinity = House::None;

    /* Collection of utility effects for this item */
    std::vector<Effect> _effects;

public:
    ItemDef() = default;
    ~ItemDef() = default;

    /**
     * Initializes a definition from JSON.
     *
     * Required keys: id, type, rarity.
     * Optional keys: name, description, icon/iconKey, houseAffinity, baseValue, effects.
     */
    bool init(const std::shared_ptr<cugl::JsonValue>& json);
    
    /** Allocates and initializes an ItemDef from JSON, returning nullptr on failure */
    static std::shared_ptr<ItemDef> alloc(const std::shared_ptr<cugl::JsonValue>& json) {
        auto result = std::make_shared<ItemDef>();
        return (result->init(json) ? result : nullptr);
    }
    
    /** Gets item ID */
    const std::string& getId() const { return _id; }
    /** Gets item name */
    const std::string& getName() const { return _name; }
    /** Gets item description */
    const std::string& getDescription() const { return _description; }
    /** Gets item icon key (used to look up texture in asset manager) */
    const std::string& getIconKey() const { return _iconKey; }
    /** Gets the base value of the item before multipliers are applied */
    const float getBaseValue() const { return _baseValue; }

    /**
     * Returns the intended house affinity for this item.
     * Rare/divine items can receive affinityBonus when this matches player house.
     */
    House getHouseAffinity() const { return _houseAffinity; }
    /** Gets item type */
    Type getType() const { return _type; }
    /** Gets item rarity */
    Rarity getRarity() const { return _rarity; }
    /** Gets utility item effects */
    const std::vector<Effect>& getEffects() const { return _effects; }
    /**
     * Returns true if this item contains at least one effect of the given type.
     *
     * @param type  The effect category to search for.
     */
    bool hasEffectType(EffectType type) const;

    /**
     * Extract Type enum from a string.
     *
     * @param value     The string token to parse.
     * @param fallback  The type to return if parsing fails
     * @return the parsed type, or fallback if unrecognized
     */
    static Type typeFromString(std::string value, Type fallback = Type::Attack);
    /**
     * Extract Rarity enum from a string.
     *
     * @param value     The string token to parse
     * @param fallback  The rarity to return if parsing fails
     * @return the parsed rarity, or fallback if unrecognized
     */
    static Rarity rarityFromString(std::string value, Rarity fallback = Rarity::Common);
    /**
     * Extract House enum from a string.
     * Accepts "zeus", "poseidon", "hades", "demeter", "ares", "athena", or "none" (case-insensitive, trimmed).
     *
     * @param value     The string token to parse
     * @param fallback  The house to return if parsing fails
     * @return the parsed house, or fallback if unrecognized
     */
    static House houseFromString(std::string value, House fallback = House::None);
    /**
     * Extract EffectType enum from a string.
     *
     * @param value  The serialized effect type name
     * @return the parsed effect, or fallback if unrecognized
     */
    static EffectType effectTypeFromString(std::string value);
};

#endif // __ITEM_DEF_H__

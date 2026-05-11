#ifndef __ITEM_DEF_H__
#define __ITEM_DEF_H__
#include <cugl/cugl.h>
#include <string>
#include <vector>

/**
 * Configuration for item use animations.
 * 
 * When an item is used, an overlay animation can play on the screen.
 * This config specifies the sprite sheet texture, layout (rows/cols), and
 * at which frame the damage should be resolved (broadcasted to network/audio).
 */
struct ItemUseAnimationConfig {
    /** Asset key for the sprite sheet texture (e.g., "mallet_animation"). */
    std::string spriteSheetId;
    
    /** Number of rows in the sprite sheet. */
    int rows = 0;
    
    /** Number of columns in the sprite sheet. */
    int cols = 0;
    
    /** Total number of frames in the sprite sheet animation. */
    int frameCount = 0;
    
    /** Duration (in seconds) for the entire animation. */
    float animationDuration = 0.0f;
    
    /** Frame index at which to trigger damage resolution and network broadcast. */
    int damageResolutionFrame = 0;

    /** If true, animation plays at the item drop position. If false (default), plays at the viewport center. */
    bool centerOnDropLocation = false;
};

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
        Divine,
        Special
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
    enum class AttackTarget : uint8_t {
        Enemy,
        AllAllies
    };
    /** Data-driven utility effect categories that items may apply. */
    enum class EffectType : uint8_t {
        Shield,
        Barrier,
        Regen,
        Resurrect,
        Educate,
        Stun,
        Love,
        Slow,
        Vulnerable,
        Upgrade,
        Forge,
        Charm
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
        EffectType type;
        /** Scalar tuning value used by barrier and vulnerable effects. */
        float multiplier = 1.0f;
        /** Flat damage reduction used by shield effects. */
        float mitigation = 0.0f;
        /** Flat healing amount used by regen effects. */
        float regenAmount = 0.0f;
        /** Flat health restored immediately when a resurrection revives a dead ally. */
        float reviveHealth = 0.0f;
        /** Duration in seconds for timed effects. */
        float duration = 0.0f;
        /** Whether an item effect should apply to all four boss sides. */
        bool applyToAllSides = false;
        /** Whether an item effect should target every allied player slot. */
        bool targetAllAllies = false;
        /** Chance for probabilistic effects, expressed as a value in [0, 1]. */
        float chance = 0.0f;
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
    
    /* Texture for representing the tooltip */
    std::string _tooltipKey;
    
    /* Type of item (e.g. Attack, Support) */
    Type _type;
    
    /* Rarity of item */
    Rarity _rarity;
    
    /* Base value of item before house multipliers are applied */
    float _baseValue = 1.0f;

    /* Explicit target routing for attack items */
    AttackTarget _attackTarget = AttackTarget::Enemy;

    /* House affinity tag used for rare/divine affinity bonus matching */
    House _houseAffinity = House::None;

    /* Collection of utility effects for this item */
    std::vector<Effect> _effects;
    
    /* Optional animation configuration for when item is used */
    ItemUseAnimationConfig _itemUseAnimationConfig;
    
    /* Flag indicating whether _itemUseAnimationConfig is valid/present */
    bool _hasItemUseAnimation = false;
    
    /* Optional sound to play when item is used (empty string if not defined) */
    std::string _itemUseSound;

    /* Relative spawn weight within this item's rarity tier (default 10). Higher = more common. */
    float _weight = 10.0f;
    
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
    /** Gets item tooltip key (used to look up texture in asset manager) */
    const std::string& getTooltipKey() const { return _tooltipKey; }
    /** Gets the base value of the item before multipliers are applied */
    const float getBaseValue() const { return _baseValue; }

    /**
     * Returns the intended house affinity for this item.
     * Rare/divine items can receive affinityBonus when this matches player house.
     */
    House getHouseAffinity() const { return _houseAffinity; }
    
    /** Gets item type */
    Type getType() const { return _type; }
    
    /**
     * Gets the target routing mode for attack items.
     *
     * Support items always return `AttackTarget::Enemy`, but the value is only
     * meaningful when `getType() == Type::Attack`.
     *
     * @return The configured attack target routing mode.
     */
    AttackTarget getAttackTarget() const { return _attackTarget; }
    
    /** Gets item rarity */
    Rarity getRarity() const { return _rarity; }

    /** Gets utility item effects */
    const std::vector<Effect>& getEffects() const { return _effects; }
    
    /**
     * Returns the first effect of the requested type, if present on this item.
     *
     * @param type The effect category to search for.
     * @return A pointer to the first matching effect, or `nullptr` if none exists.
     */
    const Effect* getEffect(EffectType type) const;

    /**
     * Returns true if this item contains at least one effect of the given type.
     *
     * @param type  The effect category to search for.
     * @return true if the item contains at least one matching effect.
     */
    bool hasEffectType(EffectType type) const;

    /** Returns true if this item has an associated use animation */
    bool hasItemUseAnimation() const { return _hasItemUseAnimation; }
    
    /** Gets the animation configuration for this item (valid only if hasItemUseAnimation() is true) */
    const ItemUseAnimationConfig& getItemUseAnimation() const { return _itemUseAnimationConfig; }
    
    /**
     * Returns the spawn weight of this item relative to other items in the same rarity tier.
     * Used for within-tier weighted random selection. Default is 10 if not specified in JSON.
     * @return the spawn weight of this item, where higher means more common within its rarity tier.
     */
    float getWeight() const { return _weight; }

    /**
     * Gets the sound asset key to play when this item is used.
     * Returns an empty string if no itemUseSound is defined in the item JSON.
     * When empty, a default sound ("attack" or "support") is played instead.
     */
    const std::string& getItemUseSound() const { return _itemUseSound; }
    
    /**
     * Parses optional itemUseAnimation configuration from JSON if present.
     * Sets _itemUseAnimationConfig and _hasItemUseAnimation fields.
     * 
     * @param json The item definition JSON object
     */
    void parseItemUseAnimation(const std::shared_ptr<cugl::JsonValue>& json);

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
     * Extract AttackTarget enum from a string.
     *
     * @param value The string token to parse.
     * @param fallback The attack target to return if parsing fails.
     * @return The parsed attack target, or fallback if unrecognized.
     */
    static AttackTarget attackTargetFromString(std::string value, AttackTarget fallback = AttackTarget::Enemy);

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

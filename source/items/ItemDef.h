#ifndef __ITEM_DEF_H__
#define __ITEM_DEF_H__
#include <cugl/cugl.h>
#include <string>

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
        Hephestus,
        Hermes,
        None
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
    
    /* Optional animation configuration for when item is used */
    ItemUseAnimationConfig _itemUseAnimationConfig;
    
    /* Flag indicating whether _itemUseAnimationConfig is valid/present */
    bool _hasItemUseAnimation = false;
    
    /* Optional sound to play when item is used (empty string if not defined) */
    std::string _itemUseSound;
    
public:
    ItemDef() = default;
    ~ItemDef() = default;

    /**
     * Initializes a definition from JSON.
     *
     * Required keys: id, type, rarity.
     * Optional keys: name, description, icon/iconKey, houseAffinity, baseValue.
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
    
    /** Returns true if this item has an associated use animation */
    bool hasItemUseAnimation() const { return _hasItemUseAnimation; }
    
    /** Gets the animation configuration for this item (valid only if hasItemUseAnimation() is true) */
    const ItemUseAnimationConfig& getItemUseAnimation() const { return _itemUseAnimationConfig; }
    
    /** Gets the sound to play when item is used (empty string if not defined) */
    const std::string& getItemUseSound() const { return _itemUseSound; }
    
    /** Extract Type enum from a string */
    static Type typeFromString(std::string value, Type fallback = Type::Attack);
    /** Extract Rarity enum from a string */
    static Rarity rarityFromString(std::string value, Rarity fallback = Rarity::Common);
    /** Extract House enum from a string */
    static House houseFromString(std::string value, House fallback = House::None);

};

#endif // __ITEM_DEF_H__

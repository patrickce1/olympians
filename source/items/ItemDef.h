#ifndef __ITEM_DEF_H__
#define __ITEM_DEF_H__
#include <cugl/cugl.h>
#include <string>

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
        Uncommon,
        Rare,
        Epic,
        Legendary,
        Divine
    };
    enum class House : uint8_t {
        Zeus,
        Poseidon,
        Hades,
        Ares,
        Aphrodite,
        Demeter,
        Dionysus,
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
    
    /* Primary House the item belongs to */
    House _primary;
    
    /* Secondary house the item belongs to (optional) */
    House _secondary;
    
public:
    ItemDef() = default;
    ~ItemDef() = default;
    
    bool init(const std::shared_ptr<cugl::JsonValue>& json);
    
    static std::shared_ptr<ItemDef> alloc(const std::shared_ptr<cugl::JsonValue>& json) {
        auto result = std::make_shared<ItemDef>();
        return (result->init(json) ? result : nullptr);
    }
    
    // Getters
    const std::string& getId() const { return _id; }
    const std::string& getName() const { return _name; }
    const std::string& getDescription() const { return _description; }
    const std::string& getIconKey() const { return _iconKey; }
    
    Type getType() const { return _type; }
    Rarity getRarity() const { return _rarity; }
    House getPrimary() const { return _primary; }
    House getSecondary() const { return _secondary; }
    
    static Type typeFromString(std::string s, Type fallback = Type::Attack);
    static Rarity rarityFromString(std::string s, Rarity fallback = Rarity::Common);
    static House houseFromString(std::string s, House fallback = House::None);
};

#endif // __ITEM_DEF_H__

#include "ItemDef.h"

using namespace cugl;

ItemDef::Type ItemDef::typeFromString(std::string s, Type fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (s == "attack")    return Type::Attack;
    if (s == "support")   return Type::Support;
    return fallback;
}

ItemDef::Rarity ItemDef::rarityFromString(std::string s, Rarity fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (s == "common")    return Rarity::Common;
    if (s == "uncommon")  return Rarity::Uncommon;
    if (s == "rare")      return Rarity::Rare;
    if (s == "epic")      return Rarity::Epic;
    if (s == "legendary") return Rarity::Legendary;
    if (s == "divine")    return Rarity::Divine;
    return fallback;
}

ItemDef::House ItemDef::houseFromString(std::string s, House fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (s == "zeus")      return House::Zeus;
    if (s == "poseidon")  return House::Poseidon;
    if (s == "hades")     return House::Hades;
    if (s == "ares")      return House::Ares;
    if (s == "aphrodite") return House::Aphrodite;
    if (s == "demeter")   return House::Demeter;
    if (s == "dionysus")  return House::Dionysus;
    if (s == "none")      return House::None;
    return fallback;
}

bool ItemDef::init(const std::shared_ptr<JsonValue>& json) {
    if (!json || !json->isObject()) return false;
    if (!json->has("id") || !json->get("id")->isString()) return false;
    _id = json->get("id")->asString();
    if (_id.empty()) return false;
    
    // Find field from JSON, otherwise use fallback
    _name = (json->has("name") && json->get("name")->isString()) ? json->get("name")->asString() : "";
    _description = (json->has("description") && json->get("description")->isString()) ? json->get("description")->asString() : "";
    _iconKey = (json->has("iconKey") && json->get("iconKey")->isString()) ? json->get("iconKey")->asString() : "";
    
    if (json->has("type") && json->get("type")->isString()) {
        _type = typeFromString(json->get("type")->asString(), Type::Attack);
    } else {
        CULogError("ItemDef '%s': missing or invalid 'type', defaulting to Attack", _id.c_str());
    }
    
    if (json->has("rarity") && json->get("rarity")->isString()) {
        _rarity = rarityFromString(json->get("rarity")->asString(), Rarity::Common);
    } else {
        CULogError("ItemDef '%s': missing or invalid 'rarity', defaulting to Common", _id.c_str());
    }
    
    if (json->has("primary") && json->get("primary")->isString()) {
        _primaryHouse = houseFromString(json->get("primary")->asString(), House::None);
    } else {
        CULogError("ItemDef '%s': missing or invalid 'primary', defaulting to None", _id.c_str());
    }
    
    if (json->has("secondary") && json->get("secondary")->isString()) {
        _secondaryHouse = houseFromString(json->get("secondary")->asString(), House::None);
    } else {
        CULogError("ItemDef '%s': missing or invalid 'secondary', defaulting to None", _id.c_str());
    }
    
    if (json->has("effectiveValue")) {
        _effectiveValue = json->getFloat("effectiveValue");
    } else {
        CULogError("ItemDef '%s': missing 'effectiveValue', defaulting to 0.0", _id.c_str());
    }
    
    return true;
}

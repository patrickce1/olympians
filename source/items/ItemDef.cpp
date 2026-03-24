#include "ItemDef.h"
#include <algorithm>
#include <cctype>

using namespace cugl;

ItemDef::Type ItemDef::typeFromString(std::string s, Type fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (s == "attack")    return Type::Attack;
    if (s == "support")   return Type::Support;
    if (s == "utility")   return Type::Utility;
    return fallback;
}

ItemDef::Rarity ItemDef::rarityFromString(std::string s, Rarity fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (s == "common")    return Rarity::Common;
    if (s == "rare")      return Rarity::Rare;
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
    if (s == "demeter")   return House::Demeter;
    if (s == "ares")      return House::Ares;
    if (s == "athena")    return House::Athena;
    if (s == "none")      return House::None;
    return fallback;
}

static std::string normalizeToken(std::string s) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

bool ItemDef::init(const std::shared_ptr<JsonValue>& json) {
    if (!json || !json->isObject()) return false;
    if (!json->has("id") || !json->get("id")->isString()) return false;

    // Hard-cut schema migration: reject legacy keys outright.
    if (json->has("effectiveValue") || json->has("primaryHouse") || json->has("secondaryHouse")) {
        CULogError("ItemDef: deprecated keys detected (effectiveValue/primaryHouse/secondaryHouse). Failing parse.");
        return false;
    }

    _id = json->get("id")->asString();
    if (_id.empty()) return false;
    
    // Find field from JSON, otherwise use fallback
    _name = (json->has("name") && json->get("name")->isString()) ? json->get("name")->asString() : "";
    _description = (json->has("description") && json->get("description")->isString()) ? json->get("description")->asString() : "";
    _iconKey = (json->has("icon") && json->get("icon")->isString())
        ? json->get("icon")->asString()
        : ((json->has("iconKey") && json->get("iconKey")->isString()) ? json->get("iconKey")->asString() : "");
    
    if (json->has("type") && json->get("type")->isString()) {
        const std::string typeText = normalizeToken(json->get("type")->asString());
        if (typeText != "attack" && typeText != "support" && typeText != "utility") {
            return false;
        }
        _type = typeFromString(typeText, Type::Attack);
    } else {
        return false;
    }
    
    if (json->has("rarity") && json->get("rarity")->isString()) {
        const std::string rarityText = normalizeToken(json->get("rarity")->asString());
        if (rarityText != "common" && rarityText != "rare" && rarityText != "divine") {
            return false;
        }
        _rarity = rarityFromString(rarityText, Rarity::Common);
    } else {
        return false;
    }

    if (json->has("houseAffinity") && json->get("houseAffinity")->isString()) {
        _houseAffinity = houseFromString(json->get("houseAffinity")->asString(), House::None);
    } else {
        _houseAffinity = House::None;
    }

    if (json->has("effect") && json->get("effect")->isString()) {
        _effect = json->get("effect")->asString();
        if (_effect.empty()) {
            _effect = "none";
        }
    } else {
        _effect = "none";
    }

    if (json->has("baseValue") && json->get("baseValue")->isNumber()) {
        _baseValue = json->getFloat("baseValue");
        if (_baseValue <= 0.0f) {
            _baseValue = 1.0f;
        }
    } else {
        _baseValue = 1.0f;
    }

    return true;
}

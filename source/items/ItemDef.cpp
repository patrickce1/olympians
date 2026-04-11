#include "ItemDef.h"
#include <algorithm>
#include <cctype>

using namespace cugl;

/**
 * Parses an item type from a string.
 * Accepts "attack", "support", or "utility" (case-insensitive, trimmed).
 *
 * @param value     The string to parse
 * @param fallback  The type to return if parsing fails
 * @return the parsed type, or fallback if unrecognized
 */
ItemDef::Type ItemDef::typeFromString(std::string value, Type fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notspace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notspace).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (value == "attack")    return Type::Attack;
    if (value == "support")   return Type::Support;
    return fallback;
}

/**
 * Parses an item rarity from a string.
 * Accepts "common", "rare", or "divine" (case-insensitive, trimmed).
 *
 * @param value     The string to parse
 * @param fallback  The rarity to return if parsing fails
 * @return the parsed rarity, or fallback if unrecognized
 */
ItemDef::Rarity ItemDef::rarityFromString(std::string value, Rarity fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notspace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notspace).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (value == "common")    return Rarity::Common;
    if (value == "rare")      return Rarity::Rare;
    if (value == "divine")    return Rarity::Divine;
    return fallback;
}

/**
 * Parses a house identifier from a string.
 * Accepts "zeus", "poseidon", "hades", "demeter", "ares", "athena", or "none" (case-insensitive, trimmed).
 *
 * @param value     The string to parse
 * @param fallback  The house to return if parsing fails
 * @return the parsed house, or fallback if unrecognized
 */
ItemDef::House ItemDef::houseFromString(std::string value, House fallback) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notspace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notspace).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    if (value == "zeus")      return House::Zeus;
    if (value == "poseidon")  return House::Poseidon;
    if (value == "hades")     return House::Hades;
    if (value == "demeter")   return House::Demeter;
    if (value == "ares")      return House::Ares;
    if (value == "athena")    return House::Athena;
    if (value == "none")      return House::None;
    return fallback;
}

/**
 * Normalizes a token by trimming whitespace and converting to lowercase.
 * Used internally for case-insensitive enum parsing from JSON strings.
 *
 * @param token  The string to normalize
 * @return the normalized token (trimmed and lowercased)
 */
static std::string normalizeToken(std::string token) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    token.erase(token.begin(), std::find_if(token.begin(), token.end(), notspace));
    token.erase(std::find_if(token.rbegin(), token.rend(), notspace).base(), token.end());
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return token;
}

/**
 * Initializes an ItemDef from a JSON object.
 * Parses item fields: id (required), name, description, icon/iconKey, type (required),
 * rarity (required), houseAffinity, and baseValue (with fallbacks for invalid values).
 *
 * @param json  The JSON object to parse
 * @return true if initialization succeeded (id and required enums were valid), false otherwise
 */
bool ItemDef::init(const std::shared_ptr<JsonValue>& json) {
    if (!json || !json->isObject()) return false;
    if (!json->has("id") || !json->get("id")->isString()) return false;

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

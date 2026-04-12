#include "ItemDef.h"
#include <algorithm>
#include <cctype>

using namespace cugl;

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
 * Parses a normalized JSON effect token into an ItemDef::EffectType.
 *
 * Supports shield, barrier, stun, and vulnerable effect strings.
 *
 * @param value  The normalized effect token from JSON.
 * @param out    Receives the parsed enum value on success.
 * @return       true if the token matched a known effect type.
 */
static bool tryParseEffectType(const std::string& value, ItemDef::EffectType& out) {
    if (value == "shield") {
        out = ItemDef::EffectType::Shield;
        return true;
    }
    if (value == "barrier") {
        out = ItemDef::EffectType::Barrier;
        return true;
    }
    if (value == "stun") {
        out = ItemDef::EffectType::Stun;
        return true;
    }
    if (value == "vulnerable") {
        out = ItemDef::EffectType::Vulnerable;
        return true;
    }
    return false;
}

/**
 * Parses an item type from a string.
 * Accepts "attack" or "support" (case-insensitive, trimmed).
 */
ItemDef::Type ItemDef::typeFromString(std::string value, Type fallback) {
    value = normalizeToken(value);

    if (value == "attack")  return Type::Attack;
    if (value == "support") return Type::Support;
    return fallback;
}

/**
 * Parses an item rarity from a string.
 * Accepts "common", "rare", or "divine" (case-insensitive, trimmed).
 */
ItemDef::Rarity ItemDef::rarityFromString(std::string value, Rarity fallback) {
    value = normalizeToken(value);

    if (value == "common") return Rarity::Common;
    if (value == "rare")   return Rarity::Rare;
    if (value == "divine") return Rarity::Divine;
    return fallback;
}

/**
 * Parses a house identifier from a string.
 */
ItemDef::House ItemDef::houseFromString(std::string value, House fallback) {
    value = normalizeToken(value);

    if (value == "zeus")        return House::Zeus;
    if (value == "poseidon")    return House::Poseidon;
    if (value == "hades")       return House::Hades;
    if (value == "demeter")     return House::Demeter;
    if (value == "ares")        return House::Ares;
    if (value == "athena")      return House::Athena;
    if (value == "aphrodite")   return House::Aphrodite;
    if (value == "hephaestus")  return House::Hephaestus;
    if (value == "hermes")      return House::Hermes;
    if (value == "none")        return House::None;
    return fallback;
}

/**
 * Parses a data-driven effect type from JSON.
 */
ItemDef::EffectType ItemDef::effectTypeFromString(std::string value) {
    value = normalizeToken(value);

    EffectType parsedType = EffectType::Shield;
    const bool parsedSuccessfully = tryParseEffectType(value, parsedType);
    CUAssertLog(parsedSuccessfully, "Unsupported effect type '%s'", value.c_str());
    return parsedType;
}

/**
 * Parses one effect object from the JSON effects array.
 */
static bool parseEffect(const std::shared_ptr<JsonValue>& json, ItemDef::Effect& out) {
    if (!json || !json->isObject()) return false;

    std::shared_ptr<JsonValue> typeNode = nullptr;
    if (json->has("kind")) {
        typeNode = json->get("kind");
    } else if (json->has("type")) {
        typeNode = json->get("type");
    }
    if (!typeNode || !typeNode->isString()) return false;

    const std::string effectType = normalizeToken(typeNode->asString());
    if (!tryParseEffectType(effectType, out.type)) return false;

    out.multiplier = 1.0f;
    if (json->has("multiplier") && json->get("multiplier")->isNumber()) {
        out.multiplier = std::max(0.0f, json->getFloat("multiplier"));
    } else if (json->has("scale") && json->get("scale")->isNumber()) {
        out.multiplier = std::max(0.0f, json->getFloat("scale"));
    }

    out.mitigation = 0.0f;
    if (json->has("mitigation") && json->get("mitigation")->isNumber()) {
        out.mitigation = std::max(0.0f, json->getFloat("mitigation"));
    } else if (json->has("amount") && json->get("amount")->isNumber()) {
        out.mitigation = std::max(0.0f, json->getFloat("amount"));
    }

    out.duration = 0.0f;
    if (json->has("duration") && json->get("duration")->isNumber()) {
        out.duration = std::max(0.0f, json->getFloat("duration"));
    }

    return true;
}

/**
 * Returns true if this item contains at least one effect of the given type.
 */
bool ItemDef::hasEffectType(EffectType type) const {
    for (const Effect& effect : _effects) {
        if (effect.type == type) {
            return true;
        }
    }
    return false;
}

/**
 * Initializes an ItemDef from a JSON object.
 */
bool ItemDef::init(const std::shared_ptr<JsonValue>& json) {
    if (!json || !json->isObject()) return false;
    if (!json->has("id") || !json->get("id")->isString()) return false;

    _id = json->get("id")->asString();
    if (_id.empty()) return false;

    _name = (json->has("name") && json->get("name")->isString()) ? json->get("name")->asString() : "";
    _description = (json->has("description") && json->get("description")->isString()) ? json->get("description")->asString() : "";
    _iconKey = (json->has("icon") && json->get("icon")->isString())
        ? json->get("icon")->asString()
        : ((json->has("iconKey") && json->get("iconKey")->isString()) ? json->get("iconKey")->asString() : "");
    
    if (json->has("type") && json->get("type")->isString()) {
        const std::string typeText = normalizeToken(json->get("type")->asString());
        if (typeText != "attack" && typeText != "support") {
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

    _effects.clear();
    if (json->has("effects")) {
        auto effects = json->get("effects");
        if (!effects->isArray()) {
            return false;
        }
        for (int effectsIndex = 0; effectsIndex < effects->size(); effectsIndex++) {
            Effect effect;
            if (!parseEffect(effects->get(effectsIndex), effect)) {
                CULog("ItemDef: skipping invalid effect at index %d for item '%s'", effectsIndex, _id.c_str());
                continue;
            }
            _effects.push_back(effect);
        }
    }

    return true;
}

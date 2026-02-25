#include "ItemInstance.h"

using namespace cugl;

/**
 * Initializes an ItemInstance with a definition ID and a host-assigned unique ID.
 * Returns false if defId is empty or id is 0 (reserved as invalid).
 *
 * @param defId     The definition ID linking this instance to its ItemDef
 * @param id        The unique instance ID assigned by the host's IdGenerator
 * @return true if initialization succeeded, false otherwise
 */
bool ItemInstance::init(const std::string& defId, ItemId id) {
    if (defId.empty()) return false;
    if (id == 0) return false;           // reserve 0 as "invalid"

    _defId = defId;
    _id = id;

    return true;
}

/**
 * Serializes this ItemInstance to a JSON object.
 * Stores the instance ID as a double (for JSON compatibility) and the defId as a string.
 *
 * @return a JsonValue object containing the id and defId fields
 */
std::shared_ptr<JsonValue> ItemInstance::toJson() const {
    auto obj = JsonValue::allocObject();
    obj->appendChild("id", JsonValue::alloc(static_cast<double>(_id)));
    obj->appendChild("defId", JsonValue::alloc(_defId));
    return obj;
}

/**
 * Deserializes this ItemInstance from a JSON object.
 * Expects fields "id" (number) and "defId" (string).
 * Returns false if either field is missing, malformed, or invalid (id == 0 or defId empty).
 *
 * @param json      The JSON object to read from
 * @return true if deserialization succeeded, false otherwise
 */
bool ItemInstance::fromJson(const std::shared_ptr<JsonValue>& json) {
    if (!json || !json->isObject()) return false;
    if (!json->has("id") || !json->get("id")->isNumber()) return false;
    if (!json->has("defId") || !json->get("defId")->isString()) return false;

    ItemId id = static_cast<ItemId>(json->get("id")->asDouble());
    std::string defId = json->get("defId")->asString();
    if (id == 0 || defId.empty()) return false;

    _id = id;
    _defId = defId;

    return true;
}

//
//  ItemInstance.cpp
//  olympians
//
//  Created by Aiden Joo on 2/23/26.
//

#include "ItemInstance.h"

using namespace cugl;

bool ItemInstance::init(const std::string& defId, ItemId id) {
    if (defId.empty()) return false;
    if (id == 0) return false;           // reserve 0 as "invalid"

    _defId = defId;
    _id = id;

    return true;
}

std::shared_ptr<JsonValue> ItemInstance::toJson() const {
    auto obj = JsonValue::allocObject();
    obj->appendChild("id", JsonValue::alloc(static_cast<double>(_id)));
    obj->appendChild("defId", JsonValue::alloc(_defId));
    return obj;
}

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

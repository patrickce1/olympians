#include "EasyPlayerAI.h"

/**
 * Reads the "easyPlayerAI" sub-object from playerAI.json and loads shared fields.
 */
bool EasyPlayerAI::init(const ItemDatabase& db, const std::string& path) {
    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        CULogError("EasyPlayerAI::init — failed to open %s", path.c_str());
        return false;
    }

    auto json = reader->readJson();
    if (!json) {
        CULogError("EasyPlayerAI::init — failed to parse %s", path.c_str());
        return false;
    }

    auto config = json->get("easyPlayerAI");
    if (!config) {
        CULogError("EasyPlayerAI::init — missing 'easyPlayerAI' block in %s", path.c_str());
        return false;
    }

    _db = &db;
    _thinkTimer = 0.0f;

    bool valid = true;

    if (config->has("thinkInterval") && config->get("thinkInterval")->isNumber()) {
        _thinkInterval = config->getFloat("thinkInterval");
    } else {
        CULogError("EasyPlayerAI::init — missing or invalid 'thinkInterval' in %s", path.c_str());
        valid = false;
    }

    if (config->has("aggressionWeight") && config->get("aggressionWeight")->isNumber()) {
        _aggressionWeight = config->getFloat("aggressionWeight");
    } else {
        CULogError("EasyPlayerAI::init — missing or invalid 'aggressionWeight' in %s", path.c_str());
        valid = false;
    }

    if (config->has("supportWeight") && config->get("supportWeight")->isNumber()) {
        _supportWeight = config->getFloat("supportWeight");
    } else {
        CULogError("EasyPlayerAI::init — missing or invalid 'supportWeight' in %s", path.c_str());
        valid = false;
    }

    if (config->has("healThreshold") && config->get("healThreshold")->isNumber()) {
        _healThreshold = config->getFloat("healThreshold");
    } else {
        CULogError("EasyPlayerAI::init — missing or invalid 'healThreshold' in %s", path.c_str());
        valid = false;
    }

    return valid;
}

/**
 * Returns whether the AI has at least one attack item in its inventory.
 */
bool EasyPlayerAI::canAttack() const {
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) return true;
    }
    return false;
}

/**
 * Returns whether the AI has a support item AND at least one neighbor
 * (left or right) is alive and below _healThreshold.
 */
bool EasyPlayerAI::canSupport() const {
    bool hasSupportItem = false;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            hasSupportItem = true;
            break;
        }
    }
    if (!hasSupportItem) return false;

    auto needsHeal = [&](Player* p) {
        return p && p->isAlive() &&
               p->getCurrentHealth() / p->getMaxHealth() < _healThreshold;
    };

    return needsHeal(getLeftPlayer()) || needsHeal(getRightPlayer());
}

/**
 * Weighted random selection between ATTACK and SUPPORT based on inventory
 * and aggressionWeight vs supportWeight.
 */
EasyPlayerAI::State EasyPlayerAI::evaluate(const Enemy& enemy) {
    if (getInventory().empty()) return State::IDLE;

    if (!canAttack() && !canSupport()) return State::PASS;

    float totalWeight = 0.0f;
    if (canAttack())  totalWeight += _aggressionWeight;
    if (canSupport()) totalWeight += _supportWeight;

    float roll = static_cast<float>(rand()) / RAND_MAX * totalWeight;

    if (canAttack() && roll < _aggressionWeight) {
        return State::ATTACK;
    } else {
        return State::SUPPORT;
    }
}

/**
 * Collects all attack items from inventory and picks one at random to use.
 */
void EasyPlayerAI::actAttack(Enemy& enemy, ItemController& items) {
    std::vector<ItemInstance::ItemId> attackItems;

    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            attackItems.push_back(item.getId());
        }
    }

    if (attackItems.empty()) return;

    ItemInstance::ItemId chosen = attackItems[rand() % attackItems.size()];
    useItemById(chosen, enemy, *_db);
}

/**
 * Finds the most injured neighbor below _healThreshold, then picks a random
 * support item to use on them.
 */
void EasyPlayerAI::actSupport(ItemController& items) {
    Player* target = nullptr;
    float lowestRatio = _healThreshold;

    auto check = [&](Player* p) {
        if (!p || !p->isAlive()) return;
        float ratio = p->getCurrentHealth() / p->getMaxHealth();
        if (ratio < lowestRatio) {
            lowestRatio = ratio;
            target = p;
        }
    };

    check(getLeftPlayer());
    check(getRightPlayer());

    if (!target) return;

    std::vector<ItemInstance::ItemId> supportItems;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            supportItems.push_back(item.getId());
        }
    }

    if (supportItems.empty()) return;

    ItemInstance::ItemId chosen = supportItems[rand() % supportItems.size()];
    useItemById(chosen, *target, *_db);
}

/**
 * Picks a random item and passes it to a random alive neighbor.
 */
void EasyPlayerAI::actPass() {
    if (getInventory().empty()) return;

    std::vector<Player*> targets;
    if (getLeftPlayer()  && getLeftPlayer()->isAlive())
        targets.push_back(getLeftPlayer());
    if (getRightPlayer() && getRightPlayer()->isAlive())
        targets.push_back(getRightPlayer());

    if (targets.empty()) return;

    const auto& inventory = getInventory();
    ItemInstance item     = inventory[rand() % inventory.size()];
    Player* chosenTarget  = targets[rand() % targets.size()];

    removeItemById(item.getId());
    chosenTarget->addItem(item);
}

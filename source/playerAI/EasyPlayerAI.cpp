#include "EasyPlayerAI.h"

/**
 * Reads the "easyPlayerAI" sub-object from playerAI.json and loads shared fields.
 */
bool EasyPlayerAI::init(Player* player, const ItemDatabase& db,
                        const std::string& path) {
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

    // Read the easyPlayerAI sub-object
    auto config = json->get("easyPlayerAI");
    if (!config) {
        CULogError("EasyPlayerAI::init — missing 'easyPlayerAI' block in %s", path.c_str());
        return false;
    }

    _player = player;
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
 *
 * @return true if an attack item is available, false otherwise.
 */
bool EasyPlayerAI::canAttack() const {
    for (const ItemInstance& item : _player->getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) return true;
    }
    return false;
}

/**
 * Returns whether the AI has a support item AND at least one neighbor
 * (left or right) is alive and below _healThreshold.
 *
 * @return true if a support action is viable, false otherwise.
 */
bool EasyPlayerAI::canSupport() const {
    // Check we have a support item
    bool hasSupportItem = false;
    for (const ItemInstance& item : _player->getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            hasSupportItem = true;
            break;
        }
    }
    if (!hasSupportItem) return false;

    // Check if either neighbor needs healing
    auto needsHeal = [&](Player* p) {
        return p && p->isAlive() &&
               p->getCurrentHealth() / p->getMaxHealth() < _healThreshold;
    };

    return needsHeal(_player->getLeftPlayer()) || needsHeal(_player->getRightPlayer());
}

/**
 * Weighted random selection between ATTACK and SUPPORT based on inventory
 * and aggressionWeight vs supportWeight.
 */
EasyPlayerAI::State EasyPlayerAI::evaluate(const Enemy& enemy) {
    // Remain idle if inventory is empty
    if (!_player || _player->getInventory().empty()) return State::IDLE;

    // Pass a card if you cannot attack or support
    if (!canAttack() && !canSupport()) return State::PASS;

    float totalWeight = 0.0f;
    if (canAttack())  totalWeight += _aggressionWeight;
    if (canSupport()) totalWeight += _supportWeight;

    // Roll a random value in [0, totalWeight). The weights carve this range into
    // adjacent segments — ATTACK owns [0, aggressionWeight) and SUPPORT owns
    // [aggressionWeight, totalWeight). Whichever segment the roll lands in wins,
    // so larger weights mean larger segments and higher probability of selection.
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

    for (const ItemInstance& item : _player->getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            attackItems.push_back(item.getId());
        }
    }

    if (attackItems.empty()) return;

    // Pick a random attack item
    ItemInstance::ItemId chosen = attackItems[rand() % attackItems.size()];
    _player->useItemById(chosen, enemy, *_db);
}

/**
 * Finds the most injured neighbor below _healThreshold, then picks a random
 * support item to use on them.
 */
void EasyPlayerAI::actSupport(ItemController& items) {
    // Pick the most injured neighbor
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

    check(_player->getLeftPlayer());
    check(_player->getRightPlayer());

    if (!target) return;

    // Collect support items and pick one at random
    std::vector<ItemInstance::ItemId> supportItems;
    for (const ItemInstance& item : _player->getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            supportItems.push_back(item.getId());
        }
    }

    if (supportItems.empty()) return;

    ItemInstance::ItemId chosen = supportItems[rand() % supportItems.size()];
    _player->useItemById(chosen, *target, *_db);
}

/**
 * Picks a random item and passes it to a random alive neighbor.
 */
void EasyPlayerAI::actPass() {
    if (_player->getInventory().empty()) return;

    // Collect alive neighbors
    std::vector<Player*> targets;
    if (_player->getLeftPlayer()  && _player->getLeftPlayer()->isAlive())
        targets.push_back(_player->getLeftPlayer());
    if (_player->getRightPlayer() && _player->getRightPlayer()->isAlive())
        targets.push_back(_player->getRightPlayer());

    if (targets.empty()) return;

    // Pick a random item and a random neighbor to pass to
    const auto& inventory = _player->getInventory();
    ItemInstance item     = inventory[rand() % inventory.size()];
    Player* chosenTarget  = targets[rand() % targets.size()];

    _player->removeItemById(item.getId());
    chosenTarget->addItem(item);
}

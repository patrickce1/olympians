#include "PlayerAI.h"

static constexpr float HOUSE_WEIGHT_MIN = 0.3f;
static constexpr float HOUSE_WEIGHT_MAX = 0.7f;

/**
 * Initializes the AI controller from the "playerAI" block in playerAI.json.
 *
 * Loads thinkInterval min/max, healThreshold min/max, rarePassChance, and
 * divinePassChance. Derives initial runtime values via applyDecisionMultiplier()
 * using the current _decisionMultiplier (default 0.0 = easiest).
 *
 * @param db    Read-only reference to the item database for item lookups.
 * @param path  Path to the playerAI.json config file.
 * @return true if all required fields were loaded successfully, false otherwise.
 */
bool PlayerAI::init(const ItemDatabase& db, const std::string& path) {
    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        if (_debug) CULogError("PlayerAI::init — failed to open %s", path.c_str());
        return false;
    }

    auto json = reader->readJson();
    if (!json) {
        if (_debug) CULogError("PlayerAI::init — failed to parse %s", path.c_str());
        return false;
    }

    auto config = json->get("playerAI");
    if (!config) {
        if (_debug) CULogError("PlayerAI::init — missing 'playerAI' block in %s", path.c_str());
        return false;
    }

    _db = &db;
    _thinkTimer = 0.0f;
    bool valid = true;

    if (config->has("thinkIntervalMin") && config->get("thinkIntervalMin")->isNumber()) {
        _thinkIntervalMin = config->getFloat("thinkIntervalMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'thinkIntervalMin'");
        valid = false;
    }

    if (config->has("thinkIntervalMax") && config->get("thinkIntervalMax")->isNumber()) {
        _thinkIntervalMax = config->getFloat("thinkIntervalMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'thinkIntervalMax'");
        valid = false;
    }

    if (config->has("healThresholdMin") && config->get("healThresholdMin")->isNumber()) {
        _healThresholdMin = config->getFloat("healThresholdMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'healThresholdMin'");
        valid = false;
    }

    if (config->has("healThresholdMax") && config->get("healThresholdMax")->isNumber()) {
        _healThresholdMax = config->getFloat("healThresholdMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'healThresholdMax'");
        valid = false;
    }

    if (config->has("rarePassChanceMin") && config->get("rarePassChanceMin")->isNumber()) {
        _rarePassChanceMin = config->getFloat("rarePassChanceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rarePassChanceMin'");
        valid = false;
    }

    if (config->has("rarePassChanceMax") && config->get("rarePassChanceMax")->isNumber()) {
        _rarePassChanceMax = config->getFloat("rarePassChanceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rarePassChanceMax'");
        valid = false;
    }

    if (config->has("divinePassChanceMin") && config->get("divinePassChanceMin")->isNumber()) {
        _divinePassChanceMin = config->getFloat("divinePassChanceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divinePassChanceMin'");
        valid = false;
    }

    if (config->has("divinePassChanceMax") && config->get("divinePassChanceMax")->isNumber()) {
        _divinePassChanceMax = config->getFloat("divinePassChanceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divinePassChanceMax'");
        valid = false;
    }
    
    if (config->has("rockPassChanceMin") && config->get("rockPassChanceMin")->isNumber()) {
        _rockPassChanceMin = config->getFloat("rockPassChanceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rockPassChanceMin'");
        valid = false;
    }

    if (config->has("rockPassChanceMax") && config->get("rockPassChanceMax")->isNumber()) {
        _rockPassChanceMax = config->getFloat("rockPassChanceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rockPassChanceMax'");
        valid = false;
    }

    if (valid) applyDecisionMultiplier();
    return valid;
}

// ---------------------------------------------------------------------------
// Decision multiplier
// ---------------------------------------------------------------------------

/**
 * Sets the AI difficulty multiplier and re-derives all runtime parameters.
 *
 * @param multiplier  Difficulty in [0, 1]. Clamped to that range.
 */
void PlayerAI::setDecisionMultiplier(float multiplier) {
    _decisionMultiplier = std::max(0.0f, std::min(1.0f, multiplier));
    applyDecisionMultiplier();
}

/**
 * Re-derives all runtime parameters from _decisionMultiplier.
 *
 * thinkInterval is interpolated from _thinkIntervalMax (worst/slowest) down
 * to _thinkIntervalMin (best/fastest), so a higher multiplier means the AI
 * acts more often.
 *
 * healThreshold is interpolated from _healThresholdMin (worst) to
 * _healThresholdMax (best), so a higher multiplier means the AI heals earlier.
 *
 * attackWeight and supportWeight are interpolated from a flat 0.5/0.5 split
 * toward the house's own attack/support ratio from houses.json. If no house
 * multipliers are available (e.g. no house assigned), weights stay at 0.5/0.5.
 *
 * effectiveRarePassChance and effectiveDivinePassChance are scaled linearly
 * from 0 to their configured maximums by _decisionMultiplier.
 */
void PlayerAI::applyDecisionMultiplier() {
    // thinkInterval: higher multiplier = shorter interval (faster AI)
    // Interpolates from max (slow/worst) down to min (fast/best)
    _thinkInterval = _thinkIntervalMax + _decisionMultiplier * (_thinkIntervalMin - _thinkIntervalMax);

    // healThreshold: higher multiplier = higher threshold (heals earlier)
    _healThreshold = _healThresholdMin + _decisionMultiplier * (_healThresholdMax - _healThresholdMin);

    // houseTypeWisdom: interpolate attack/support weights from 0.5/0.5
    // toward the house's own ratios. Falls back to 0.5/0.5 if no house is set.
    float houseAttack  = 0.5f;
    float houseSupport = 0.5f;
    if (_db) {
        const auto* mults = _db->getHouseMultipliers(getHouseName());
        if (mults) {
            float total = mults->attack + mults->support;
            if (total > 0.0f) {
                houseAttack  = mults->attack  / total;
                houseSupport = mults->support / total;
            }
            // Clamp so no house goes more extreme than 70/30 in either direction.
            // This ensures even pure-attack (Ares: 1/0) or pure-support (Demeter: 0/1)
            // houses retain meaningful behaviour in both action types.
            houseAttack  = std::max(HOUSE_WEIGHT_MIN, std::min(HOUSE_WEIGHT_MAX, houseAttack));
            houseSupport = 1.0f - houseAttack;
        }
    }
    _attackWeight  = 0.5f + _decisionMultiplier * (houseAttack  - 0.5f);
    _supportWeight = 0.5f + _decisionMultiplier * (houseSupport - 0.5f);

    // rarityWisdom: scale pass chances linearly from 0 at worst to full at best
    _effectiveRarePassChance   = _rarePassChanceMin   + _decisionMultiplier * (_rarePassChanceMax   - _rarePassChanceMin);
    _effectiveDivinePassChance = _divinePassChanceMin + _decisionMultiplier * (_divinePassChanceMax - _divinePassChanceMin);
    _effectiveRockPassChance = _rockPassChanceMin + _decisionMultiplier * (_rockPassChanceMax - _rockPassChanceMin);

    CULog(
        "[PlayerAI '%s'] multiplier=%.2f → interval=%.2f heal=%.2f "
        "atk=%.2f sup=%.2f rarePass=%.2f divinePass=%.2f rockPass=%.2f",
        getPlayerName().c_str(), _decisionMultiplier,
        _thinkInterval, _healThreshold,
        _attackWeight, _supportWeight,
        _effectiveRarePassChance, _effectiveDivinePassChance,
        _effectiveRockPassChance);
}

// ---------------------------------------------------------------------------
// Ownership tracking
// ---------------------------------------------------------------------------

/**
 * Records an item ID as originally spawned for this AI.
 * Must NOT be called for items received via a pass.
 *
 * @param itemId  The ItemId of the newly spawned item to mark as owned.
 */
void PlayerAI::onItemSpawned(ItemInstance::ItemId itemId) {
    _ownedItemIds.insert(itemId);
}

// ---------------------------------------------------------------------------
// Update (FSM loop)
// ---------------------------------------------------------------------------

/**
 * Steps the AI forward by one frame.
 *
 * @param dt     Time elapsed since the last frame in seconds.
 * @param enemy  The current enemy.
 * @param items  The ItemController.
 */
void PlayerAI::update(float dt, Enemy& enemy, ItemController& items) {
    _thinkTimer += dt;
    if (_thinkTimer < _thinkInterval) return;
    _thinkTimer = 0.0f;

    if (_debug) CULog("[PlayerAI '%s'] inventory=%d hp=%.1f/%.1f multiplier=%.2f",
          getPlayerName().c_str(),
          (int)getInventory().size(),
          getCurrentHealth(), getMaxHealth(),
          _decisionMultiplier);

    _state = evaluate(enemy);

    // Dead AI cannot attack or support — fall through to pass or idle
    if (!isAlive() && (_state == State::ATTACK || _state == State::SUPPORT)) {
        _state = (rand() % 2 == 0) ? State::PASS : State::IDLE;
    }

    switch (_state) {
        case State::ATTACK:
            if (!isAlive()) break;
            if (_debug) CULog("[PlayerAI '%s'] state → ATTACK", getPlayerName().c_str());
            actAttack(enemy, items);
            break;
        case State::SUPPORT:
            if (!isAlive()) break;
            if (_debug) CULog("[PlayerAI '%s'] state → SUPPORT", getPlayerName().c_str());
            actSupport(items);
            break;
        case State::PASS:
            if (_debug) CULog("[PlayerAI '%s'] state → PASS", getPlayerName().c_str());
            actPass();
            break;
        case State::IDLE:
            if (_debug) CULog("[PlayerAI '%s'] state → IDLE", getPlayerName().c_str());
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Returns whether the AI has at least one attack item in its inventory.
 *
 * @return true if any inventory item has type ItemDef::Type::Attack.
 */
bool PlayerAI::canAttack() const {
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) return true;
    }
    return false;
}

/**
 * Returns whether the AI has at least one support item in its inventory,
 * regardless of whether any neighbor currently needs healing.
 * Used by evaluate() to decide if SUPPORT is a viable roll option.
 *
 * @return true if any inventory item has type ItemDef::Type::Support.
 */
bool PlayerAI::hasSupportItem() const {
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) return true;
    }
    return false;
}

/**
 * Returns whether the AI has a support item AND at least one neighbor is
 * alive and below _healThreshold. Used only by actSupport() to find a target.
 *
 * @return true if a support item exists and a valid heal target is available.
 */
bool PlayerAI::canSupport() const {
    if (!hasSupportItem()) return false;

    auto needsHeal = [&](Player* player) {
        return player && player->isAlive() &&
               player->getCurrentHealth() < 100;
    };
    return needsHeal(getLeftPlayer()) || needsHeal(getRightPlayer());
}

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------

/**
 * Evaluates game context and returns the state the AI should transition to.
 *
 * Priority order:
 *   1. Support check — if the AI has a support item and any neighbor needs
 *      healing (below _healThreshold), heal immediately. This is always the
 *      top priority so teammates are never left injured while the AI attacks.
 *   2. Rarity-pass check — scan for unowned non-attack divine items
 *      (divinePassChance), then unowned non-attack rare items (rarePassChance).
 *      Attack items are never passed. First hit returns PASS.
 *   3. Attack — if the AI has an attack item, attack the enemy.
 *   4. Fallback — if nothing else is viable, pass a random item or idle.
 *
 * @param enemy  The current enemy.
 * @return The State the AI should transition to this think cycle.
 */
PlayerAI::State PlayerAI::evaluate(const Enemy& enemy) {
    _pendingPassItemId = 0;
    _pendingPassTarget = nullptr;

    if (getInventory().empty()) return State::IDLE;

    // --- 1. Support check ---
    // Always heal first if a teammate needs it and we have a support item.
    // Uses _healThreshold so better AI heals earlier, worse AI only heals
    // when teammates are nearly dead.
    if (hasSupportItem()) {
        auto needsHeal = [&](Player* player) {
            return player && player->isAlive() &&
                   player->getCurrentHealth() / player->getMaxHealth() < _healThreshold;
        };
        if (needsHeal(getLeftPlayer()) || needsHeal(getRightPlayer())) {
            if (_debug) CULog("[PlayerAI '%s'] evaluate — support priority (teammate below %.0f%%)",
                  getPlayerName().c_str(), _healThreshold * 100.0f);
            return State::SUPPORT;
        }
    }

    // --- 2. Rarity-pass check (non-attack items only) ---
    // Attack items are never passed — they should always be used on the enemy.
    // Pass target preference is based on house affinity: if a neighbor plays
    // the house the item belongs to, they get priority. If neither neighbor
    // matches, passes to a random alive neighbor.
    bool hasAliveNeighbor = (getLeftPlayer()  && getLeftPlayer()->isAlive()) ||
                            (getRightPlayer() && getRightPlayer()->isAlive());

    if (hasAliveNeighbor) {
        // Returns the alive neighbor whose house matches this item's affinity,
        // or nullptr if neither neighbor matches (actPass() picks randomly).
        auto findAffinityNeighbor = [&](const ItemInstance& inventoryItem) -> Player* {
            auto itemDef = _db->getDef(inventoryItem.getDefId());
            if (!itemDef) return nullptr;

            ItemDef::House itemAffinity = itemDef->getHouseAffinity();
            if (itemAffinity == ItemDef::House::None) return nullptr;

            auto matchesAffinity = [&](Player* neighbor) -> bool {
                if (!neighbor || !neighbor->isAlive()) return false;
                return ItemDef::houseFromString(neighbor->getHouseName(), ItemDef::House::None)
                       == itemAffinity;
            };

            if (matchesAffinity(getLeftPlayer()))  return getLeftPlayer();
            if (matchesAffinity(getRightPlayer())) return getRightPlayer();
            return nullptr; // no affinity match — actPass() picks randomly
        };

        for (const ItemInstance& inventoryItem : getInventory()) {
            auto itemDef = _db->getDef(inventoryItem.getDefId());
            if (!itemDef) continue;
            if (itemDef->getType() == ItemDef::Type::Attack) continue; // never pass attack items

            float passChance = 0.0f;
            if      (itemDef->getRarity() == ItemDef::Rarity::Divine) passChance = _effectiveDivinePassChance;
            else if (itemDef->getRarity() == ItemDef::Rarity::Rare)   passChance = _effectiveRarePassChance;
            else continue; // commons are never rarity-passed

            float passRoll = static_cast<float>(rand()) / RAND_MAX;
            if (passRoll < passChance) {
                _pendingPassItemId = inventoryItem.getId();
                _pendingPassTarget = findAffinityNeighbor(inventoryItem);
                if (_debug) CULog(
                    "[PlayerAI '%s'] evaluate — rarity pass triggered (roll=%.2f chance=%.2f) → target='%s'",
                    getPlayerName().c_str(), passRoll, passChance,
                    _pendingPassTarget ? _pendingPassTarget->getPlayerName().c_str() : "random");
                return State::PASS;
            }
        }
    }

    // --- 3. Weighted action roll ---
    // No emergency heal and no rarity-pass fired. Use the house-weighted ratio
    // to decide between attacking and supporting. Better AI will lean toward
    // their house's natural style; worse AI is closer to 50/50.
    // Falls back to whichever is available if only one option exists.
    bool canAttackNow  = canAttack();
    bool canSupportNow = canSupport();

    if (!canAttackNow && !canSupportNow) {
        if (_debug) CULog("[PlayerAI '%s'] evaluate — fallback pass/idle", getPlayerName().c_str());
        return hasAliveNeighbor ? State::PASS : State::IDLE;
    }

    if (canAttackNow && !canSupportNow) {
        if (_debug) CULog("[PlayerAI '%s'] evaluate — attacking (no support items)", getPlayerName().c_str());
        return State::ATTACK;
    }

    if (!canAttackNow && canSupportNow) {
        if (_debug) CULog("[PlayerAI '%s'] evaluate — supporting (no attack items)", getPlayerName().c_str());
        return State::SUPPORT;
    }

    // Both options available — roll against house-weighted ratio
    float totalWeight = _attackWeight + _supportWeight;
    float actionRoll  = static_cast<float>(rand()) / RAND_MAX * totalWeight;

    if (actionRoll < _attackWeight) {
        if (_debug) CULog("[PlayerAI '%s'] evaluate — attacking (roll=%.2f atk=%.2f sup=%.2f)",
              getPlayerName().c_str(), actionRoll, _attackWeight, _supportWeight);
        return State::ATTACK;
    } else {
        if (_debug) CULog("[PlayerAI '%s'] evaluate — supporting (roll=%.2f atk=%.2f sup=%.2f)",
              getPlayerName().c_str(), actionRoll, _attackWeight, _supportWeight);
        return State::SUPPORT;
    }
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

/**
 * Collects all attack items from inventory and uses a random one on the enemy.
 * Does nothing if no attack items are found.
 *
 * @param enemy  The enemy to attack.
 * @param items  The ItemController used to resolve the item action.
 */
void PlayerAI::actAttack(Enemy& enemy, ItemController& items) {
    std::vector<ItemInstance::ItemId> attackItems;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            attackItems.push_back(item.getId());
        }
    }
    if (attackItems.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actAttack — no attack items, aborting", getPlayerName().c_str());
        return;
    }
    ItemInstance::ItemId chosen = attackItems[rand() % attackItems.size()];
    if (_debug) CULog("[PlayerAI '%s'] actAttack — using item %llu on '%s'",
          getPlayerName().c_str(), (unsigned long long)chosen, enemy.getId().c_str());
    useItemById(chosen, enemy, *_db);
}

/**
 * Finds the most injured neighbor and uses a random
 * support item on them. Does nothing if no valid target or support item exists.
 *
 * @param items  The ItemController used to resolve the item action.
 */
void PlayerAI::actSupport(ItemController& items) {
    Player* target = nullptr;
    float lowestRatio = 1.0f;

    auto check = [&](Player* p) {
        if (!p || !p->isAlive()) return;
        float ratio = p->getCurrentHealth() / p->getMaxHealth();
        if (ratio < lowestRatio) { lowestRatio = ratio; target = p; }
    };
    check(getLeftPlayer());
    check(getRightPlayer());

    if (!target) {
        if (_debug) CULog("[PlayerAI '%s'] actSupport — no alive neighbour, aborting",
                getPlayerName().c_str());
        return;
    }

    std::vector<ItemInstance::ItemId> supportItems;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            supportItems.push_back(item.getId());
        }
    }
    if (supportItems.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actSupport — no support items, aborting", getPlayerName().c_str());
        return;
    }

    ItemInstance::ItemId chosen = supportItems[rand() % supportItems.size()];
    
    // Gaia rock check — using a gaia_rock on a teammate damages them.
    // Roll against _effectiveRockPassChance and pass it instead if triggered.
    auto chosenDef = _db->getDef("");
    for (const ItemInstance& inventoryItem : getInventory()) {
        if (inventoryItem.getId() == chosen) {
            chosenDef = _db->getDef(inventoryItem.getDefId());
            break;
        }
    }

    if (chosenDef && chosenDef->getId() == "gaia_rock") {
        float rockRoll = static_cast<float>(rand()) / RAND_MAX;
        if (rockRoll < _effectiveRockPassChance) {
            if (_debug) CULog(
                "[PlayerAI '%s'] actSupport — gaia_rock pass triggered (roll=%.2f chance=%.2f)",
                getPlayerName().c_str(), rockRoll, _effectiveRockPassChance);
            // Divert to a pass — set pending item and let actPass() handle delivery
            _pendingPassItemId = chosen;
            _pendingPassTarget = nullptr; // no affinity target for rocks — pass randomly
            actPass();
            return;
        }
    }
    if (_debug) CULog("[PlayerAI '%s'] actSupport — using item %llu on '%s' (ratio=%.2f)",
          getPlayerName().c_str(), (unsigned long long)chosen,
          target->getPlayerName().c_str(), lowestRatio);
    useItemById(chosen, *target, *_db);
}

/**
 * Passes an item to a neighbor (left or right).
 *
 * If _pendingPassItemId was set by evaluate() this cycle, that specific item
 * is passed. If _pendingPassTarget is set, the item goes directly to that
 * neighbor (the one who originally owned it). If _pendingPassTarget is nullptr,
 * the item goes to a random alive neighbor.
 *
 * When no pending item is set (fallback pass), selects an item based on
 * inventory balance: passes a support item if the AI has more support items
 * than attack items, passes an attack item if it has more attack items, or
 * picks randomly if counts are equal. This keeps the AI's inventory balanced
 * rather than hoarding one type while passing another.
 *
 * Does nothing if inventory is empty or no alive neighbor exists.
 */
void PlayerAI::actPass() {
    if (getInventory().empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actPass — inventory empty, aborting", getPlayerName().c_str());
        return;
    }

    std::vector<Player*> targets;
    if (getLeftPlayer()  && getLeftPlayer()->isAlive())  targets.push_back(getLeftPlayer());
    if (getRightPlayer() && getRightPlayer()->isAlive()) targets.push_back(getRightPlayer());

    if (targets.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actPass — no alive neighbours, aborting", getPlayerName().c_str());
        return;
    }

    // Resolve which item to pass
    const ItemInstance* resolved = nullptr;
    if (_pendingPassItemId != 0) {
        for (const ItemInstance& inventoryItem : getInventory()) {
            if (inventoryItem.getId() == _pendingPassItemId) {
                resolved = &inventoryItem;
                break;
            }
        }
        if (!resolved && _debug) {
            CULog("[PlayerAI '%s'] actPass — pending item %llu not found, falling back to balance pass",
                  getPlayerName().c_str(), (unsigned long long)_pendingPassItemId);
        }
    }

    ItemInstance itemToPass;
    if (resolved) {
        itemToPass = *resolved;
    } else {
        // Fallback pass — balance inventory by passing from the larger type bucket.
        // Count attack and support items separately, then pick from whichever
        // is larger. If equal, pick randomly across all items.
        std::vector<ItemInstance::ItemId> attackItems;
        std::vector<ItemInstance::ItemId> supportItems;

        for (const ItemInstance& inventoryItem : getInventory()) {
            auto itemDef = _db->getDef(inventoryItem.getDefId());
            if (!itemDef) continue;
            if      (itemDef->getType() == ItemDef::Type::Attack)  attackItems.push_back(inventoryItem.getId());
            else if (itemDef->getType() == ItemDef::Type::Support) supportItems.push_back(inventoryItem.getId());
        }

        ItemInstance::ItemId chosenId = 0;
        if (attackItems.size() > supportItems.size() && !attackItems.empty()) {
            // More attack items — pass one to balance toward support
            chosenId = attackItems[rand() % attackItems.size()];
            if (_debug) CULog("[PlayerAI '%s'] actPass — balance pass: passing attack item (atk=%zu sup=%zu)",
                  getPlayerName().c_str(), attackItems.size(), supportItems.size());
        } else if (supportItems.size() > attackItems.size() && !supportItems.empty()) {
            // More support items — pass one to balance toward attack
            chosenId = supportItems[rand() % supportItems.size()];
            if (_debug) CULog("[PlayerAI '%s'] actPass — balance pass: passing support item (atk=%zu sup=%zu)",
                  getPlayerName().c_str(), attackItems.size(), supportItems.size());
        } else {
            // Equal counts or only one type present — pick randomly from full inventory
            const auto& inventory = getInventory();
            chosenId = inventory[rand() % inventory.size()].getId();
            if (_debug) CULog("[PlayerAI '%s'] actPass — balance pass: random (atk=%zu sup=%zu)",
                  getPlayerName().c_str(), attackItems.size(), supportItems.size());
        }

        // Resolve the chosen ID back to an ItemInstance
        for (const ItemInstance& inventoryItem : getInventory()) {
            if (inventoryItem.getId() == chosenId) {
                itemToPass = inventoryItem;
                break;
            }
        }
    }

    _pendingPassItemId = 0;

    // Resolve target — prefer the affinity neighbor set by evaluate(), fall back to random
    Player* chosenTarget = nullptr;
    if (_pendingPassTarget && _pendingPassTarget->isAlive()) {
        chosenTarget = _pendingPassTarget;
    } else {
        chosenTarget = targets[rand() % targets.size()];
    }
    _pendingPassTarget = nullptr;

    int passDirection = 0;
    if (chosenTarget == getLeftPlayer())       passDirection = 1;
    else if (chosenTarget == getRightPlayer()) passDirection = 2;

    if (_debug) CULog("[PlayerAI '%s'] actPass — passing item %llu to '%s'",
          getPlayerName().c_str(), (unsigned long long)itemToPass.getId(),
          chosenTarget->getPlayerName().c_str());

    removeItemById(itemToPass.getId());
    itemToPass.setPassDirection(passDirection);
    itemToPass.setSlideOrigin(ItemInstance::SlideOriginType::SLIDE_FROM_PASS);
    chosenTarget->addItem(itemToPass);
}

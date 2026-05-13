#include <cugl/cugl.h>
#include "GameState.h"
#include <array>
#include <cstdlib>

/**
 * Loads house definitions from JSON into the house loader.
 * Must be called first in init() since player construction depends on it.
 *
 * @return true if the house file loaded successfully.
 */
bool GameState::initHouses() {
    const std::string houseJsonPath = "json/houses.json";
    if (!_houseLoader.loadFromFile(houseJsonPath)) {
        CULog("GameState: Failed to load house.json");
        return false;
    }
    return true;
}

/**
 * Builds the player array (one human + three AI), links all players in a
 * circular neighbour ring, and populates the player ID map.
 * Must be called after intHouses() so the house loader is ready.
 */
void GameState::initPlayers() {
    _players.reserve(4);

    auto humanPlayer = std::make_shared<Player>("", 0, "Player 1", _houseLoader);
    _players.push_back(humanPlayer);

    for (int i = 1; i <= 3; i++) {
       auto aiPlayer = std::make_shared<EasyPlayerAI>(
            "", i,
            "AI Player " + std::to_string(i),
            _houseLoader
        );
        _players.push_back(aiPlayer);
    }

    // Circular neighbour linking: 0 <-> 1 <-> 2 <-> 3 <-> 0
    const int playerCount = (int)_players.size();
    for (int i = 0; i < playerCount; i++) {
        int leftIdx  = (i - 1 + playerCount) % playerCount;
        int rightIdx = (i + 1) % playerCount;
        _players[i]->setLeftPlayer(_players[leftIdx].get());
        _players[i]->setRightPlayer(_players[rightIdx].get());
    }

    // Populate network lookup map (key == array index until lobby assigns real IDs).
    for (int i = 0; i < playerCount; i++) {
        _playerIdMap[i] = _players[i].get();
    }

    // Default to index 0; setLocalPlayer() is called again after network lobby.
    //figure out our own location in the circle
    setLocalPlayer(0);
}

/**
 * Replaces the AI placeholder at the given slot with a real human player,
 * using the house they selected in the character select screen.
 *
 * Called during game setup after the lobby has finalized the player order
 * and all players have broadcast their house selections.
 *
 * After replacing the player object, all neighbour pointers in the circular
 * ring are re-wired so that every player's left/right references remain valid.
 *
 * @param playerNumber  The 0-based slot index of the player to promote.
 * @param playerName    The display name of the player joining this slot.
 * @param houseName     The ID of the house the player selected (e.g. "athena").
 *                      Must match a valid house definition in the HouseLoader.
 *                      Passing an unrecognized ID will produce a player with
 *                      default/missing stats and may cause a crash downstream.
 */
void GameState::setRealPlayer(int playerNumber, const std::string& playerName, const std::string& houseName) {
    if (playerNumber < 0 || playerNumber >= (int)_players.size()) return;

    const bool replacedLocalPlayer = (_localPlayer == _players[playerNumber].get());

    if (houseName.empty()) {
        // Always reconstruct with no house to guarantee house is cleared,
        // regardless of whether the slot was previously AI or real
        _players[playerNumber] = std::make_shared<Player>(
            "",
            playerNumber,
            playerName,
            _houseLoader
        );
        _playerIdMap[playerNumber] = _players[playerNumber].get();

        const int n = (int)_players.size();
        for (int i = 0; i < n; i++) {
            _players[i]->setLeftPlayer (_players[(i - 1 + n) % n].get());
            _players[i]->setRightPlayer(_players[(i + 1)     % n].get());
        }
        
        if (replacedLocalPlayer) {
            _localPlayer = _players[playerNumber].get();
        }
        return;
    }

    // Full reconstruction with house stats
    _players[playerNumber] = std::make_shared<Player>(
        houseName,
        playerNumber,
        playerName,
        _houseLoader
    );
    _playerIdMap[playerNumber] = _players[playerNumber].get();

    const int n = (int)_players.size();
    for (int i = 0; i < n; i++) {
        _players[i]->setLeftPlayer (_players[(i - 1 + n) % n].get());
        _players[i]->setRightPlayer(_players[(i + 1)     % n].get());
    }
    if (replacedLocalPlayer) {
        _localPlayer = _players[playerNumber].get();
    }
}

/**
 * Loads the enemy from JSON and initialises it.
 * Must be called after initPlayers() so the player array exists for
 * EnemyController to reference later.
 *
 * @return true if the enemy loaded and initialised successfully.
 */
bool GameState::initEnemy() {
    const std::string enemyJsonPath = "json/enemies.json";
    _enemy = std::make_shared<Cyclops>(); //cyclops is the default boss
    if (!_enemy->init("cyclops", enemyJsonPath)) {
        CULog("GameState: Failed to initialize enemy");
        return false;
    }
    CULog("GameState: Enemy initialized id='%s'", _enemy->getId().c_str());
    return true;
}

/**
 * Creates an enemy instance of the appropriate type based on enemy ID.
 * Currently supports Cyclops (custom class) and Cerberus (generic Enemy).
 * 
 * @param enemyID The unique identifier for the enemy to create
 * @return A shared pointer to the newly created enemy instance
 */
static std::shared_ptr<Enemy> createEnemyByID(const std::string& enemyID) {
    if (enemyID == "cyclops") {
        return std::make_shared<Cyclops>();
    } else if (enemyID == "cerberus") {
        // TODO: Create a custom Cerberus class in a future PR
        return std::make_shared<Enemy>();
    }
    else if (enemyID == "gaia") {
        return std::make_shared<Gaia>();
    }
    // Fallback for unknown enemy types
    return std::make_shared<Enemy>();
}

/**
 * Initialises the enemy for the game session with animation metadata from AssetManager.
 * Stores the asset manager reference and loads enemy definitions with animation registry.
 *
 * @param assets   The AssetManager containing animation metadata and asset definitions.
 * @return true if enemy initialized successfully, false on error.
 */
bool GameState::initEnemyWithAssets(const std::shared_ptr<cugl::AssetManager>& assets) {
    const std::string enemyJsonPath = "json/enemies.json";
    _enemy = createEnemyByID("cyclops");
    _assets = assets;
    
    if (!_enemy->init("cyclops", enemyJsonPath, assets)) {
        CULog("ERROR: Failed to initialize enemy");
        return false;
    }
    
    CULog("GameState: Enemy initialized id='%s'", _enemy->getId().c_str());
    return true;
}

/**
 * Finishes initialising all AI-controlled players using the item database.
 * Must be called after initPlayers() and after the ItemController is ready,
 * since AI init requires the item definition database.
 *
 * @param itemController  The ItemController whose database the AI players need.
 * @return true if all AI players initialised successfully.
 */
bool GameState::initAI(ItemController& itemController) {
    const std::string aiConfigPath = "json/playerAI.json";
    const int playerCount = (int)_players.size();

    for (int i = 0; i < playerCount; i++) {
        auto* ai = dynamic_cast<PlayerAI*>(_players[i].get());
        if (!ai) {
            CULog("GameState: Player %d is not a PlayerAI — skipping AI init", i);
            continue;
        }
        if (!ai->init(itemController.getDatabase(), aiConfigPath)) {
            CULog("GameState: Failed to initialize AI for player %d", i);
            return false;
        }
        CULog("GameState: Initialized AI for player %d", i);
    }
    return true;
}

/**
 * Initialises the game world: loads houses and the enemy from JSON,
 * builds the player array (one human + three AI), links all players in a
 * circular neighbour ring, and finishes AI initialisation using the
 * provided item database.
 *
 * @param itemController  The ItemController whose database is needed for
 *                        AI player initialisation.
 * @param assets          The AssetManager containing animation metadata and
 *                        asset definitions needed for enemy initialisation.
 * @return true if all resources loaded and initialised successfully.
 */
bool GameState::init(ItemController& itemController, const std::shared_ptr<cugl::AssetManager>& assets) {
    _itemDatabase = &itemController.getDatabase();
    if (!initHouses())                      return false;
    initPlayers();
    if (!initEnemyWithAssets(assets))       return false;
    if (!initAI(itemController))            return false;
    return true;
}

/**
 * Releases all owned resources and resets every pointer to nullptr.
 */
void GameState::dispose() {
    for (auto& player : _players) {
        player->clearInventory();
        player->clearRuntimeEffects();
        player->clearItemUseState();
    }
    _players.clear();
    _playerIdMap.clear();
    _localPlayer = nullptr;
    _enemy.reset();
}

/**
 * Resets all players' inventories to their default state.
 * Does not reload assets or rebuild the player array.
 */
void GameState::reset() {
    for (auto& player : _players) {
        player->clearInventory();
        player->clearRuntimeEffects();
        player->clearItemUseState();
        player->setCurrentHealth(player->getMaxHealth());
    }
    _enemy->setCurrentHealth(_enemy->getMaxHealth());
}

/**
 * Assigns the local player by index into the player array.
 *
 * @param assignedIndex  Zero-based index into the player array.
 */
void GameState::setLocalPlayer(int assignedIndex) {
    CUAssertLog(
        assignedIndex >= 0 && assignedIndex < (int)_players.size(),
        "GameState::setLocalPlayer — assigned index out of range"
    );
    _localPlayer = _players[assignedIndex].get();
}

/**
 * Assigns the enemy for the game session without animation metadata.
 * Creates an appropriate enemy instance and initializes it from JSON.
 *
 * @param enemyID  The unique ID of the chosen enemy (e.g., "cyclops", "cerberus").
 */
void GameState::setEnemy(std::string enemyID) {
    const std::string enemyJsonPath = "json/enemies.json";
    _enemy = createEnemyByID(enemyID);
    _enemy->init(enemyID, enemyJsonPath);
}

/**
 * Assigns the enemy for the game session with animation metadata from AssetManager.
 * Creates an appropriate enemy instance and initializes it with animation registry.
 *
 * @param enemyID  The unique ID of the chosen enemy (e.g., "cyclops", "cerberus").
 * @param assets   The AssetManager containing animation metadata in enemyAnimations.json.
 */
void GameState::setEnemy(std::string enemyID, const std::shared_ptr<cugl::AssetManager>& assets) {
    const std::string enemyJsonPath = "json/enemies.json";
    _enemy = createEnemyByID(enemyID);
    _enemy->init(enemyID, enemyJsonPath, assets);
}

/**
 * Returns the player associated with a given network player ID.
 *
 * @param playerId  The network-assigned player ID.
 * @return          The matching Player pointer, or nullptr if not found.
 */
Player* GameState::getPlayerById(int playerId) const {
    auto playerEntry = _playerIdMap.find(playerId);
    return (playerEntry != _playerIdMap.end()) ? playerEntry->second : nullptr;
}

/**
 * Returns a raw pointer to the player at the given slot index.
 *
 * @param  slot  Zero-based index into the player array.
 * @return      The Player at that slot, or nullptr if out of range.
 */
Player* GameState::getPlayerBySlot(int slot) const {
    if (slot < 0 || slot >= (int)_players.size()) return nullptr;
    return _players[slot].get();
}

/* Goes through the list of attack messages in attacks and applies the damage specified to the boss*/
void GameState::attackUpdates(std::vector<AttackMessage> attacks) {
    for (const AttackMessage& attack : attacks) {
        float authoritativeDamage = attack.damage;

        if (_itemDatabase) {
            Player* attackingPlayer = getPlayerBySlot(attack.damageDirection);
            std::shared_ptr<ItemDef> def = _itemDatabase->getDef(attack.itemDefID);
            if (attackingPlayer && def && def->getType() == ItemDef::Type::Attack) {
                authoritativeDamage = attackingPlayer->resolveItemMagnitude(*def, *_itemDatabase);
                attackingPlayer->recordItemUse(*def);
            }
        }

        const float enemyHealthBefore = _enemy->getCurrentHealth();
        _enemy->takeDamage(authoritativeDamage, attack.damageDirection);
        Player* attackingPlayer = getPlayerBySlot(attack.damageDirection);
        if (attackingPlayer) {
            attackingPlayer->applyLifestealHeal(std::max(0.0f, enemyHealthBefore - _enemy->getCurrentHealth()));
        }
    }
}

/* Goes through the list of heal messages in heals and increases player health according to the heal amount*/
void GameState::healUpdates(std::vector<HealMessage> heals) {
    for (HealMessage heal : heals) {
        if (heal.playerID < 0 || heal.playerID >= (int)_players.size()) continue;
        _players[heal.playerID]->updateHealth(heal.heal);
    }
}

/**
 * Applies all queued boss heal messages to the enemy's current health.
 * Called by the host each frame after processing incoming network messages.
 * Currently used exclusively for Gaia's rock item, which heals the boss
 * instead of dealing damage.
 *
 * @param bossHeals  The queued boss heal updates to apply this frame.
 */
void GameState::bossHealUpdates(std::vector<BossHealMessage> bossHeals) {
    for (BossHealMessage bossHeal : bossHeals) {
        _enemy->updateHealth(bossHeal.healAmount);
    }
}

/**
 * Goes through the list of support effect messages and applies them to the specified player.
 *
 * @param supportEffects  The queued support-effect updates to apply this frame.
 */
void GameState::supportEffectUpdates(std::vector<SupportEffectMessage> supportEffects) {
    auto isPartyCharmActive = [&]() {
        for (const auto& player : _players) {
            if (player && player->hasCharm()) {
                return true;
            }
        }
        return false;
    };

    for (const SupportEffectMessage& effect : supportEffects) {
        SupportEffectMessage resolvedEffect = effect;
        if (isPartyCharmActive() && effect.effectType != SupportEffectType::Charm) {
            switch (effect.effectType) {
                case SupportEffectType::Shield:
                    resolvedEffect.magnitude *= 2.0f;
                    resolvedEffect.duration *= 2.0f;
                    break;
                case SupportEffectType::Barrier:
                    resolvedEffect.magnitude *= 0.5f;
                    resolvedEffect.duration *= 2.0f;
                    break;
                case SupportEffectType::Regen:
                    resolvedEffect.magnitude *= 2.0f;
                    break;
                case SupportEffectType::Resurrect:
                    resolvedEffect.magnitude *= 2.0f;
                    resolvedEffect.secondaryMagnitude *= 2.0f;
                    break;
                case SupportEffectType::Educate:
                    resolvedEffect.duration *= 2.0f;
                    break;
                case SupportEffectType::Frenzy:
                    resolvedEffect.magnitude *= 0.5f;
                    break;
                case SupportEffectType::Lifesteal:
                    resolvedEffect.magnitude *= 2.0f;
                    break;
                case SupportEffectType::Heal:
                case SupportEffectType::Forge:
                case SupportEffectType::Charm:
                    break;
            }
        }

        auto applySupportEffect = [&](Player& target) {
            switch (resolvedEffect.effectType) {
                case SupportEffectType::Heal:
                    target.updateHealth(resolvedEffect.magnitude);
                    break;
                case SupportEffectType::Shield:
                    target.applyShield(resolvedEffect.magnitude, resolvedEffect.duration);
                    break;
                case SupportEffectType::Barrier:
                    target.applyBarrier(resolvedEffect.magnitude, resolvedEffect.duration);
                    break;
                case SupportEffectType::Regen:
                    target.applyRegen(resolvedEffect.magnitude, resolvedEffect.duration);
                    break;
                case SupportEffectType::Resurrect:
                    if (target.isAlive()) {
                        break;
                    }
                    target.setCurrentHealth(resolvedEffect.magnitude);
                    if (resolvedEffect.secondaryMagnitude > 0.0f && resolvedEffect.duration > 0.0f) {
                        target.applyRegen(resolvedEffect.secondaryMagnitude, resolvedEffect.duration);
                    }
                    break;
                case SupportEffectType::Educate:
                    target.applyEducate(resolvedEffect.duration);
                    break;
                case SupportEffectType::Charm:
                    target.applyCharm(resolvedEffect.duration);
                    break;
                case SupportEffectType::Lifesteal:
                    target.applyLifesteal(resolvedEffect.magnitude, resolvedEffect.duration);
                    break;
                case SupportEffectType::Forge:
                case SupportEffectType::Frenzy:
                    break;
            }
        };

        if (resolvedEffect.applyToAllPlayers) {
            for (const auto& player : _players) {
                if (!player) {
                    continue;
                }
                applySupportEffect(*player);
            }
            continue;
        }

        if (resolvedEffect.playerID < 0 || resolvedEffect.playerID >= (int)_players.size()) continue;

        Player* target = _players[resolvedEffect.playerID].get();
        if (!target) continue;
        applySupportEffect(*target);
    }
}

/**
 * Applies enemy-targeted effect messages from clients onto the host's authoritative enemy state.
 *
 * @param enemyEffects  The queued enemy-effect updates to apply this frame.
 */
void GameState::enemyEffectUpdates(std::vector<EnemyEffectMessage> enemyEffects) {
    if (!_enemy) return;

    bool partyCharmActive = false;
    for (const auto& player : _players) {
        if (player && player->hasCharm()) {
            partyCharmActive = true;
            break;
        }
    }

    for (EnemyEffectMessage effect : enemyEffects) {
        if (partyCharmActive) {
            switch (effect.effectType) {
                case EnemyEffectType::Stun:
                case EnemyEffectType::Love:
                    effect.duration *= 2.0f;
                    break;
                case EnemyEffectType::Slow:
                    effect.magnitude *= 0.5f;
                    effect.duration *= 2.0f;
                    break;
                case EnemyEffectType::Vulnerable:
                    effect.magnitude *= 2.0f;
                    effect.duration *= 2.0f;
                    break;
            }
        }

        switch (effect.effectType) {
            case EnemyEffectType::Stun:
                _enemy->applyStun(effect.duration);
                break;
            case EnemyEffectType::Love:
                _enemy->applyLove(effect.duration, effect.playerIndex);
                break;
            case EnemyEffectType::Slow:
                _enemy->applySlow(effect.magnitude, effect.duration);
                break;
            case EnemyEffectType::Vulnerable:
                if (effect.applyToAllSides) {
                    _enemy->applyVulnerableToAllSides(effect.magnitude, effect.duration);
                } else {
                    _enemy->applyVulnerable(effect.magnitude, effect.duration, effect.playerIndex);
                }
                break;
        }
    }
}

/**
 * Overwrites the local game state with a snapshot received from the host.
 *
 * Applies the host's authoritative boss and player health values directly,
 * discarding any local speculative state. Called once per frame on clients
 * after getNetworkUpdates() is processed.
 *
 * @param newState  The authoritative game state snapshot from the host.
 */
void GameState::networkUpdate(GameStateMessage newState) {
    // update boss health
    _enemy->setCurrentHealth(newState.bossHealth);
    
    //ensure state is synced
    _enemy->enterState((EnemyLoader::State) newState.bossState);
    _enemy->setStateTime(newState.stateTime);

    //update boss direction
    _enemy->setTargetIndex(newState.bossTarget);

    // sync authoritative enemy runtime effects
    _enemy->syncStunDuration(newState.bossStunDuration);
    _enemy->syncLoveDuration(newState.bossLoveDuration);
    _enemy->syncSlow(newState.bossSlowMultiplier, newState.bossSlowDuration);
    _enemy->syncVulnerable(newState.bossVulnerableMultipliers, newState.bossVulnerableDurations);

    // update player health and authoritative timed support effects
    std::vector<float> healths = {
        newState.player1HP,
        newState.player2HP,
        newState.player3HP,
        newState.player4HP
    };
    std::vector<std::array<float, 10>> runtimeEffects = {
        std::array<float, 10>{newState.player1ShieldMitigation, newState.player1ShieldDuration,
                             newState.player1BarrierMultiplier, newState.player1BarrierDuration,
                             newState.player1RegenAmountRemaining, newState.player1RegenDuration,
                             newState.player1EducateDuration, newState.player1CharmDuration,
                             newState.player1LifestealMultiplier, newState.player1LifestealDuration},
        std::array<float, 10>{newState.player2ShieldMitigation, newState.player2ShieldDuration,
                             newState.player2BarrierMultiplier, newState.player2BarrierDuration,
                             newState.player2RegenAmountRemaining, newState.player2RegenDuration,
                             newState.player2EducateDuration, newState.player2CharmDuration,
                             newState.player2LifestealMultiplier, newState.player2LifestealDuration},
        std::array<float, 10>{newState.player3ShieldMitigation, newState.player3ShieldDuration,
                             newState.player3BarrierMultiplier, newState.player3BarrierDuration,
                             newState.player3RegenAmountRemaining, newState.player3RegenDuration,
                             newState.player3EducateDuration, newState.player3CharmDuration,
                             newState.player3LifestealMultiplier, newState.player3LifestealDuration},
        std::array<float, 10>{newState.player4ShieldMitigation, newState.player4ShieldDuration,
                             newState.player4BarrierMultiplier, newState.player4BarrierDuration,
                             newState.player4RegenAmountRemaining, newState.player4RegenDuration,
                             newState.player4EducateDuration, newState.player4CharmDuration,
                             newState.player4LifestealMultiplier, newState.player4LifestealDuration}
    };

    for (int i = 0; i < _players.size(); i++) {
        _players[i]->setCurrentHealth(healths[i]);
        _players[i]->syncRuntimeEffects(runtimeEffects[i][0], runtimeEffects[i][1],
                                        runtimeEffects[i][2], runtimeEffects[i][3],
                                        runtimeEffects[i][4], runtimeEffects[i][5],
                                        runtimeEffects[i][6], runtimeEffects[i][7],
                                        runtimeEffects[i][8], runtimeEffects[i][9]);
        _players[i]->setMalletUseCount(newState.playerMalletUseCounts[i]);
    }
}

/** Returns whether or not the players won based on the current game state
* The game is considered won if the boss health is 0
*/
bool GameState::didWin() {
    return _enemy->getCurrentHealth() <= 0;
}

/** Returns whether or not the players lost based on the current game state
* The game is considered lost if all players reach a life of 0
*/
bool GameState::didLose() {
    return _players[0]->getCurrentHealth() <= 0
        && _players[1]->getCurrentHealth() <= 0
        && _players[2]->getCurrentHealth() <= 0
        && _players[3]->getCurrentHealth() <= 0;
}

/**
 * Assigns a unique house to every slot that does not yet have one.
 * Skips any slot that already has a house. For empty slots, builds a pool
 * of houses not yet claimed by any other slot, picks one at random, and
 * reconstructs the slot as an EasyPlayerAI with that house so AI behavior
 * is preserved. The pool is rebuilt each iteration so previously assigned
 * houses are excluded.
 *
 * Host only — rand() is called locally so clients must receive the results
 * via broadcastAIHouseSelection() rather than running this themselves.
 *
 * @param itemController  The ItemController whose database AI players need
 *                        to initialise their behavior after reconstruction.
 */
void GameState::assignMissingHousesForAI(ItemController& itemController) {
    const auto& allHouses = _houseLoader.getAllOrdered();
    if (allHouses.empty()) return;

    const int n = (int)_players.size();

    for (int i = 0; i < n; i++) {
        // Only fill slots that are AI and have no house.
        // Real players must select their own house — we must not assign one for them.
        if (!_players[i]->isAI()) continue;
        if (!_players[i]->getHouseName().empty()) continue;

        // Pool is rebuilt each iteration so previously assigned houses
        // are already reflected in _players and correctly excluded.
        std::vector<std::string> availableHouses;
        for (const auto& house : allHouses) {
            bool taken = false;
            for (const auto& player : _players) {
                if (player->getHouseName() == house.id) {
                    taken = true;
                    break;
                }
            }
            if (!taken) availableHouses.push_back(house.id);
        }

        if (availableHouses.empty()) continue;

        std::string chosenHouse = availableHouses[rand() % availableHouses.size()];

        auto ai = std::make_shared<EasyPlayerAI>(chosenHouse, i, _players[i]->getPlayerName(), _houseLoader);
        ai->init(itemController.getDatabase(), "json/playerAI.json");
        _players[i] = ai;
        _playerIdMap[i] = ai.get();
    }

    // Re-wire neighbour ring after all replacements
    for (int i = 0; i < n; i++) {
        _players[i]->setLeftPlayer(_players[(i - 1 + n) % n].get());
        _players[i]->setRightPlayer(_players[(i + 1) % n].get());
    }
}

/**
 * Replaces the player at the given slot with an EasyPlayerAI, optionally
 * preserving their house. Re-wires the neighbour ring and updates the
 * player ID map. Note: caller must call ai->init() after this to set _db.
 *
 * @param slot   The 0-based slot index of the player to demote.
 * @param house  The house ID to assign to the new AI, or "" for none.
 */
void GameState::demoteToAI(int slot, const std::string& house) {
    if (slot < 0 || slot >= (int)_players.size()) return;

    const bool replacedLocalPlayer = (_localPlayer == _players[slot].get());

    _players[slot] = std::make_shared<EasyPlayerAI>(
        house,   // preserve house instead of always passing ""
        slot,
        "AI Player " + std::to_string(slot),
        _houseLoader
    );
    _playerIdMap[slot] = _players[slot].get();

    const int n = (int)_players.size();
    for (int i = 0; i < n; i++) {
        _players[i]->setLeftPlayer (_players[(i - 1 + n) % n].get());
        _players[i]->setRightPlayer(_players[(i + 1)     % n].get());
    }
    if (replacedLocalPlayer) {
        _localPlayer = _players[slot].get();
    }
}

/**
 * Swaps two player slots in the local player array.
 * Called on the host after NetworkController::swapSlots() to keep
 * _players in sync with the updated network slot assignments.
 * Re-wires neighbour pointers for the affected slots after the swap.
 *
 * @param slotA  First 0-based slot index.
 * @param slotB  Second 0-based slot index.
 */
void GameState::swapPlayers(int slotA, int slotB) {
    int total = (int)_players.size();
    if (slotA == slotB || slotA < 0 || slotB < 0
        || slotA >= total || slotB >= total) return;

    std::swap(_players[slotA], _players[slotB]);

    // Update id map so getPlayerById() resolves correctly after the swap
    _playerIdMap[slotA] = _players[slotA].get();
    _playerIdMap[slotB] = _players[slotB].get();

    // Re-wire full circular neighbour ring — same pattern as setRealPlayer(),
    // demoteToAI(), and assignMissingHousesForAI()
    for (int i = 0; i < total; i++) {
        _players[i]->setLeftPlayer (_players[(i - 1 + total) % total].get());
        _players[i]->setRightPlayer(_players[(i + 1) % total].get());
    }
}

/**
 * Reorders the player array according to a permutation supplied by the boss
 * scramble mechanic.  After reordering, all neighbour pointers in the
 * circular ring are re-wired and the player ID map is rebuilt.
 *
 * The permutation is expressed as a "where does slot i go?" mapping:
 *   newMapping[oldSlot] = newSlot 
 * e.g. newMapping = {2, 0, 3, 1} moves
 *   old slot 0 → new slot 2
 *   old slot 1 → new slot 0
 *   old slot 2 → new slot 3
 *   old slot 3 → new slot 1
 *
 * Clients must receive the permutation over the network and
 * call this with the same array as the hosts' so all peers stay in sync.
 *
 * @param newMapping  A length-4 array where permutation[i] is the new
 *                     slot index that the player currently at slot i
 *                     should occupy.  Must be a valid permutation of
 *                     {0, 1, 2, 3}; behaviour is undefined otherwise.
 */
void GameState::applyPlayerScramble(const std::array<int, 4>& newMapping) {
    const int n = (int)_players.size();
    const int originalLocalPlayerNumber = _localPlayer->getPlayerNumber();
    CUAssertLog(n == 4, "applyScramble expects exactly 4 players");

    // Build the reordered array without mutating _players mid-loop.
    std::array<std::shared_ptr<Player>, 4> reordered;
    for (int oldSlot = 0; oldSlot < n; oldSlot++) {
        int newSlot = newMapping[oldSlot];
        CUAssertLog(newSlot >= 0 && newSlot < n, "applyScramble: permutation value out of range");
        reordered[newSlot] = _players[oldSlot];
    }

    // Apply the reordering to _players and _playerIdMap
    for (int i = 0; i < n; i++) {
        _players[i] = reordered[i];
        _playerIdMap[i] = _players[i].get();
    }

    // Relink left/right neighbors
    for (int i = 0; i < n; i++) {
        _players[i]->setLeftPlayer(_players[(i - 1 + n) % n].get());
        _players[i]->setRightPlayer(_players[(i + 1) % n].get());
    }

    // Make sure all players reset their player numbers
    for (int i = 0; i < n; i++) {
        _players[i]->setPlayerNumber(i);
    }

    // Set local player
    int newSlot = newMapping[originalLocalPlayerNumber];
    _localPlayer = _players[newSlot].get();
}
#include <cugl/cugl.h>
#include "GameState.h"
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
    _enemy = std::make_shared<Enemy>();
    if (!_enemy->init("cyclops", enemyJsonPath)) {
        CULog("GameState: Failed to initialize enemy");
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
 * @return true if all resources loaded and initialised successfully.
 */
bool GameState::init(ItemController& itemController) {
    if (!initHouses())        return false;
    initPlayers();
    if (!initEnemy())             return false;
    if (!initAI(itemController))  return false;
    return true;
}

/**
 * Releases all owned resources and resets every pointer to nullptr.
 */
void GameState::dispose() {
    for (auto& player : _players) {
        player->clearInventory();
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
 * Assigns the enemy for the game session.
 *
 * @param enemyID  the unique ID of the chosen enemy.
 */
void GameState::setEnemy(std::string enemyID) {
    const std::string enemyJsonPath = "json/enemies.json";
    if (_enemy == nullptr) {
        if (enemyID.compare("cyclops") == 0) {
            CULog("making cyclops");
            _enemy = std::make_shared<Cyclops>();
        }
        else if (enemyID.compare("cerberus") == 0) {
            //TODO for future pr: replace this with a custom Cerberus class
            CULog("making cerberus");
            _enemy = std::make_shared<Enemy>();
        }
    }
    _enemy->init(enemyID, enemyJsonPath);
};

/**
 * Returns the player associated with a given network player ID.
 *
 * @param playerId  The network-assigned player ID.
 * @return          The matching Player pointer, or nullptr if not found.
 */
Player* GameState::getPlayerById(int playerId) const {
    auto it = _playerIdMap.find(playerId);
    return (it != _playerIdMap.end()) ? it->second : nullptr;
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
    for (AttackMessage attack : attacks) {
        _enemy->takeDamage(attack.damage, attack.damageDirection);
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

    // update player health
    std::vector<float> healths = {
        newState.player1HP,
        newState.player2HP,
        newState.player3HP,
        newState.player4HP
    };

    for (int i = 0; i < _players.size(); i++) {
        _players[i]->setCurrentHealth(healths[i]);
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
 * Iterates all slots and skips any that already have a house assigned.
 * For each empty slot, builds a pool of houses not yet claimed by any
 * other slot, picks one at random, and calls setRealPlayer() to apply it.
 *
 * Host only — rand() is called locally so clients must receive the
 * results via broadcastAIHouseSelection() rather than running this
 * themselves.
 */
void GameState::assignMissingHousesForAI() {
    const auto& allHouses = _houseLoader.getAllOrdered();
    if (allHouses.empty()) return;

    const int n = (int)_players.size();

    for (int i = 0; i < n; i++) {
        if (!_players[i]->getHouseName().empty()) continue;

        // Pool is rebuilt each iteration so previously assigned houses
        // (including those just assigned to earlier AI slots this loop)
        // are already reflected in _players and correctly excluded.
        std::vector<std::string> available;
        for (const auto& house : allHouses) {
            bool taken = false;
            for (const auto& player : _players) {
                if (player->getHouseName() == house.id) {
                    taken = true;
                    break;
                }
            }
            if (!taken) available.push_back(house.id);
        }

        if (available.empty()) continue;

        std::string chosen = available[rand() % available.size()];
        setRealPlayer(i, _players[i]->getPlayerName(), chosen);
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

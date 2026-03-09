#include <cugl/cugl.h>
#include "GameState.h"

/**
 * Loads character definitions from JSON into the character loader.
 * Must be called first in init() since player construction depends on it.
 *
 * @return true if the character file loaded successfully.
 */
bool GameState::initCharacters() {
    const std::string characterJsonPath = "json/characters.json";
    if (!_characterLoader.loadFromFile(characterJsonPath)) {
        CULog("GameState: Failed to load characters.json");
        return false;
    }
    return true;
}

/**
 * Builds the player array (one human + three AI), links all players in a
 * circular neighbour ring, and populates the player ID map.
 * Must be called after initCharacters() so the character loader is ready.
 */
void GameState::initPlayers() {
    _players.reserve(4);

    auto humanPlayer = std::make_shared<Player>("Percy", 0, "Player 1", _characterLoader);
    _players.push_back(humanPlayer);

    for (int i = 1; i <= 3; i++) {
       auto aiPlayer = std::make_shared<EasyPlayerAI>(
            "Percy", i,
            "Player " + std::to_string(i),
            _characterLoader
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

void GameState::setRealPlayer(int playerNumber, const std::string& playerName) {
    if (playerNumber < 0 || playerNumber >= _players.size()) {
        CULog("Invalid player number %d", playerNumber);
        return;
    }
    _players[playerNumber] = std::make_shared<Player>(
        "Percy", playerNumber,
        playerName + std::to_string(playerNumber),
        _characterLoader
    );

    // reset all neighbors for every player
    int playerCount = _players.size();
    for (int i = 0; i < playerCount; i++) {
        int leftIdx = (i - 1 + playerCount) % playerCount;
        int rightIdx = (i + 1) % playerCount;
        _players[i]->setLeftPlayer(_players[leftIdx].get());
        _players[i]->setRightPlayer(_players[rightIdx].get());
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
    if (!_enemy->init("enemy1", enemyJsonPath)) {
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

    for (int i = 1; i < playerCount; i++) {
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
 * Initialises the game world: loads characters and the enemy from JSON,
 * builds the player array (one human + three AI), links all players in a
 * circular neighbour ring, and finishes AI initialisation using the
 * provided item database.
 *
 * @param itemController  The ItemController whose database is needed for
 *                        AI player initialisation.
 * @return true if all resources loaded and initialised successfully.
 */
bool GameState::init(ItemController& itemController) {
    if (!initCharacters())        return false;
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
    }
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
 * Returns the player associated with a given network player ID.
 *
 * @param playerId  The network-assigned player ID.
 * @return          The matching Player pointer, or nullptr if not found.
 */
Player* GameState::getPlayerById(int playerId) const {
    auto it = _playerIdMap.find(playerId);
    return (it != _playerIdMap.end()) ? it->second : nullptr;
}

void GameState::attackUpdates(std::vector<NetworkController::AttackMessage> attacks) {
    for (NetworkController::AttackMessage attack : attacks) {
        _enemy->updateHealth(-1 * attack.damage);
    }
}

void GameState::healUpdates(std::vector<NetworkController::HealMessage> heals) {
    for (NetworkController::HealMessage heal : heals) {
        //find the player by ID and apply heal
    }
}

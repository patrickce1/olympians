#include <cugl/cugl.h>
#include "GameState.h"

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

    // --- Character loading ---
    const std::string characterJsonPath = "json/characters.json";
    if (!_characterLoader.loadFromFile(characterJsonPath)) {
        CULog("GameState: Failed to load characters.json");
        return false;
    }

    // --- Build the player array ---
    // Index 0: human-controlled Player.
    // Indices 1-3: AI-controlled EasyPlayerAI (inherits PlayerAI -> Player).
    // All stored as shared_ptr<Player> so EnemyController, neighbour links,
    // and any other system that iterates _players works uniformly.
    _players.reserve(4);

    auto humanPlayer = std::make_shared<Player>("Percy", 1, "Player 1", _characterLoader);
    _players.push_back(humanPlayer);

    // AI players — constructed here but not yet init()'d because
    // init() needs the ItemController database passed in below.
    for (int i = 1; i <= 3; i++) {
        auto aiPlayer = std::make_shared<EasyPlayerAI>(
            "Percy", i + 1,
            "Player " + std::to_string(i + 1),
            _characterLoader
        );
        _players.push_back(aiPlayer);
    }

    // --- Circular neighbour linking ---
    // Links all 4 players in a ring: 0 <-> 1 <-> 2 <-> 3 <-> 0
    const int playerCount = (int)_players.size();
    for (int i = 0; i < playerCount; i++) {
        int leftIdx  = (i - 1 + playerCount) % playerCount;
        int rightIdx = (i + 1) % playerCount;
        _players[i]->setLeftPlayer(_players[leftIdx].get());
        _players[i]->setRightPlayer(_players[rightIdx].get());
    }

    // --- Player ID map ---
    // Populate the network lookup map. Key == array index for now;
    // will be replaced with host-assigned IDs once networking is wired up.
    for (int i = 0; i < playerCount; i++) {
        _playerIdMap[i] = _players[i].get();
    }

    // Default local player to index 0 (single-player / pre-network default).
    // setLocalPlayer() will be called again with the real slot after lobby.
    setLocalPlayer(0);

    // --- Enemy loading ---
    const std::string enemyJsonPath = "json/enemies.json";
    _enemy = std::make_shared<Enemy>();
    if (!_enemy->init("enemy1", enemyJsonPath)) {
        CULog("GameState: Failed to initialize enemy");
        return false;
    }
    CULog("GameState: Enemy initialized id='%s'", _enemy->getId().c_str());

    // --- AI player init ---
    // Finish initialising AI players now that the item database is available.
    const std::string aiConfigPath = "json/playerAI.json";
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

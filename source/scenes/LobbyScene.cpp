#include "LobbyScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Player Icon Blink Timer */
#define BLINK_TIMER  0.5f
/** Error display time for disconnect error */
#define ERROR_DISPLAY_TIME  2.0f

/**
 * Initializes the controller contents, and starts the game
 *
 * In previous labs, this method "started" the scene.  But in this
 * case, we only use to initialize the scene user interface.  We
 * do not activate the user interface yet, as an active user
 * interface will still receive input EVEN WHEN IT IS HIDDEN.
 *
 * That is why we have the method {@link #setActive}.
 *
 * @param assets             The (loaded) assets for this game mode
 * @param networkController  The network controller shared across all scenes
 * @param gameState          The state of the game
 * @param itemController     The item controller needed to init AI players
 *                           when assignMissingHousesForAI() runs at game start
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool LobbyScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
          const std::shared_ptr<NetworkController>& networkController,
          GameState* gameState,
          ItemController* itemController){
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    _gameState = gameState;
    
    // Start up the input handler
    _assets = assets;
    _network = networkController;

    Size dimen = getSize();
    
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("lobbyScene");
    
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD
    
    // Setup UI and listeners
    setupUI();
    setupListeners();
    
    _status = Status::IDLE;
    
    // Store item controller so assignMissingHousesForAI() can init AI
    // behavior when the host presses Begin Quest.
    _itemController = itemController;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the lobby UI elements.
 *
 * This method looks up important UI components from the scene graph,
 * including the start button, back button, and game ID label, and stores
 * them for later interaction.
 */
void LobbyScene::setupUI() {
    _enterGame = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("lobbyScene.start"));

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("lobbyScene.back"));

    _gameId = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("lobbyScene.header.gameID"));

    _bossImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(_assets->get<scene2::SceneNode>("lobbyScene.tableArea.bossCircle.bossLobbyButton.bossLobbyImage"));
    
    _bossLobbyButton = std::dynamic_pointer_cast<cugl::scene2::Button>(_assets->get<scene2::SceneNode>("lobbyScene.tableArea.bossCircle.bossLobbyButton"));
    
    _playerInfoContainer = _assets->get<scene2::SceneNode>("lobbyScene.tableArea");

    if (_playerInfoContainer) {
        for (int i = 0; i <= 3; i++) {
            std::string cardName = "playerCard" + std::to_string(i);
            auto card = _playerInfoContainer->getChildByName(cardName);
            auto label = std::dynamic_pointer_cast<scene2::Label>(
                card->getChildByName("username")
            );
            auto image = std::dynamic_pointer_cast<scene2::Button>(
                card->getChildByName("playerIcon")
            );

            _playerSlots.push_back(label);
            _playerImages.push_back(image);
        }
    }
    
    _localPlayerIconIndicator = _assets->get<scene2::SceneNode>("lobbyScene.tableArea.playerCard3.glowBorder");
    
    _errorPopup = _assets->get<scene2::SceneNode>("lobbyScene.errorPopup");
    if (_errorPopup) {
        auto overlay = std::dynamic_pointer_cast<scene2::PolygonNode>(
            _errorPopup->getChildByName("overlayBG"));
        if (overlay) {
            overlay->setContentSize(getSize());
            overlay->setAnchor(Vec2::ANCHOR_CENTER);
            overlay->setPosition(getSize() / 2);
        }
        _errorPopup->setVisible(false);
    }
}

/**
 * Attaches input listeners to the lobby UI buttons.
 *
 * This method assigns button callbacks that update the lobby scene status
 * when the user presses the start or back buttons.
 */
void LobbyScene::setupListeners() {
    _enterGame->addListener([this](const std::string& name, bool down) {
        if (!down || !_network->isHost()) return;
        
        // Assign unique houses to any AI slots that don't have one.
        // ItemController is needed to reinitialize AI behavior after
        // reconstructing slots as EasyPlayerAI with their new house.
        _gameState->assignMissingHousesForAI(*_itemController);

        // Broadcast each AI house to clients.
        const auto& players = _gameState->getPlayers();
        int totalSlots = (int)players.size();
        for (int i = 0; i < totalSlots; i++) {
            if (!_network->checkRealPlayer(i)) {
                const std::string& house = players[i]->getHouseName();
                if (!house.empty()) {
                    _network->broadcastAIHouseSelection(i, house);
                }
            }
        }

        //Confirm all players have house according to network.
        if (!_network->allPlayersSelectedHouse()) return;

        _status = Status::PRE_GAME_START;
    });

    _backButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            if (_network->isHost()) {
                _network->broadcastSessionTerminated();
                _pendingDisconnect = true;
            } else {
                _pendingDisconnect = true;
            }
            _status = Status::ABORT;
        }
    });

    _bossLobbyButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::BOSSSELECT;
        }
    });

    // Each display slot i corresponds to a game slot resolved via remapPlayersForDisplay().
    // Display slot 3 (last) is always the local player.
    for (int i = 0; i < (int)_playerImages.size(); i++) {
        _playerImages[i]->addListener([this, i](const std::string& name, bool down) {
            if (!down) return;

            // Resolve which game slot this display slot maps to
            int localIndex = _network->getLocalPlayerNumber();
            const auto& players = _gameState->getPlayers();
            int totalSlots = (int)players.size();

            // remapPlayersForDisplay walks (localIndex+1) % total ... (localIndex+totalSlots) % total
            // display slot i => game slot (localIndex + 1 + i) % totalSlots
            int gameSlot = (localIndex + 1 + i) % totalSlots;

            // Display slot 3 is the local player's own slot (i == _playerImages.size()-1)
            bool isLocalSlot = (i == (int)_playerImages.size() - 1);

            if (isLocalSlot) {
                // Always allow the local player to open their own house select
                _pendingSlotToBeOpened = -1;
                _status = Status::SELECT;
                return;
            }

            bool isReal = _network->checkRealPlayer(gameSlot);
            if (!isReal) {
                // AI slot — only the host may open it
                if (_network->isHost()) {
                    _pendingSlotToBeOpened = gameSlot;
                    _status = Status::SELECT;
                }
                // non-host: no-op, no feedback
            } else {
                // Another real player's slot — blocked for everyone
                // no-op, no feedback
            }
        });
    }
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void LobbyScene::dispose() {
    if (_active) {
        removeAllChildren();
        _playerSlots.clear();
        _playerImages.clear();
        _enterGame = nullptr;
        _backButton = nullptr;
        _gameId = nullptr;
        _bossImage = nullptr;
        _bossLobbyButton = nullptr;
        _playerInfoContainer = nullptr;
        _active = false;
    }
    _network = nullptr;
}

/**
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.  Buttons
 * should be activated when it is made active and deactivated when
 * it is not.
 *
 * @param value whether the scene is currently active
 */
void LobbyScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _currentBoss = "";
            _enterGame->deactivate();
            _backButton->activate();
            _bossLobbyButton->activate();
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->activate();
            }
            
            // Show a disconnect banner if one was queued by SceneLoader
            if (!_disconnectBanner.empty()) {
                showDisconnectBanner(_disconnectBanner);
                _disconnectBanner = "";
            }
        } else {
            if (_pendingDisconnect) {
                _network->disconnect();
                _pendingDisconnect = false;
            }
            _backButton->deactivate();
            _enterGame->deactivate();
            _bossLobbyButton->deactivate();
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->deactivate();
                icon->setDown(false);
            }
            
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backButton->setDown(false);
            _bossLobbyButton->setDown(false);
        }
    }
}

/**
 * Updates the username labels in the lobby UI to match the given player list.
 * The list is expected to already be in display order (local player last)
 * as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read names from.
 */
void LobbyScene::updateLobbyText(std::vector<Player*> players) {
    for (int i = 0; i < _playerSlots.size(); i++) {
        _playerSlots[i]->setText(players[i]->getPlayerName());
    }
}

/**
 * Updates the player icon images in the lobby UI based on each player's
 * selected house. The list is expected to already be in display order
 * (local player last) as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read house names from.
 */
void LobbyScene::updateLobbyPlayerIcons(std::vector<Player*> players) {
    for (int i = 0; i < _playerImages.size(); i++) {
        auto image = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
            _playerImages[i]->getChildByName("playerIconImg"));
        if (image) {
            std::string key = players[i]->getHouseName() + "SIcon";
            
            if (_assets->get<cugl::graphics::Texture>(key) != nullptr) {
                image->setTexture(_assets->get<cugl::graphics::Texture>(key));
            } else {
                image->setTexture(_assets->get<cugl::graphics::Texture>("emptySIcon"));
            }
        }
    }
}

/**
 * Remaps the full player list from GameState so the local player always
 * appears last (bottom slot of the UI). Walks the circular player array
 * starting one step to the right of the local player, so that left/right
 * neighbour relationships are preserved visually. Includes both real and
 * AI players since both are stored in GameState.
 *
 * This is purely a display remapping — no game or network state is changed.
 *
 * @return  A reordered list of raw Player pointers with the local player last.
 */
std::vector<Player*> LobbyScene::remapPlayersForDisplay() {
    int localIndex = _network->getLocalPlayerNumber();
    const auto& players = _gameState->getPlayers();
    int totalSlots = (int)players.size();

    std::vector<Player*> remappedPlayerSlots;
    remappedPlayerSlots.reserve(totalSlots);

    for (int i = 1; i < totalSlots + 1; i++) {
        int slot = (localIndex + i) % totalSlots;
        remappedPlayerSlots.push_back(players[slot].get());
    }

    return remappedPlayerSlots;
}

/**
 * Syncs the game state player names and houses with the current
 * networked player list. Called every frame during the lobby so that
 * _gameState reflects the latest connected player info before the
 * game scene activates.
 *
 * Uses checkRealPlayer() per slot rather than assuming real players
 * occupy the first N slots, since players can swap positions.
 * AI slots always use demoteToAI() to preserve isAI() == true —
 * setRealPlayer() reconstructs as a plain Player which would break
 * assignMissingHousesForAI() and AI behavior in GameScene.
 */
void LobbyScene::updateNetworkOrder() {
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    const auto& networkedPlayers = _network->getNetworkedPlayers();
    const int totalSlots = (int)_gameState->getPlayers().size();

    for (int i = 0; i < totalSlots; i++) {
        if (_network->checkRealPlayer(i)) {
            // Real player slot — if it previously had an AI house, clear it first
            if (_network->isHost() && !_network->getAIHouse(i).empty()) {
                _gameState->setRealPlayer(i, networkedPlayers[i].username, "");
                _network->clearAIHouse(i);
            }
            _gameState->setRealPlayer(i, networkedPlayers[i].username, networkedPlayers[i].houseID);
        } else {
            // AI slot — always use demoteToAI() to preserve isAI() == true.
            // House is synced from the host's authoritative _aIHouses map,
            // which is kept in sync across all clients via LOBBY_UPDATE.
            _gameState->demoteToAI(i, _network->getAIHouse(i));
        }
    }
}

/**
 Updates the _selectedHouse variable if the local player has selected a house in the
 house select screen.
 */
void LobbyScene::updateLocalPlayerSelectedHouse() {
    const auto& networkedPlayers = _network->getNetworkedPlayers();
    
    // check if local player has selected house
    int localIndex = _network->getLocalPlayerNumber();

    if (localIndex < networkedPlayers.size()) {
        const auto& player = networkedPlayers[localIndex];
        _hasSelectedHouse = (!player.houseID.empty());
    } else {
        _hasSelectedHouse = false;
    }
}

/**
 * Updates the image of the boss circle based on the selected enemy.
 *
 * @param enemyID The identifier of the enemy whose background should be displayed.
 */
void LobbyScene::updateLobbyBossImage(std::string enemyID) {
    if (enemyID == "" && _currentBoss == "") {
        return;
    } else if (enemyID == _currentBoss) {
        return;
    }
    
    _currentBoss = enemyID;
    if (_currentBoss == "cyclops") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("cyclopsLobbyImage"));
    } else if (_currentBoss == "cerberus") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("cerberusLobbyImage"));
    } else if (_currentBoss == "circe") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("circeLobbyImage"));
    } else if (_currentBoss == "gaia") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("gaiaLobbyImage"));
    }
    _bossImage->setContentSize(228,228);
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void LobbyScene::update(float timestep) {
    // Disconnect Error Pop Up Logic
    if (_errorPopup && _errorPopup->isVisible()) {
        _errorTimer += timestep;
        if (_errorTimer >= ERROR_DISPLAY_TIME) {
            _errorPopup->setVisible(false);
            _errorTimer = 0.0f;
        }
    }
    
    //get the room once we are fully connected
    if (_network->checkConnection() == NetworkController::Status::CONNECTED) {
        std::string roomNum = _network->getRoom();
        _gameId->setText(roomNum);
        
        // Invalid room — server connected but room doesn't exist
        if (roomNum == "#####" || roomNum == "nullstr") {
            _network->disconnect();
            _status = Status::ABORT;
            return;
        }
        
        _network->broadcastJoinedLobby();
    }
    else {
        _gameId->setText("#####");
    }

    _network->getNetworkUpdates();
    if (!_network->isHost()) {
        
        // The host is in preGameScene
        if (_network->getHostsCurrentScene() == 0) {
            _status = Status::PRE_GAME_START;
        }
        
        // The host is in GameScene
        if (_network->getHostsCurrentScene() == 1) {
            _status = Status::GAME_START;
        }
        
        // Host voluntarily left — they broadcast SESSION_TERMINATED before disconnecting.
        if (_network->wasSessionTerminated()) {
            _network->clearQueues();
            _network->disconnect();
            _status = Status::HOST_LEFT;
            return;
        }
        
        // Host dropped unexpectedly — no broadcast, detected via the disconnect callback.
        if (_network->wasHostDisconnected()) {
            _network->clearQueues();
            _network->disconnect();
            _status = Status::HOST_DISCONNECTED;
            return;
        }
    }
    
    // change boss icon to the currently chosen boss
    updateLobbyBossImage(_network->getEnemy());
    
    // Only the host can start; only enable the button when all players have locked in a house.
    if (_network->isHost()) {
            _enterGame->activate();
    } else {
        _enterGame->deactivate();
    }
    
    // Remap for display only — network order is unchanged
    updateNetworkOrder();
    std::vector<Player*> displayOrder = remapPlayersForDisplay();
    
    updateLocalPlayerSelectedHouse();
    
    updateLobbyText(displayOrder);
    updateLobbyPlayerIcons(displayOrder);
    _network->clearQueues();
    
    if (!_hasSelectedHouse) {
        _blinkTimer += timestep;

        if (_blinkTimer >= BLINK_TIMER) {
            _blinkTimer = 0.0f;
            _blinkOn = !_blinkOn;
            if(_blinkOn){
                _localPlayerIconIndicator->setColor(Color4(255,255,255,255));
            } else {
                _localPlayerIconIndicator->setColor(Color4(255,255,255,150));
            }
        }
    } else {
        _localPlayerIconIndicator->setVisible(true);
        _localPlayerIconIndicator->setColor(Color4(255,255,255,255));
    }
}

/**
 * Shows a temporary disconnect notification using the error popup node.
 * Auto-dismisses after ERROR_DISPLAY_TIME seconds via the existing
 * _errorTimer mechanism in update().
 *
 *@param message  The "[Name] disconnected" string to display.
 */
void LobbyScene::showDisconnectBanner(const std::string& message) {
    if (!_errorPopup) return;
    auto label = std::dynamic_pointer_cast<scene2::Label>(
        _errorPopup->getChildByName("errorLabel"));
    if (label) label->setText(message);
    _errorPopup->setVisible(true);
    _errorTimer = 0.0f;
}

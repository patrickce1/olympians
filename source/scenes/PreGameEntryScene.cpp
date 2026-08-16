#include "PreGameEntryScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** How long (seconds) to show the error popup before auto-dismissing */
#define ERROR_DISPLAY_TIME  2.0f

/**
 * Initializes the scene contents, and starts the scene
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
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool PreGameEntryScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
          const std::shared_ptr<NetworkController>& networkController,GameState* gameState) {
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
    
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("preGameEntryScene");
    
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD
    
    // Setup UI
    setupUI();
    
    _status = Status::IDLE;
    
    _timeline = ActionTimeline::alloc();
    
    _errorTimer = 0.0f;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the pre game entry scene UI elements.
 *
 * This method looks up important UI components from the scene graph,
 * including the loading bar, player labels, and house tiles, and storing
 * them for later interaction.
 */
void PreGameEntryScene::setupUI() {
    _loadingBar = std::dynamic_pointer_cast<scene2::ProgressBar>(
        _assets->get<scene2::SceneNode>("preGameEntryScene.loadingBar.preGameBarFill"));
    
    _topClouds = _assets->get<scene2::SceneNode>("preGameEntryScene.preEntryCloud");
    if (_topClouds) _topCloudPos = _topClouds->getPosition();
    
    _bottomClouds = _assets->get<scene2::SceneNode>("preGameEntryScene.preEntryBottomCloud");
    if (_bottomClouds) _bottomCloudPos = _bottomClouds->getPosition();
    
    auto bottomSection = _assets->get<scene2::SceneNode>("preGameEntryScene.bottomSection");
    auto topSection = _assets->get<scene2::SceneNode>("preGameEntryScene.topSection");
    
    if (bottomSection && topSection) {
        auto extractFromSection = [&](const std::shared_ptr<scene2::SceneNode>& tileBlock) {
            auto tile = std::dynamic_pointer_cast<scene2::PolygonNode>(tileBlock->getChildByName("emptyBox"));
            if (tile) {
                _playerTiles.push_back(tile);
            }
            auto labels = tileBlock->getChildByName("labels");
            if (labels) {
                auto houseName = std::dynamic_pointer_cast<scene2::Label>((labels->getChildByName("houseName")->getChildByName("label")));
                auto playerName = std::dynamic_pointer_cast<scene2::Label>((labels->getChildByName("playerName")->getChildByName("label")));
                
                if (houseName && playerName) {
                    _houseNames.push_back(houseName);
                    _playerNames.push_back(playerName);
                }
            }
        };

        extractFromSection(bottomSection->getChildByName("rightPlayerTile"));
        extractFromSection(topSection->getChildByName("topPlayerTile"));
        extractFromSection(topSection->getChildByName("leftPlayerTile"));
        extractFromSection(bottomSection->getChildByName("localPlayerTile"));
    }
    
    // Error popup node
    _errorPopup = _assets->get<scene2::SceneNode>("preGameEntryScene.errorPopup");
    if (_errorPopup) {
        _errorPopup->setVisible(false);
    }
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void PreGameEntryScene::dispose() {
    if (_active) {
        removeAllChildren();
        _playerNames.clear();
        _playerTiles.clear();
        _houseNames.clear();
        _topClouds = nullptr;
        _bottomClouds = nullptr;
        _loadingBar = nullptr;
        _timeline = nullptr;
        _active = false;
        _errorPopup = nullptr;
    }
    _network = nullptr;
}

/**
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.
 *
 * @param value whether the scene is currently active
 */
void PreGameEntryScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _loadingProgress = 0.0f;
            _loadingTarget = 0.0f;
            _loadingComplete = false;
            _disconnectMessage = "";

            if (_loadingBar) {
                _loadingBar->setProgress(0.0f);
            }
            
            if (_topClouds) {
                _topClouds->setPosition(_topCloudPos + Vec2(0, 300)); // above screen
            }

            if (_bottomClouds) {
                _bottomClouds->setPosition(_bottomCloudPos - Vec2(0, 300)); // below screen
            }

            animateCloudsIn();
            
            _errorTimer = 0.0f;
            if (_errorPopup) _errorPopup->setVisible(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void PreGameEntryScene::update(float timestep) {
    if (!_active || !_loadingBar) return;
    
    if (_network->isHost()) {
        _network->broadcastHostsCurrentScene(0);
    }
    _network->getNetworkUpdates();
    
    // If host has returned to lobby, follow immediately.
    // This catches the case where _disconnectedSlots was cleared before
    // this client's PreGameEntryScene could detect the disconnect itself.
    if (!_network->isHost() && _network->getHostsCurrentScene() == 2) {
        _status = Status::PLAYER_DISCONNECTED;
        return;
    }
    
    // Client disconnect
    updateNetworkOrder();
    if (_status == Status::PLAYER_DISCONNECTED) return;
    
    // Host disconnect
    if (!_network->isHost()) {
        if (_network->wasHostDisconnected()) {
            _network->clearQueues();
            _network->disconnect();
            _status = Status::HOST_DISCONNECTED;
            return;
        }
    }


    // Ease the displayed bar toward the real progress reported by GameScene.
    if (!_timeline->isActive("bottom_clouds") && _status != Status::ERROR_DISPLAY){
        _loadingProgress += (_loadingTarget - _loadingProgress) * std::min(1.0f, timestep * 8.0f);
        _loadingBar->setProgress(_loadingProgress);
    }

    // Do not transition until both the real work and the displayed bar finish.
    if (_loadingComplete && _loadingProgress >= 0.995f) {
        _loadingProgress = 1.0f;
        _loadingBar->setProgress(1.0f);
        _status = Status::START;
    }
    
    std::vector<Player*> displayOrder = remapPlayersForDisplay();
    updateEntryScreenTiles(displayOrder);
    updateEntryScreenText(displayOrder);
    
    _timeline->update(timestep);
    
    if (_status == Status::ERROR_DISPLAY) {
        _errorTimer += timestep;
        if (_errorTimer >= ERROR_DISPLAY_TIME) {
            dismissError();
        }
    }
}

void PreGameEntryScene::setLoadingProgress(float progress) {
    _loadingTarget = std::clamp(progress, 0.0f, 1.0f);
    _loadingComplete = (_loadingTarget >= 1.0f);
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
std::vector<Player*> PreGameEntryScene::remapPlayersForDisplay() {
    int localIndex = _network->getLocalPlayerNumber();
    const auto& players = _gameState->getPlayers();
    int totalTiles = (int)players.size();

    std::vector<Player*> remappedPlayerTiles;
    remappedPlayerTiles.reserve(totalTiles);

    for (int i = 1; i < totalTiles + 1; i++) {
        int slot = (localIndex + i) % totalTiles;
        remappedPlayerTiles.push_back(players[slot].get());
    }

    return remappedPlayerTiles;
};

/**
 * Updates the player tile images in the pre game entry UI based on each player's
 * selected house. The list is expected to already be in display order
 * (local player last) as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read house names from.
 */
void PreGameEntryScene::updateEntryScreenTiles(std::vector<Player*> players) {
    for (int i = 0; i < _playerTiles.size(); i++) {
        auto tile = _playerTiles[i];
        if (tile) {
            std::string key = players[i]->getHouseName() + "Box";
            
            if (_assets->get<cugl::graphics::Texture>(key) != nullptr) {
                tile->setTexture(_assets->get<cugl::graphics::Texture>(key));
            } else {
                tile->setTexture(_assets->get<cugl::graphics::Texture>("emptyBox"));
            }
            tile->setScale(0.5f);
        }
    }
}

/**
 * Updates the username and house labels in the pre game entry UI to match the given player list.
 * The list is expected to already be in display order (local player last)
 * as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read names from.
 */
void PreGameEntryScene::updateEntryScreenText(std::vector<Player*> players) {
    for (int i = 0; i < _playerNames.size(); i++) {
        _playerNames[i]->setText(players[i]->getPlayerName());
        
        std::string name = players[i]->getHouseName();
        for (char &c : name) c = toupper(c);
        _houseNames[i]->setText(name);
    }
}

/**
 * Animates the entry clouds from off-screen positions into their final
 * layout positions using the scene's ActionTimeline system.
 *
 * Both the top and bottom cloud layers are first positioned outside
 * the visible screen bounds in `setActive()`, then smoothly transitioned
 * into their target positions using easing-based MoveTo actions.
 */
void PreGameEntryScene::animateCloudsIn() {
    if (_topClouds) {
        auto moveTop = cugl::scene2::MoveTo::alloc(_topCloudPos);
        auto easing = EasingFactory::alloc(EasingFactory::Type::CUBIC_OUT);

        _timeline->add("top_clouds", moveTop->attach(_topClouds), 1.2f, easing);
    }

    if (_bottomClouds) {
        auto moveBottom = cugl::scene2::MoveTo::alloc(_bottomCloudPos);
        auto easing = EasingFactory::alloc(EasingFactory::Type::CUBIC_OUT);

        _timeline->add("bottom_clouds", moveBottom->attach(_bottomClouds), 1.2f, easing);
    }
}

/**
 * Displays the error popup with the given message and switches to
 * Status::ERROR_DISPLAY so update() can auto-dismiss it.
 *
 * If no "clientScene.errorPopup" node was found during setupUI(), the
 * message is printed to the console and the scene resets immediately.
 *
 * @param message  Human-readable error text to show.
 */
void PreGameEntryScene::showError(const std::string& message) {
    if (_errorPopup) {
        auto label = std::dynamic_pointer_cast<scene2::Label>(
            _errorPopup->getChildByName("errorLabel"));
        if (label) {
            label->setText(message);
        }
        _errorPopup->setVisible(true);
    }

    _errorTimer = 0.0f;
    _status = Status::ERROR_DISPLAY;
}

/**
 * Hides the error popup and returns the scene to IDLE so the player can
 * correct their input and try again.
 */
void PreGameEntryScene::dismissError() {
    if (_errorPopup) {
        _errorPopup->setVisible(false);
    }
    _status = Status::ABORT;
}

/**
 * Syncs the latest network state into GameState and detects player disconnects.
 * For each slot: if still a real player, updates their username and house; if
 * it was a real player but is no longer connected, stores their name in
 * _disconnectMessage, sets status to PLAYER_DISCONNECTED, and returns early so
 * SceneLoader can route everyone back to the lobby; if it was always an AI,
 * updates its house assignment from the network's authoritative AI house map.
 *
 * Called every frame so that clients who arrived from HouseSelectScene or
 * BossSelectScene (which do not run this sync) are caught up before
 * GameScene starts.
 */
void PreGameEntryScene::updateNetworkOrder() {
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    const auto& players = _gameState->getPlayers();
    const auto& slotToPlayer = _network->getNetworkedPlayers();
    const auto& disconnectedSlots = _network->getDisconnectedSlots();
    const int totalSlots = (int)players.size();

    // Check disconnected slots first — read name from GameState before
    // any demoteToAI call can overwrite it.
    for (int slot : disconnectedSlots) {
        if (slot < 0 || slot >= totalSlots) continue;
        _disconnectMessage = players[slot]->getPlayerName() + " disconnected";
        _status = Status::PLAYER_DISCONNECTED;
        return;
    }

    for (int i = 0; i < totalSlots; i++) {
        auto pair = slotToPlayer.find(i);
        if (pair != slotToPlayer.end()) {
            _gameState->setRealPlayer(i, pair->second.username, pair->second.houseID);
        } else {
            _gameState->demoteToAI(i, _network->getAIHouse(i));
        }
    }
}

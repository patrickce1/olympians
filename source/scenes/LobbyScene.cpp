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
/** How much larger the dragged player card appears while being held. */
constexpr float LOBBY_DRAG_PICKUP_SCALE = 1.12f;
/** Number of frames a press must be held before it is treated as a drag. */
constexpr int LOBBY_DRAG_HOLD_FRAMES = 8;

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
 * @param audio    The audio controller used for various sounds.
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool LobbyScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
          const std::shared_ptr<NetworkController>& networkController,
          GameState* gameState,
          ItemController* itemController,
          AudioController* audio){
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    _gameState = gameState;
    _audio = audio;

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
    
    _itemsButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("lobbyScene.itemsTab"));

    _gameId = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("lobbyScene.header.gameID"));

    _bossImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(_assets->get<scene2::SceneNode>("lobbyScene.tableArea.bossCircle.bossLobbyButton.bossLobbyImage"));
    
    _bossLobbyButton = std::dynamic_pointer_cast<cugl::scene2::Button>(_assets->get<scene2::SceneNode>("lobbyScene.tableArea.bossCircle.bossLobbyButton"));
    
    _lobbyDescriptionLabel = _assets->get<scene2::SceneNode>("lobbyScene.lobbyBottomLabel");
    
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
            image->getChildByName("playerIconImg")->setScale(0.5f);

            _playerCards.push_back(card);
            _playerSlots.push_back(label);
            _playerImages.push_back(image);
        }
    }
    
    _localPlayerIconIndicator = _assets->get<scene2::SceneNode>("lobbyScene.tableArea.playerCard3.glowBorder");
    
    _errorPopup = _assets->get<scene2::SceneNode>("lobbyScene.errorPopup");
    if (_errorPopup) {
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
    ButtonHelpers::addTapListener(_enterGame, [this] {
        if (!_network->isHost()) return;

        // Assign unique houses to any AI slots that don't have one.
        // ItemController is needed to reinitialize AI behavior after
        // reconstructing slots as PlayerAI with their new house.
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

        // Confirm all players have house according to network.
        if (!_network->allPlayersSelectedHouse()) return;

        if (_audio) _audio->playSoundUnique("start_sound");

        _status = Status::PRE_GAME_START;
    });

    ButtonHelpers::addTapListener(_backButton, [this] {
        if (_audio) _audio->playSoundUnique("page_turn");
        if (_network->isHost()) {
            _network->broadcastSessionTerminated();
            _pendingDisconnect = true;
        } else {
            _pendingDisconnect = true;
        }
        _status = Status::ABORT;
    });

    ButtonHelpers::addTapListener(_bossLobbyButton, [this] {
        if (_audio) _audio->playSoundUnique("small_click");
        _status = Status::BOSSSELECT;
    });
    
    for (int i =0; i < 4; i++){
        ButtonHelpers::addTapListener(_playerImages[i], [this] {
            if (_audio && _network->isHost()) _audio->playSoundUnique("small_click");
        });
    }

    
    // Wire the local slot (index 3) for all players.
    // For non-hosts this is the only interaction they have.
    // For the host, the press system handles everything including this slot,
    // so the listener is a no-op for hosts to avoid double-firing.
//    _playerImages[3]->addListener([this](const std::string& name, bool down) {
//        if (!down) return;
//        if (_network->isHost()) return; // host handled entirely by press system
//        _pendingSlotToBeOpened = -1;
//        _status = Status::SELECT;
//    });

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
        _itemsButton = nullptr;
        _lobbyDescriptionLabel = nullptr;
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
            _prevPlayerCount = (int)_network->getNetworkedPlayers().size();
            _enterGame->deactivate();
            _backButton->activate();
            _bossLobbyButton->activate();
            _itemsButton->activate();
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->activate();
            }
            
            // Show a disconnect banner if one was queued by SceneLoader
            if (!_disconnectBanner.empty()) {
                showDisconnectBanner(_disconnectBanner);
                _disconnectBanner = "";
            }
            CULog("[LobbyScene] Cached player XP: %d", SavedDataManager::get().getPlayerXP());
        } else {
            if (_pendingDisconnect) {
                _network->disconnect();
                _pendingDisconnect = false;
            }
            _backButton->deactivate();
            _enterGame->deactivate();
            _bossLobbyButton->deactivate();
            _itemsButton->deactivate();
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
            image->setScale(0.5f);
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

    const auto& slotToPlayer = _network->getNetworkedPlayers();
    const auto& disconnectedSlots = _network->getDisconnectedSlots();
    const int totalSlots = (int)_gameState->getPlayers().size();

    for (int i = 0; i < totalSlots; i++) {
        auto pair = slotToPlayer.find(i);
        if (pair != slotToPlayer.end()) {
            // Real player slot — check if they just disconnected
            if (std::find(disconnectedSlots.begin(), disconnectedSlots.end(), i) != disconnectedSlots.end()) {
                _gameState->demoteToAI(i, _network->getAIHouse(i));
            } else {
                _gameState->setRealPlayer(i, pair->second.username, pair->second.houseID);
            }
        } else {
            // AI slot — sync house assignment
            _gameState->demoteToAI(i, _network->getAIHouse(i));
        }
    }
    int currentCount = (int)slotToPlayer.size();
    if (currentCount > _prevPlayerCount) {
        if (_audio) _audio->playSoundUnique("lobby_join");
    }
    _prevPlayerCount = currentCount;
}

/**
 Updates the _selectedHouse variable if the local player has selected a house in the
 house select screen.
 */
void LobbyScene::updateLocalPlayerSelectedHouse() {
    const auto& slotToPlayer = _network->getNetworkedPlayers();
    
    int localIndex = _network->getLocalPlayerNumber();

    auto pair = slotToPlayer.find(localIndex);
    if (pair != slotToPlayer.end()) {
        _hasSelectedHouse = !pair->second.houseID.empty();
    } else {
        _hasSelectedHouse = false;
    }
}

/**
 * Updates the image of the boss circle based on the selected enemy.
 *
 * @param enemyID The identifier of the enemy whose background should be displayed.
 */
void LobbyScene::updateLobbyBoss(std::string enemyID) {
    if (enemyID == "" && _currentBoss == "") {
        return;
    } else if (enemyID == _currentBoss) {
        return;
    }
    
    auto nameLabel = std::dynamic_pointer_cast<scene2::Label>(
        _lobbyDescriptionLabel->getChildByName("bossName")
    );
    auto descriptionLabel = std::dynamic_pointer_cast<scene2::Label>(
        _lobbyDescriptionLabel->getChildByName("description")
    );
    
    _currentBoss = enemyID;
    
    std::string name = _currentBoss;
    for (char &character : name) character = toupper(character);
    nameLabel->setText(name);
    
    if (_currentBoss == "cyclops") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("cyclopsLobbyImage"));
        descriptionLabel->setText("The lone guardian of the cave, blinded by rage and hunger. Defeat it to escape its domain.");
    } else if (_currentBoss == "cerberus") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("cerberusLobbyImage"));
        descriptionLabel->setText("Hades’ companion gone rogue, defeat it and bring him back to Hell. Attacks.....Defenses....");
    } else if (_currentBoss == "circe") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("circeLobbyImage"));
        descriptionLabel->setText("The cunning enchantress who tests your resolve. Defeat her to break her spell.");
    } else if (_currentBoss == "gaia") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("gaiaLobbyImage"));
        descriptionLabel->setText("The primordial force of the earth. Defeat her to overcome nature itself.");
    }
    _bossImage->setContentSize(228,228);
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 * @param input         The input controller instance
 */
void LobbyScene::update(float timestep, InputController& input) {
    // Disconnect Error Pop Up Logic
    if (_errorPopup && _errorPopup->isVisible()) {
        _errorTimer += timestep;
        if (_errorTimer >= ERROR_DISPLAY_TIME) {
            _errorPopup->setVisible(false);
            _errorTimer = 0.0f;
        }
    }
    
    //Host is in lobbyScene
    if (_network->isHost()) {
        _network->broadcastHostsCurrentScene(2);
    }
    
    if (_network->getEnemy() == "circe" && _network->isHost() && (!SavedDataManager::get().getTutorialCompleted() || _forceTutorial)) {
        _network->setLocalHouse("ares");
        _forceTutorial = false;

        // Sync the house to GameState before starting the game so AI doesn't pick Athena
        int localIndex = _network->getLocalPlayerNumber();
        Player* localPlayer = _gameState->getPlayerBySlot(localIndex);
        if (localPlayer) {
            _gameState->setRealPlayer(localIndex, localPlayer->getPlayerName(), "ares");
        }

        //Start game and set the bots' houses
        _enterGame->setDown(true);
        _enterGame->setDown(false);
        return;
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
    updateLobbyBoss(_network->getEnemy());
    
    // Only the host can start; only enable the button when all players have locked in a house.
    if (_network->isHost()) {
        _enterGame->activate();
        _enterGame->setVisible(true);
        _lobbyDescriptionLabel->setVisible(false);
    } else {
        _enterGame->deactivate();
        _enterGame->setVisible(false);
        _lobbyDescriptionLabel->setVisible(true);
    }
    
    // Press logic
    handleLobbySlotPressBegin(input);
    handleLobbySlotPressTracking(input);
    handleLobbySlotPressRelease(input);
    
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
 * Enables or disables all interactive input controls.
 *
 * Called with false when a join attempt starts so the player cannot spam
 * the button, and called with true when the scene resets to IDLE.
 *
 * @param enabled  Whether the controls should accept input.
 */
void LobbyScene::setInputEnabled(bool enabled) {
    if (enabled) {
        _backButton->activate();
        _bossLobbyButton->activate();
        _itemsButton->activate();
        for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
            icon->activate();
        }
    } else {
        _backButton->deactivate();
        _bossLobbyButton->deactivate();
        _itemsButton->deactivate();
        for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
            icon->deactivate();
        }
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
    if (_audio) _audio->playSoundUnique("client_error");
    _errorTimer = 0.0f;
}

/**
 * HOST ONLY. Handles the beginning of a touch on a non-local player slot.
 * On the first frame of contact, records which slot is being pressed and
 * captures offset data for potential drag use. Increments _dragHoldFrames
 * each subsequent frame while the touch is held. Once _dragHoldFrames
 * reaches LOBBY_DRAG_HOLD_FRAMES, commits to drag mode by scaling up the
 * card and reparenting it to the top of _playerInfoContainer so it renders
 * above all other cards. Below that threshold the press is resolved as a
 * tap in handleLobbySlotPressRelease().
 * No-op if the host is touching their own local slot (bottom slot).
 *
 * @param input  The input controller for this frame.
 */
void LobbyScene::handleLobbySlotPressBegin(InputController& input) {
    if (!_network->isHost()) return;
    if (!input.isTouching() && !input.isMouseDown()) return;

    if (_dragSourceDisplaySlot == -1) {
        // Hit-test against playerCard bounds in tableArea's local space.
        // _playerCards[i]->getBoundingBox() returns bounds in tableArea space.
        // worldToNodeCoords on tableArea converts the world-space touch to the same space.
        Vec2 worldPos      = screenToWorldCoords(input.getTouchStart());
        Vec2 containerLocal = _playerInfoContainer->worldToNodeCoords(worldPos);
        for (int i = 0; i < (int)_playerCards.size(); i++) {
            cugl::Rect cardBounds = _playerCards[i]->getBoundingBox();
            CULog("[PressBegin] slot %d cardBounds origin=(%.1f,%.1f) size=(%.1f,%.1f)",
                  i, cardBounds.origin.x, cardBounds.origin.y,
                  cardBounds.size.width, cardBounds.size.height);

            if (!cardBounds.contains(containerLocal)) continue;

            _dragSourceDisplaySlot = i;
            _draggedCard           = _playerImages[i];
            _dragCardOriginPos     = _playerCards[i]->getPosition(); // playerCard pos in tableArea space
            Vec2 cardContainerLocal = containerLocal; // already in tableArea space
            _dragCardOffset         = _playerCards[i]->getPosition() - cardContainerLocal;
            _dragHoldFrames         = 0;
            return;
        }

        CULog("[PressBegin] touch did not hit any slot");
        return;
    }

    // Subsequent frames — increment hold counter
    _dragHoldFrames++;
    CULog("[PressBegin] holding slot %d, frame %d / %d",
          _dragSourceDisplaySlot, _dragHoldFrames, LOBBY_DRAG_HOLD_FRAMES);

    if (_dragHoldFrames == LOBBY_DRAG_HOLD_FRAMES) {
        CULog("[PressBegin] threshold reached — committing to drag mode");
        _draggedCard->setScale(LOBBY_DRAG_PICKUP_SCALE);
        // Reparent the playerCard node (parent of playerIcon), not playerIcon itself.
        // _draggedCard's parent is playerCard; playerCard's parent is _playerInfoContainer.
        auto cardNode = _playerCards[_dragSourceDisplaySlot];
        if (_playerInfoContainer) {
            _playerInfoContainer->removeChild(cardNode);
            _playerInfoContainer->addChild(cardNode);
        }
    }
}

/**
 * HOST ONLY. Moves the pressed player card to follow the current touch
 * position each frame once the hold threshold has been reached and the
 * interaction is committed as a drag. No-op during the tap-detection
 * window (_dragHoldFrames < LOBBY_DRAG_HOLD_FRAMES) so the card does
 * not move on a brief tap. Uses getDragPos() and the captured
 * _dragCardOffset so the card stays under the exact contact point.
 * No-op if no press is in progress or the touch has ended.
 *
 * @param input  The input controller for this frame.
 */
void LobbyScene::handleLobbySlotPressTracking(InputController& input) {
    if (!_draggedCard) return;
    if (_dragHoldFrames < LOBBY_DRAG_HOLD_FRAMES) return;
    if (!input.isTouching() && !input.isMouseDown()) return;

    Vec2 worldPos       = screenToWorldCoords(input.getDragPos());
    Vec2 containerLocal = _playerInfoContainer->worldToNodeCoords(worldPos);
    
    // Move the playerCard node, not the playerIcon inside it
    auto cardNode = _playerCards[_dragSourceDisplaySlot];
    cardNode->setPosition(containerLocal + _dragCardOffset);

    CULog("[PressTracking] dragging slot %d to containerLocal=(%.1f, %.1f)",
          _dragSourceDisplaySlot, containerLocal.x, containerLocal.y);
}

/**
 * HOST ONLY. Resolves a touch release as either a tap or a drag based
 * on _dragHoldFrames relative to LOBBY_DRAG_HOLD_FRAMES.
 *
 * Tap (below threshold): if the pressed slot is an AI slot, opens house
 * select for that slot. If it is a real player slot, does nothing.
 *
 * Drag (at or above threshold): hit-tests the release position against
 * all player card slots. A release on a different slot swaps the two game
 * slots via NetworkController and GameState. A release on the same slot or
 * dead space cancels the drag with no state change; the display
 * self-corrects on the next frame since it is fully recomputed from
 * GameState each frame.
 *
 * Always restores the card's position and scale and clears all press
 * state before returning to prevent a single-frame visual glitch.
 *
 * @param input  The input controller for this frame.
 */
void LobbyScene::handleLobbySlotPressRelease(InputController& input) {
    if (!input.touchEnded()) return;

    CULog("[PressRelease] touchEnded=true, _dragSourceDisplaySlot=%d, holdFrames=%d",
          _dragSourceDisplaySlot, _dragHoldFrames);

    if (_dragSourceDisplaySlot == -1) {
        CULog("[PressRelease] no slot was being tracked — ignoring");
        return;
    }

    int localIndex = _network->getLocalPlayerNumber();
    const auto& players = _gameState->getPlayers();
    int totalSlots = (int)players.size();

    if (_dragHoldFrames < LOBBY_DRAG_HOLD_FRAMES) {
        // --- Tap path ---
        bool isLocalSlot = (_dragSourceDisplaySlot == (int)_playerCards.size() - 1);

        if (isLocalSlot) {
            // Tapped own slot — open own house select
            CULog("[PressRelease] TAP on local slot — opening own house select");
            _pendingSlotToBeOpened = -1;
            _status = Status::SELECT;
        } else {
            int gameSlot = (localIndex + 1 + _dragSourceDisplaySlot) % totalSlots;
            bool isReal  = _network->checkRealPlayer(gameSlot);
            CULog("[PressRelease] TAP on display slot %d => game slot %d, isReal=%d",
                  _dragSourceDisplaySlot, gameSlot, isReal);
            if (!isReal) {
                CULog("[PressRelease] AI slot — opening house select for game slot %d", gameSlot);
                _pendingSlotToBeOpened = gameSlot;
                _status = Status::SELECT;
            } else {
                CULog("[PressRelease] real player slot — no-op");
            }
        }
    } else {
        // --- Drag path ---
        // Hit-test release against playerCard bounds in tableArea local space,
        // matching the same space used in handleLobbySlotPressBegin.
        Vec2 worldPos       = screenToWorldCoords(input.getReleasePosition());
        Vec2 containerLocal = _playerInfoContainer->worldToNodeCoords(worldPos);
        CULog("[PressRelease] DRAG released at containerLocal=(%.1f, %.1f)",
              containerLocal.x, containerLocal.y);

        int targetDisplaySlot = -1;
        for (int i = 0; i < (int)_playerCards.size(); i++) {
            // Skip the source slot — its card has moved so its bounds are unreliable
            if (i == _dragSourceDisplaySlot) continue;

            cugl::Rect cardBounds = _playerCards[i]->getBoundingBox();
            CULog("[PressRelease] checking slot %d cardBounds origin=(%.1f,%.1f) size=(%.1f,%.1f)",
                  i, cardBounds.origin.x, cardBounds.origin.y,
                  cardBounds.size.width, cardBounds.size.height);
            if (cardBounds.contains(containerLocal)) {
                targetDisplaySlot = i;
                break;
            }
        }

        bool isValidDrop = (targetDisplaySlot != -1);
        CULog("[PressRelease] targetDisplaySlot=%d isValidDrop=%d", targetDisplaySlot, isValidDrop);

        if (isValidDrop) {
            auto displayToGameSlot = [&](int displaySlot) -> int {
                if (displaySlot == (int)_playerCards.size() - 1) return localIndex;
                return (localIndex + 1 + displaySlot) % totalSlots;
            };
            int srcGameSlot = displayToGameSlot(_dragSourceDisplaySlot);
            int dstGameSlot = displayToGameSlot(targetDisplaySlot);
            CULog("[PressRelease] swapping game slots %d <-> %d", srcGameSlot, dstGameSlot);
            _network->swapSlots(srcGameSlot, dstGameSlot);
            _gameState->swapPlayers(srcGameSlot, dstGameSlot);
        } else {
            CULog("[PressRelease] invalid drop — no swap");
        }
    }

    // In the cleanup block at the bottom of handleLobbySlotPressRelease:
    if (_draggedCard) {
        // Restore the playerCard node's position, not just the icon inside it
        _playerCards[_dragSourceDisplaySlot]->setPosition(_dragCardOriginPos);
        _draggedCard->setScale(1.0f);
    }

    _draggedCard           = nullptr;
    _dragCardOffset        = cugl::Vec2::ZERO;
    _dragCardOriginPos     = cugl::Vec2::ZERO;
    _dragSourceDisplaySlot = -1;
    _dragHoldFrames        = 0;
    CULog("[PressRelease] press state cleared");
}

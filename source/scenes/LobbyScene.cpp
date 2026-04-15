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
/** Minimum pointer travel before a press is treated as a drag. */
#define LOBBY_DRAG_THRESHOLD 12.0f

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
 * @param assets                             The (loaded) assets for this game mode
 * @param networkController     The network controller shared across all scenes
 * @param gameState                       The state of the game
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool LobbyScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
                     const std::shared_ptr<NetworkController>& networkController,
                     GameState* gameState){
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
    setupDragInput();
    
    _status = Status::IDLE;
    
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
            _playerCards.push_back(card);
            _playerCardHomePositions.push_back(card ? card->getPosition() : Vec2::ZERO);
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
}

/**
 * Attaches input listeners to the lobby UI buttons.
 *
 * This method assigns button callbacks that update the lobby scene status
 * when the user presses the start or back buttons.
 */
void LobbyScene::setupListeners() {
    _enterGame->addListener([this](const std::string& name, bool down) {
        if (down && _network->isHost() && _network->allPlayersSelectedHouse()) {
            _network->broadcastGameStart();
            _status = Status::START;
        }
    });

    _backButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            if (_network->isHost()) {
                _network->broadcastSessionTerminated();
                _pendingDisconnect = true;
            } else {
                // Client leaving — disconnect so host is notified via disconnect callback
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
    
    // Add listeners to all player icon buttons. Down arms drag on the exact
    // clickable icon region; release keeps the normal house-select behavior.
    for (int i = 0; i < (int)_playerImages.size(); i++) {
        auto icon = _playerImages[i];
        icon->addListener([this, i](const std::string& name, bool down) {
            const int lockedDisplayIndex = static_cast<int>(_playerImages.size()) - 1;
            if (down) {
                // Always reset drag suppression for a new press.
                _pointerDown = false;
                _isDraggingCard = false;
                _didDragCard = false;
                _pendingDragInit = false;
                _draggedCardIndex = -1;

                if (i == lockedDisplayIndex) {
                    return;
                }
                _draggedCardIndex = i;
                _pointerDown = true;
                _isDraggingCard = false;
                _didDragCard = false;
                _pendingDragInit = true;
                return;
            }

            if (!down && !_didDragCard) {
                _status = Status::SELECT;
            }
        });
    }
}

/**
 * Initializes touch and mouse listeners used for lobby drag-and-drop.
 */
void LobbyScene::setupDragInput() {
    _touch = Input::get<Touchscreen>();
    if (_touch) {
        _touchListenerKey = _touch->acquireKey();
        _touch->addBeginListener(_touchListenerKey, [this](const TouchEvent& event, bool focus) {
            if (!_active) return;
        });
        _touch->addMotionListener(_touchListenerKey, [this](const TouchEvent& event, const Vec2& prev, bool focus) {
            if (!_active) return;
            if (_pendingDragInit) {
                handlePointerDown(screenToWorldCoords(event.position));
            }
            handlePointerDrag(screenToWorldCoords(event.position));
        });
        _touch->addEndListener(_touchListenerKey, [this](const TouchEvent& event, bool focus) {
            if (!_active) return;
            handlePointerUp(screenToWorldCoords(event.position));
        });
    }

    _mouse = Input::get<Mouse>();
    if (_mouse) {
        _mouseListenerKey = _mouse->acquireKey();
        _mouse->setPointerAwareness(Mouse::PointerAwareness::DRAG);
        _mouse->addPressListener(_mouseListenerKey, [this](const MouseEvent& event, Uint8 clicks, bool focus) {
            if (!_active) return;
        });
        _mouse->addDragListener(_mouseListenerKey, [this](const MouseEvent& event, const Vec2& previous, bool focus) {
            if (!_active) return;
            if (_pendingDragInit) {
                handlePointerDown(screenToWorldCoords(event.position));
            }
            handlePointerDrag(screenToWorldCoords(event.position));
        });
        _mouse->addReleaseListener(_mouseListenerKey, [this](const MouseEvent& event, Uint8 clicks, bool focus) {
            if (!_active) return;
            handlePointerUp(screenToWorldCoords(event.position));
        });
    }
}

/**
 * Removes any touch/mouse listeners registered by setupDragInput().
 */
void LobbyScene::disposeDragInput() {
    if (_touch) {
        _touch->removeBeginListener(_touchListenerKey);
        _touch->removeMotionListener(_touchListenerKey);
        _touch->removeEndListener(_touchListenerKey);
        _touch = nullptr;
    }

    if (_mouse) {
        _mouse->removePressListener(_mouseListenerKey);
        _mouse->removeDragListener(_mouseListenerKey);
        _mouse->removeReleaseListener(_mouseListenerKey);
        _mouse = nullptr;
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
        _playerCards.clear();
        _playerCardHomePositions.clear();
        _enterGame = nullptr;
        _backButton = nullptr;
        _gameId = nullptr;
        _bossImage = nullptr;
        _bossLobbyButton = nullptr;
        _playerInfoContainer = nullptr;
        _active = false;
    }
    disposeDragInput();
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
            //Setup everyhing for interaction
            _status = IDLE;
            _pointerDown = false;
            _isDraggingCard = false;
            _draggedCardIndex = -1;
            _didDragCard = false;
            _pendingDragInit = false;
            _isSwapAnimating = false;
            _swapAnimElapsed = 0.0f;
            _swapAnimDisplayA = -1;
            _swapAnimDisplayB = -1;
            _pendingModelSwapA = -1;
            _pendingModelSwapB = -1;
            _isReturnAnimating = false;
            _returnAnimDisplayIndex = -1;
            _returnAnimElapsed = 0.0f;
            _sentJoinMessage = false;
            _currentBoss = "";
            _enterGame->deactivate();
            _backButton->activate();
            _bossLobbyButton->activate();
            for (std::shared_ptr<cugl::scene2::Label> label : _playerSlots) {
                if (label) {
                    label->setVisible(true);
                }
            }
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->activate();
            }
        } else {
            if (_pendingDisconnect) {
                _network->disconnect();
                _pendingDisconnect = false;
            }
            _backButton->deactivate();
            _enterGame->deactivate();
            _bossLobbyButton->deactivate();
            _pointerDown = false;
            _isDraggingCard = false;
            _draggedCardIndex = -1;
            _pendingDragInit = false;
            _isSwapAnimating = false;
            _swapAnimElapsed = 0.0f;
            _swapAnimDisplayA = -1;
            _swapAnimDisplayB = -1;
            _pendingModelSwapA = -1;
            _pendingModelSwapB = -1;
            _isReturnAnimating = false;
            _returnAnimDisplayIndex = -1;
            _returnAnimElapsed = 0.0f;
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->deactivate();
                icon->setDown(false);
            }
            for (std::shared_ptr<cugl::scene2::Label> label : _playerSlots) {
                if (label) {
                    label->setVisible(true);
                }
            }
            
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backButton->setDown(false);
            _bossLobbyButton->setDown(false);

            for (int i = 0; i < (int)_playerCards.size() && i < (int)_playerCardHomePositions.size(); i++) {
                if (_playerCards[i]) {
                    _playerCards[i]->setPosition(_playerCardHomePositions[i]);
                }
            }
        }
    }
}

/**
 * Starts a potential drag if the pointer pressed on a player card.
 *
 * @param scenePos  Pointer location in scene coordinates.
 */
void LobbyScene::handlePointerDown(const cugl::Vec2& scenePos) {
    if (_isSwapAnimating || _isReturnAnimating) {
        _pointerDown = false;
        _pendingDragInit = false;
        return;
    }

    if (_draggedCardIndex < 0 || _draggedCardIndex >= (int)_playerCards.size()) {
        _pointerDown = false;
        _pendingDragInit = false;
        return;
    }

    _pointerStartPos = scenePos;
    _pendingDragInit = false;

    if (_draggedCardIndex >= 0 && _draggedCardIndex < (int)_playerCards.size() && _playerCards[_draggedCardIndex]) {
        Vec2 localPos = _playerInfoContainer->worldToNodeCoords(scenePos);
        _dragOffset = _playerCards[_draggedCardIndex]->getPosition() - localPos;
    }
}

/**
 * Updates the currently dragged card position.
 *
 * @param scenePos  Pointer location in scene coordinates.
 */
void LobbyScene::handlePointerDrag(const cugl::Vec2& scenePos) {
    if (_isSwapAnimating || _isReturnAnimating) {
        return;
    }

    if (!_pointerDown || _draggedCardIndex < 0 || _draggedCardIndex >= (int)_playerCards.size()) {
        return;
    }

    auto card = _playerCards[_draggedCardIndex];
    if (!card) {
        return;
    }
    Vec2 localPos = _playerInfoContainer->worldToNodeCoords(scenePos);


    if (!_isDraggingCard && localPos.distance(_pointerStartPos) >= LOBBY_DRAG_THRESHOLD) {
        _isDraggingCard = true;
        _didDragCard = true;
    }

    if (_isDraggingCard) {
        card->setPosition(localPos + _dragOffset);
    }
}

/**
 * Ends drag handling and performs a slot swap if dropped over another card.
 *
 * @param scenePos  Pointer location in scene coordinates.
 */
void LobbyScene::handlePointerUp(const cugl::Vec2& scenePos) {
    if (_isSwapAnimating || _isReturnAnimating) {
        _pointerDown = false;
        return;
    }

    if (!_pointerDown) {
        return;
    }

    _pointerDown = false;
    bool startedSwapAnim = false;

    if (_draggedCardIndex >= 0 && _draggedCardIndex < (int)_playerCards.size()) {
        if (_isDraggingCard) {
            int targetIndex = findCardAt(scenePos, _draggedCardIndex);
            const int lockedDisplayIndex = static_cast<int>(_playerCards.size()) - 1;
            if (targetIndex >= 0 && targetIndex != _draggedCardIndex && targetIndex != lockedDisplayIndex) {
                swapPlayersByDisplayIndex(_draggedCardIndex, targetIndex);
                startedSwapAnim = _isSwapAnimating;
            }
        }

        if (!startedSwapAnim && _draggedCardIndex < (int)_playerCardHomePositions.size() && _playerCards[_draggedCardIndex]) {
            beginReturnAnimation(_draggedCardIndex);
        }
    }

    _isDraggingCard = false;
    _draggedCardIndex = -1;
}

/**
 * Returns the card index at a scene position. (What is underneath the pointer)
 *
 * @param scenePos  Pointer location in scene coordinates.
 * @param ignore    Card index to skip during hit-test.
 * @return          Card index, or -1 if no card is hit.
 */
int LobbyScene::findCardAt(const cugl::Vec2& scenePos, int ignore) const {
    if (!_playerInfoContainer) return -1;
    Vec2 localPos = _playerInfoContainer->worldToNodeCoords(scenePos);
    for (int i = 0; i < (int)_playerCards.size(); i++) {
        if (i == ignore) continue;
        if (_playerCards[i] && _playerCards[i]->getBoundingBox().contains(localPos)) {
            return i;
        }
    }
    return -1;
}

/**
 * Converts a display-slot index (0..N-1 in lobby UI order) to the
 * underlying model/network slot index.
 *
 * @param displayIndex  The lobby card index in display order.
 * @return              The backing model slot, or -1 if unavailable.
 */
int LobbyScene::displayIndexToModelIndex(int displayIndex) const {
    if (!_network || !_gameState) {
        return -1;
    }

    const int totalSlots = static_cast<int>(_gameState->getPlayers().size());
    const int localIndex = _network->getLocalPlayerNumber();
    if (totalSlots <= 0 || localIndex < 0 || displayIndex < 0 || displayIndex >= totalSlots) {
        return -1;
    }

    return (localIndex + displayIndex + 1) % totalSlots;
}

/**
 * Swaps two players selected by their display-slot indices.
 *
 * @param displayA  First lobby card index.
 * @param displayB  Second lobby card index.
 */
void LobbyScene::swapPlayersByDisplayIndex(int displayA, int displayB) {
    if (!_network || !_gameState || !_network->isHost()) {
        return;
    }

    const int lockedDisplayIndex = static_cast<int>(_playerCards.size()) - 1;
    if (displayA == lockedDisplayIndex || displayB == lockedDisplayIndex) {
        return;
    }

    const int modelA = displayIndexToModelIndex(displayA);
    const int modelB = displayIndexToModelIndex(displayB);
    if (modelA < 0 || modelB < 0 || modelA == modelB) {
        return;
    }

    beginSwapAnimation(displayA, displayB, modelA, modelB);
}

/**
 * Begins a swap animation between two player cards, moving each to the
 * other's home position.
 * Records both cards' current positions as animation start points and hides their slot labels for the duration of the animation.
 * The model swap is deferred until the animation completes. Does nothing if
 * either display index is invalid or either card pointer is null.
 *
 * @param displayA  The display index of the first card to swap.
 * @param displayB  The display index of the second card to swap.
 * @param modelA    The model index of the first player, committed on completion.
 * @param modelB    The model index of the second player, committed on completion.
 */
void LobbyScene::beginSwapAnimation(int displayA, int displayB, int modelA, int modelB) {
    if (displayA < 0 || displayB < 0 || displayA >= (int)_playerCards.size() || displayB >= (int)_playerCards.size()) {
        return;
    }

    auto cardA = _playerCards[displayA];
    auto cardB = _playerCards[displayB];
    if (!cardA || !cardB) {
        return;
    }

    _isSwapAnimating = true;
    _swapAnimElapsed = 0.0f;
    _swapAnimDisplayA = displayA;
    _swapAnimDisplayB = displayB;
    _swapAnimStartA = cardA->getPosition();
    _swapAnimStartB = cardB->getPosition();
    _pendingModelSwapA = modelA;
    _pendingModelSwapB = modelB;

    if (displayA >= 0 && displayA < (int)_playerSlots.size() && _playerSlots[displayA]) {
        _playerSlots[displayA]->setVisible(false);
    }
    if (displayB >= 0 && displayB < (int)_playerSlots.size() && _playerSlots[displayB]) {
        _playerSlots[displayB]->setVisible(false);
    }
}

/**
 * Updates the swap animation for two player cards trading positions.
 * Linearly interpolates each card from its starting position to the other
 * card's home position over the configured duration.
 * On completion, snaps both cards to their final positions, restores slot visibility, and
 * commits the pending model swap via the network and game state.
 * Resets all animation state when complete or if either display index is invalid.
 *
 * @param timestep  The time elapsed since the last update, in seconds.
 */
void LobbyScene::updateSwapAnimation(float timestep) {
    if (!_isSwapAnimating) {
        return;
    }

    bool invalid = false;
    if (_swapAnimDisplayA < 0 || _swapAnimDisplayB < 0){
        invalid = true;
    }
    if (_swapAnimDisplayA >= (int)_playerCards.size() || _swapAnimDisplayB >= (int)_playerCards.size()) {
        invalid = true;
    }
    if (!_playerCards[_swapAnimDisplayA] || !_playerCards[_swapAnimDisplayB]) {
        invalid = true;
    }
    if (_swapAnimDisplayA >= (int)_playerCardHomePositions.size() || _swapAnimDisplayB >= (int)_playerCardHomePositions.size()) {
        invalid = true;
    };
    if (invalid) { //end
        if (_swapAnimDisplayA >= 0 && _swapAnimDisplayA < (int)_playerSlots.size() && _playerSlots[_swapAnimDisplayA]) {
            _playerSlots[_swapAnimDisplayA]->setVisible(true); //reset anim
        }
        if (_swapAnimDisplayB >= 0 && _swapAnimDisplayB < (int)_playerSlots.size() && _playerSlots[_swapAnimDisplayB]) {
            _playerSlots[_swapAnimDisplayB]->setVisible(true);
        }
        _isSwapAnimating = false;
        _swapAnimDisplayA = -1;
        _swapAnimDisplayB = -1;
        return;
    }

    _swapAnimElapsed += timestep;
    const float duration = (_swapAnimDuration <= 0.0f ? 0.001f : _swapAnimDuration);
    float t = _swapAnimElapsed / duration;
    if (t > 1.0f) {
        t = 1.0f;
    }

    const Vec2 endA = _playerCardHomePositions[_swapAnimDisplayB];
    const Vec2 endB = _playerCardHomePositions[_swapAnimDisplayA];
    _playerCards[_swapAnimDisplayA]->setPosition(_swapAnimStartA.lerp(endA, t));
    _playerCards[_swapAnimDisplayB]->setPosition(_swapAnimStartB.lerp(endB, t));

    if (t >= 1.0f) {
        _playerCards[_swapAnimDisplayA]->setPosition(_playerCardHomePositions[_swapAnimDisplayA]);
        _playerCards[_swapAnimDisplayB]->setPosition(_playerCardHomePositions[_swapAnimDisplayB]);

        if (_swapAnimDisplayA >= 0 && _swapAnimDisplayA < (int)_playerSlots.size() && _playerSlots[_swapAnimDisplayA]) {
            _playerSlots[_swapAnimDisplayA]->setVisible(true);
        }
        if (_swapAnimDisplayB >= 0 && _swapAnimDisplayB < (int)_playerSlots.size() && _playerSlots[_swapAnimDisplayB]) {
            _playerSlots[_swapAnimDisplayB]->setVisible(true);
        }

        if (_pendingModelSwapA >= 0 && _pendingModelSwapB >= 0 && _pendingModelSwapA != _pendingModelSwapB) {
            if (_network->swapLobbyPlayers(_pendingModelSwapA, _pendingModelSwapB)) {
                _gameState->swapPlayerSlots(_pendingModelSwapA, _pendingModelSwapB);
            }
        }

        _isSwapAnimating = false;
        _swapAnimElapsed = 0.0f;
        _swapAnimDisplayA = -1;
        _swapAnimDisplayB = -1;
        _pendingModelSwapA = -1;
        _pendingModelSwapB = -1;
    }
}

/**
 * Begins a return animation for the player card at the given display index,
 * moving it back to its home position. Records the card's current position
 * as the animation start point. Does nothing if the index is invalid or
 * the card pointer is null.
 *
 * @param displayIndex  The display index of the card to animate back home.
 */
void LobbyScene::beginReturnAnimation(int displayIndex) {
    if (displayIndex < 0 || displayIndex >= (int)_playerCards.size() ||
        displayIndex >= (int)_playerCardHomePositions.size()) {
        return;
    }

    auto card = _playerCards[displayIndex];
    if (!card) {
        return;
    }

    _isReturnAnimating = true;
    _returnAnimDisplayIndex = displayIndex;
    _returnAnimElapsed = 0.0f;
    _returnAnimStart = card->getPosition();
}

/**
 * Updates the return animation for a player card moving back to its home position.
 * Linearly interpolates the card's position from its starting point to its home
 * position over the configured duration. Resets animation state when complete
 * or if the display index is invalid.
 *
 * @param timestep  The time elapsed since the last update, in seconds.
 */
void LobbyScene::updateReturnAnimation(float timestep) {
    if (!_isReturnAnimating) {
        return;
    }

    if (_returnAnimDisplayIndex < 0 || _returnAnimDisplayIndex >= (int)_playerCards.size() ||
        _returnAnimDisplayIndex >= (int)_playerCardHomePositions.size() ||
        !_playerCards[_returnAnimDisplayIndex]) {
        _isReturnAnimating = false;
        _returnAnimDisplayIndex = -1;
        _returnAnimElapsed = 0.0f;
        return;
    }

    _returnAnimElapsed += timestep;
    const float duration = (_returnAnimDuration <= 0.0f ? 0.001f : _returnAnimDuration);
    float t = _returnAnimElapsed / duration;
    if (t > 1.0f) {
        t = 1.0f;
    }

    const Vec2 endPos = _playerCardHomePositions[_returnAnimDisplayIndex];
    _playerCards[_returnAnimDisplayIndex]->setPosition(_returnAnimStart.lerp(endPos, t));

    if (t >= 1.0f) {
        _playerCards[_returnAnimDisplayIndex]->setPosition(endPos);
        _isReturnAnimating = false;
        _returnAnimDisplayIndex = -1;
        _returnAnimElapsed = 0.0f;
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
 */
void LobbyScene::updateNetworkOrder() {
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    const auto& networkedPlayers = _network->getNetworkedPlayers();
    const int realCount = (int)networkedPlayers.size();
    const int totalSlots = (int)_gameState->getPlayers().size();

    for (int i = 0; i < realCount; i++) {
        _gameState->setRealPlayer(
            i,
            networkedPlayers[i].username,
            networkedPlayers[i].houseID
        );
    }

    // Host only: demote any slots beyond the current real player count back to AI
    if (_network->isHost()) {
        for (int i = realCount; i < totalSlots; i++) {
            if (!_gameState->getPlayerBySlot(i)->isAI()) {
                _gameState->demoteToAI(i);
            }
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
    updateSwapAnimation(timestep);
    updateReturnAnimation(timestep);

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
        if (_network->checkGameStarted()) {
            _status = START;
        }
        if (_network->wasSessionTerminated()) {
            _network->clearQueues();
            _network->disconnect();
            _status = Status::ABORT;
            return;
        }
    }
    
    // change boss icon to the currently chosen boss
    updateLobbyBossImage(_network->getEnemy());
    
    // Only the host can start; only enable the button when all players have locked in a house.
    if (_network->isHost() && _network->allPlayersSelectedHouse()) {
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

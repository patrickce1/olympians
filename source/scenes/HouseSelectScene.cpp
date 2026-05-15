#include "HouseSelectScene.h"

using namespace cugl;
using namespace std;

#pragma mark -
#pragma mark Scene Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Role card width */
#define ROLE_CARD_WIDTH 300
/** Interpolation smoothing factor*/
#define SMOOTHING_FACTOR 0.2f
/** Minimum number of frames for a swipe to be registered*/
static constexpr int SWIPE_HOLD_FRAMES = 4;

#pragma mark -
#pragma mark Provided Methods
/**
 * Initializes the house selection scene.
 *
 * This method sets up all UI elements, binds necessary callbacks,
 * and stores references to shared resources such as the asset manager
 * and network controller. It prepares the scene for use but does not
 * make it active or responsive to input.
 *
 * Activation and input handling are controlled separately via setActive().
 *
 * @param assets                           The loaded asset manager used to retrieve scene resources
 * @param networkController   The network controller used for multiplayer communication
 * @param gameState                     The state of the game
 *
 * @return true if the scene was successfully initialized; false otherwise
 */
bool HouseSelectScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
                            const std::shared_ptr<NetworkController>& networkController,
                            GameState* gameState) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    _gameState = gameState;
    _assets = assets;
    _network = networkController;
    loadHouses();
    
    Size dimen = getSize();
    
    // Acquire the scene built by the asset loader and resize it the scene
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("houseSelectScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    setupUI();
    setupListeners();
    
    _status = Status::WAITING;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the house select UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * lock button, back button, carousel navigation
 * buttons, and the role carousel container. It also initializes the
 * carousel item list and the mini icons.
 */
void HouseSelectScene::setupUI() {

    _selectButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.select"));

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.back"));

    // Player and Teammate Icon Widgets
    _playerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.playerSelectIcon"));
    
    _leftPlayerIcon = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>((
        _assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconLeft.icon")));
    _leftPlayerIcon->setScale(0.5f);
    
    _rightPlayerIcon = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>((
        _assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconRight.icon")));
    _rightPlayerIcon->setScale(0.5f);
    
    _upPlayerIcon = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>((
        _assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconUp.icon")));
    _upPlayerIcon->setScale(0.5f);

    if (_playerIcon) {
        _playerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("emptyLocalIcon"));
        _playerIconImage->setScale(0.5f);
        
        _playerIconGlow = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("lockedGlow"));
    }
    
    _playerIconImage->setAnchor(cugl::Vec2::ANCHOR_CENTER);

    _leftButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.carouselButtons.directionButtons.leftScroll"));

    _rightButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.carouselButtons.directionButtons.rightScroll"));

    _houseSelectionCardContainer = _assets->get<scene2::SceneNode>("houseSelectScene.carouselButtons.heroCardContainer");

    if (_houseSelectionCardContainer) {
        for (int i = 0; i < _houseLoader.getAllOrdered().size(); i++) {
            _houseCards.push_back(_houseSelectionCardContainer->getChild(i));
        }
    }
    
    auto houseCarouselDotsContainer = _assets->get<scene2::SceneNode>("houseSelectScene.classSelectionCarouselIcons");
    
    if (houseCarouselDotsContainer) {
        for (int i = 0; i < _houseLoader.getAllOrdered().size(); i++) {
            _houseCarouselDotIndicators.push_back(houseCarouselDotsContainer->getChild(i));
        }
    }
    
    _backgroundImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(_assets->get<scene2::SceneNode>
                                                                            ("houseSelectScene.showroomImage"));
    
    // Capture the container's base position and build the position maps. Card 4 (_currentIndex default) is the starting card,
    // so it maps to startX. All other cards are offset by multiples of ROLE_CARD_WIDTH from there.
    if (_houseSelectionCardContainer) {
        _baseCarouselPosition = _houseSelectionCardContainer->getPosition();
        float startX = _baseCarouselPosition.x;
        int numCards = (int)_houseCards.size();
        for (int i = 0; i < numCards; i++) {
            float containerX     = startX - ((i - 4) * ROLE_CARD_WIDTH);
            _xPosToHouse[containerX] = i;
            _houseToTargetX[i]       = containerX;
        }
    }
}

/**
 * Attaches input listeners to the house select buttons.
 *
 * This method assigns callbacks for starting the game, returning to the
 * previous menu, and navigating the role selection carousel.
 */
void HouseSelectScene::setupListeners() {
    
    _selectButton->addListener([this](const std::string& name, bool down) {
        if (!down) return;

        HouseLoader::HouseDef currentHouse = _houseLoader.getAllOrdered()[_currentIndex];

        // If a house is selected and we're facing it, clear the selection
        if (_selectedHouse && isCurrentHouseSelected()) {
            _selectedHouse = false;
            _playerIconGlow->setVisible(false);
            updateText(_selectButton, "SELECT");
            updateSelectedIcon(_currentIndex);
            commitHouseUnlock();
            return;
        }

        // Otherwise, check if the house is taken and select it
        bool taken = _network->isHouseTaken(currentHouse.id);

        if (!taken && _targetSlot != -1) {
            int localIndex = _network->getLocalPlayerNumber();
            const auto& slotToPlayer = _network->getNetworkedPlayers();
            auto pair = slotToPlayer.find(localIndex);
            if (pair != slotToPlayer.end()) {
                taken = (pair->second.houseID == currentHouse.id);
            }
        }

        if (taken) return;

        _selectedHouse = true;
        updateSelectedIcon(_currentIndex, false);
        _playerIconGlow->setVisible(true);
        _status = Status::ABORT;
        commitHouseLock(currentHouse);
    });

    _backButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });

    _leftButton->addListener([this](const std::string& name, bool down){
        if (!down) slideTo(_currentIndex - 1);
    });

    _rightButton->addListener([this](const std::string& name, bool down){
        if (!down) slideTo(_currentIndex + 1);
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void HouseSelectScene::dispose() {
    if (_active){
        removeAllChildren();
        _selectButton = nullptr;
        _backButton = nullptr;
        _playerIcon = nullptr;
        _playerIconImage = nullptr;
        _playerIconGlow = nullptr;
        _leftButton = nullptr;
        _rightButton = nullptr;
        _houseSelectionCardContainer = nullptr;
        _houseCards.clear();
        _houseCarouselDotIndicators.clear();
        _active = false;
    }
    // Full wipe — clear all persisted slot states
    _slotStates.clear();
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
void HouseSelectScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _isSwiping            = false;
            _swipeContainerStartX = 0.0f;
            _swipeTouchInitialPos = cugl::Vec2::ZERO;
            _swipeHoldFrames      = 0;
            _status = WAITING;

            if (_pendingReset) {
                _pendingReset = false;
            }

            // Restore state for the slot we're opening, or use defaults
            SlotState& state = _slotStates[_targetSlot];
            
            // Lock state reflects whether the slot has a house, not whether the
            // host explicitly clicked lock. After a reset, AI slots may have a
            // house assigned by assignMissingHousesForAI without ever being
            // manually locked, so we derive it from the player's actual house.
            int slot = (_targetSlot == -1) ? _network->getLocalPlayerNumber() : _targetSlot;
            Player* player = _gameState->getPlayerBySlot(slot);
            _selectedHouse = (player && !player->getHouseName().empty());
            state.selectedHouse = _selectedHouse;

            // Restore lock button label and glow
            updateText(_selectButton, "SELECT");
            _playerIconGlow->setVisible(_selectedHouse);

            // Jump carousel to the saved index (no animation on restore)
            _isAnimating = false;
            refreshLocalPlayerIcon();
            _rightButton->setVisible(true);
            _leftButton->setVisible(true);
            slideTo(getInitialCarouselIndex(_targetSlot));
            updateTeammateIcons();

            _selectButton->activate();
            _leftButton->activate();
            _rightButton->activate();
            _backButton->activate();
        } else {
            _isSwiping            = false;
            _swipeContainerStartX = 0.0f;
            
            // Save current state before deactivating
            SlotState& state    = _slotStates[_targetSlot];
            state.carouselIndex = _currentIndex;
            state.selectedHouse = _selectedHouse;

            _targetSlot = -1;
            _selectButton->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backButton->deactivate();
            _selectButton->setDown(false);
            _backButton->setDown(false);
            _leftButton->setDown(false);
            _rightButton->setDown(false);
        }
    }
}

/**
 * Updates the text in the given button.
 *
 * Techincally a button does not contain text. A button is simply a scene graph
 * node with one child for the up state and another for the down state. So to
 * change the text in one of our buttons, we have to descend the scene graph.
 * This method simplifies this process for you.
 *
 * @param button    The button to modify
 * @param text      The new text value
 */
void HouseSelectScene::updateText(const std::shared_ptr<scene2::Button>& button, const std::string text) {
    auto label = std::dynamic_pointer_cast<scene2::Label>(button->getChildByName("label"));
    label->setText(text);
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 * @param input         The input controller instance
 */
void HouseSelectScene::update(float timestep, InputController& input) {
    _network->getNetworkUpdates();
    
    // Check if host disconnected
    if (_network->wasHostDisconnected() && !_network->isHost()) {
        _network->disconnect();
        _status = Status::HOST_DISCONNECTED;
        return;
    }

    // Check kick BEFORE clearQueues wipes the flag
    if (!_network->isHost() && _network->wasSessionTerminated()) {
        _network->clearQueues();
        _network->disconnect();
        _status = Status::ABORT;
        return;
    }
    
    // Forward to PreGameScene
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
        return;
    }
    
    updateNetworkOrder();
    updateTeammateIcons();
    updateTakenHouseCards();
    _playerIconGlow->setVisible(hasLocalPlayerSelectedHouse());
    
    // The carousel move logic
    if (_isAnimating) {
        Vec2 current = _houseSelectionCardContainer->getPosition();
        Vec2 next = current.lerp(_slideTarget, SMOOTHING_FACTOR); // 0.2 = smoothing factor

        if (current.distance(_slideTarget) < 1.0f) {
            _houseSelectionCardContainer->setPosition(_slideTarget);
            _isAnimating = false;
        } else {
            _houseSelectionCardContainer->setPosition(next);
        }
    }
    
    handleSwipeBegin(input);
    handleSwipeTracking(input);
    handleSwipeRelease(input);
    
    updateBossBGImage(_network->getEnemy());
}

/**
 * Initiates a slide animation to center the house card at `newIndex`.
 *
 * Does nothing if an animation is already in progress or if the
 * index is out of bounds. Otherwise computes the target container
 * position and stores it in `_slideTarget`.
 *
 * @param newIndex The index of the item to slide to.
 */
void HouseSelectScene::slideTo(int newIndex) {
    if (_isAnimating) return;
    if (newIndex < 0 || newIndex >= _houseCards.size()) return;

    _isAnimating = true;

    float shiftAmount = ROLE_CARD_WIDTH;
    
    int deltaIndex = newIndex - _currentIndex;
    Vec2 currentPos = _houseSelectionCardContainer->getPosition();
    float targetX = currentPos.x - (deltaIndex * shiftAmount);
    
    _slideTarget = Vec2(targetX, currentPos.y);
    _currentIndex = newIndex;
    
    if (_currentIndex == 0) {
        _leftButton->setVisible(false);
    } else if (_currentIndex == _houseCards.size() - 1) {
        _rightButton->setVisible(false);
    } else {
        _rightButton->setVisible(true);
        _leftButton->setVisible(true);
    }
    
    for (int i = 0; i < _houseCards.size(); i++) {
        auto card = _houseCards[i];
        if (card) {
            auto glow = card->getChildByName("glowOverlayHero");
            if (glow){
                glow->setVisible(false);
                if (i == newIndex) {
                    glow->setVisible(true);
                }
            }
        }
    }
    
    updateCarouselDots(newIndex);
    updateSelectedIcon(newIndex);
    updateText(_selectButton, (_selectedHouse && isCurrentHouseSelected()) ? "DESELECT" : "SELECT");
}

/**
 * Updates the circular indicators at the bottom of what card in the carousel
 * we are currently at.
 *
 * @param currentIndex The index of the card we are at.
 */
void HouseSelectScene::updateCarouselDots(int currentIndex) {
    for (int i = 0; i < _houseCarouselDotIndicators.size(); i++) {
        auto node = _houseCarouselDotIndicators[i];
        
        auto fill   = node->getChildByName("fill");
        
        if (i == currentIndex) {
            fill->setColor(Color4("#4c3214ff"));
        } else {
            fill->setColor(Color4("#9d7137ff"));
        }
    }
}

/**
 * Updates the teammate icon diamond that corresponds to _targetSlot with
 * the house at the given carousel index. Used during AI slot mode so the
 * host can preview the selection without touching their own icon diamond.
 *
 * @param currentIndex  The carousel index whose house to preview.
 */
void HouseSelectScene::updateAIPreviewIcon(int currentIndex) {
    if (!_gameState || !_network) return;

    int localIndex = _network->getLocalPlayerNumber();
    int totalSlots = (int)_gameState->getPlayers().size();

    // Teammate icons: right=(localIndex+1)%total, up=(localIndex+2)%total, left=(localIndex+3)%total
    std::vector<std::shared_ptr<cugl::scene2::PolygonNode>> iconSlots = {
        _rightPlayerIcon, _upPlayerIcon, _leftPlayerIcon
    };

    for (int i = 1; i <= 3; i++) {
        int slot = (localIndex + i) % totalSlots;
        if (slot != _targetSlot) continue;

        auto activeIcon = iconSlots[i - 1];
        if (!activeIcon) return;

        const HouseLoader::HouseDef& house = _houseLoader.getAllOrdered()[currentIndex];
        std::string key = house.id + "SIcon";
        auto texture = _assets->get<cugl::graphics::Texture>(key);
        activeIcon->setTexture(texture != nullptr
            ? texture
            : _assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
        activeIcon->setScale(0.46);
        return;
    }
}

/**
 * Updates the local player's icon in the diamond based on the house card
 * they are currently on. If commitToGameState is true, also updates the
 * local player's house in GameState — should only be true when the player
 * locks in their selection.
 *
 * @param currentIndex      The index of the card we are at.
 * @param commitToGameState Whether to write the house selection to GameState.
 */
void HouseSelectScene::updateSelectedIcon(int currentIndex, bool commitToGameState) {
    if (!_playerIconImage) return;
    
    // If a house is already selected, always show the committed house, not the carousel position
    if (_selectedHouse) {
        refreshLocalPlayerIcon();
        return;
    }

    const HouseLoader::HouseDef& selectedHouse = _houseLoader.getAllOrdered()[currentIndex];
    std::string key = selectedHouse.id + "SIcon";
    auto texture = _assets->get<cugl::graphics::Texture>(key);

    if (_targetSlot == -1) {
        // Normal mode — update the local player's own icon diamond
        _playerIconImage->setTexture(texture != nullptr
            ? texture
            : _assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
        _playerIconImage->setScale(0.5f);

        if (commitToGameState && _gameState) {
            int localIndex = _network->getLocalPlayerNumber();
            if (localIndex >= 0) {
                _gameState->setRealPlayer(
                    localIndex,
                    _gameState->getPlayerBySlot(localIndex)->getPlayerName(),
                    selectedHouse.id
                );
            }
        }
    } else {
        // AI slot mode — update the teammate icon that corresponds to _targetSlot
        // and leave the host's own icon diamond untouched
        updateAIPreviewIcon(currentIndex);
    }
}

/**
 * Updates the background image of the boss display based on the selected enemy.
 *
 * @param enemyID The identifier of the enemy whose background should be displayed.
 */
void HouseSelectScene::updateBossBGImage(std::string enemyID) {
    if (enemyID == "" && _currentBoss == "") {
        return;
    } else if (enemyID == _currentBoss) {
        return;
    }
    
    _currentBoss = enemyID;
    if (_currentBoss == "cyclops") {
        _backgroundImage->setTexture(_assets->get<cugl::graphics::Texture>("cyclopsShowroom"));
    } else if (_currentBoss == "cerberus") {
        _backgroundImage->setTexture(_assets->get<cugl::graphics::Texture>("cerberusShowroom"));
    } else if (_currentBoss == "circe") {
        _backgroundImage->setTexture(_assets->get<cugl::graphics::Texture>("circeShowroom"));
    } else if (_currentBoss == "gaia") {
        _backgroundImage->setTexture(_assets->get<cugl::graphics::Texture>("gaiaShowroom"));
    }
}

/** Loads houses definitions from the house JSON to use in house selection. */
bool HouseSelectScene::loadHouses() {
    const std::string houseJsonPath = "json/houses.json";
    if (!_houseLoader.loadFromFile(houseJsonPath)) {
        CULog("HouseSelectScene: Failed to load house.json");
        return false;
    }
    return true;
}

/**
 * Syncs the game state player names and houses with the current
 * networked player list. Called every frame during house selection so that
 * _gameState reflects the latest connected player info, including house
 * selections made by other players while this scene is active.
 *
 * Uses checkRealPlayer() per slot rather than assuming real players occupy
 * the first N slots, since players can swap positions. AI slots always use
 * demoteToAI() to preserve isAI() == true — setRealPlayer() would
 * reconstruct them as plain Player objects and break AI behavior.
 */
void HouseSelectScene::updateNetworkOrder() {
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    _network->getNetworkUpdates();
    const auto& slotToPlayer = _network->getNetworkedPlayers();
    if (slotToPlayer.empty()) return;

    int totalSlots = (int)_gameState->getPlayers().size();

    for (int i = 0; i < totalSlots; i++) {
        auto pair = slotToPlayer.find(i);
        if (pair != slotToPlayer.end()) {
            _gameState->setRealPlayer(i, pair->second.username, pair->second.houseID);
        } else {
            // AI slot — use demoteToAI() to preserve isAI() == true.
            // House is synced from the host's authoritative _aIHouses map,
            // kept in sync across all clients via LOBBY_UPDATE.
            _gameState->demoteToAI(i, _network->getAIHouse(i));
        }
    }

    _network->clearQueues();
}

/**
 * Updates the three teammate icon images in the house select screen
 * based on each player's selected house. Uses the same circular remapping
 * as LobbyScene so that left, right, and top slots always reflect the
 * correct neighbours relative to the local player.
 *
 * Slot order after remap: [0]=right neighbour, [1]=opposite, [2]=left neighbour
 * matching the _rightPlayerIcon, _upPlayerIcon, _leftPlayerIcon positions.
 */
void HouseSelectScene::updateTeammateIcons() {
    if (!_gameState || !_network) return;
    
    int localIndex = _network->getLocalPlayerNumber();
    if (localIndex < 0) return;
    
    const auto& players = _gameState->getPlayers();
    int totalSlots = (int)players.size();

    std::vector<std::shared_ptr<cugl::scene2::PolygonNode>> iconSlots = {
        _rightPlayerIcon, _upPlayerIcon, _leftPlayerIcon
    };

    for (int i = 1; i <= 3; i++) {
        int slot = (localIndex + i) % totalSlots;
        
        // Skip the slot being actively previewed — updateAIPreviewIcon owns it
        if (_targetSlot != -1 && slot == _targetSlot) continue;
        
        auto activeIcon = iconSlots[i - 1];
        if (!activeIcon) continue;

        std::string house = players[slot]->getHouseName();
        std::string key = house + "SIcon";
        if (_assets->get<cugl::graphics::Texture>(key) != nullptr) {
            activeIcon->setTexture(_assets->get<cugl::graphics::Texture>(key));
        } else {
            activeIcon->setTexture(_assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
        }
        activeIcon->setScale(0.48);
    }
}

/**
 * Greys out any house cards that have already been claimed by another
 * player. Called every frame in update() so the visual stays in sync
 * as other players lock in their selections.
 *
 * If a card has a child node named "takenOverlay", that node is shown
 * or hidden. Otherwise the card's color alpha is reduced to indicate
 * it is unavailable.
 */
void HouseSelectScene::updateTakenHouseCards() {
    std::vector<std::string> takenHouses = _network->getTakenHouses();
    
    // In AI slot mode, the host's own house is also unavailable
    if (_targetSlot != -1) {
        int localIndex = _network->getLocalPlayerNumber();
        const auto& slotToPlayer = _network->getNetworkedPlayers();
        auto pair = slotToPlayer.find(localIndex);
        if (pair != slotToPlayer.end()) {
            const std::string& hostHouse = pair->second.houseID;
            if (!hostHouse.empty()) {
                takenHouses.push_back(hostHouse);
            }
        }
    }

    const auto& allHouses = _houseLoader.getAllOrdered();
    for (int i = 0; i < (int)_houseCards.size(); i++) {
        auto card = _houseCards[i];
        if (!card) continue;

        bool taken = false;
        if (i < (int)allHouses.size()) {
            const std::string& houseID = allHouses[i].id;
            for (const auto& takenHouse : takenHouses) {
                if (takenHouse == houseID) { taken = true; break; }
            }
        }

        auto children = card->getChildren();

        for (auto child : children) {
            if (child->getName() == "title") {
                child->setColor(Color4(255, 255, 255, 255));
            } else {
                child->setColor(taken ? Color4(255, 255, 255, 100) : Color4(255, 255, 255, 255));
            }
        }
    }
}

/**
 * Commits a house lock for the current carousel selection. Writes the
 * chosen house to the correct slot in GameState and broadcasts it over
 * the network. If _targetSlot is -1, writes to the local player's slot;
 * otherwise writes to the AI slot the host is configuring.
 *
 * @param selectedHouse  The house definition the player locked in.
 */
void HouseSelectScene::commitHouseLock(const HouseLoader::HouseDef& selectedHouse) {
    if (_targetSlot == -1) {
        // Normal mode: selecting for the local player
        if (_gameState) {
            int localIndex = _network->getLocalPlayerNumber();
            _gameState->setRealPlayer(
                localIndex,
                _gameState->getPlayerBySlot(localIndex)->getPlayerName(),
                selectedHouse.id
            );
        }
        _network->broadcastSelectedHouse(selectedHouse.id);
        if (_network->isHost()) {
            _network->setLocalHouse(selectedHouse.id);
        }
    } else {
        // AI slot mode: host is selecting on behalf of an AI slot
        if (_gameState) {
            _gameState->setRealPlayer(
                _targetSlot,
                _gameState->getPlayerBySlot(_targetSlot)->getPlayerName(),
                selectedHouse.id
            );
        }
        _network->broadcastAIHouseSelection(_targetSlot, selectedHouse.id);
    }
}

/**
 * Clears the house selection for the current target slot and broadcasts
 * the change. Only has an effect in AI slot mode (_targetSlot != -1).
 */
void HouseSelectScene::commitHouseUnlock() {
    if (_targetSlot == -1) {
        // Normal mode: clear local player's house
        _network->broadcastSelectedHouse(std::string(""));
        if (_network->isHost()) {
            _network->setLocalHouse("");
        }
    } else {
        // AI slot mode: clear the AI slot
        if (_gameState) {
            _gameState->setRealPlayer(
                _targetSlot,
                _gameState->getPlayerBySlot(_targetSlot)->getPlayerName(),
                ""
            );
        }
        _network->broadcastAIHouseSelection(_targetSlot, "");
    }
}

/**
 * Refreshes the local player's icon diamond to reflect their actual
 * committed house selection when the scene activates. Prevents a stale
 * carousel preview texture from persisting across activations.
 */
void HouseSelectScene::refreshLocalPlayerIcon() {
    int localIndex = _network->getLocalPlayerNumber();
    const auto& slotToPlayer = _network->getNetworkedPlayers();
    auto pair = slotToPlayer.find(localIndex);
    std::string localHouse = (pair != slotToPlayer.end()) ? pair->second.houseID : "";

    if (localHouse.empty()) {
        _playerIconImage->setTexture(_assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
    } else {
        std::string key = localHouse + "SIcon";
        auto texture = _assets->get<cugl::graphics::Texture>(key);
        _playerIconImage->setTexture(texture != nullptr
            ? texture
            : _assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
    }
}

/**
 * Returns true if the local player has a house selected in the network.
 */
bool HouseSelectScene::hasLocalPlayerSelectedHouse() const {
    int localIndex = _network->getLocalPlayerNumber();
    const auto& slotToPlayer = _network->getNetworkedPlayers();
    auto pair = slotToPlayer.find(localIndex);
    if (pair == slotToPlayer.end()) return false;
    return !pair->second.houseID.empty();
}

/**
 * Returns the carousel index for the given slot when the scene opens.
 * If the player in that slot has a house selected, returns the index of
 * that house so the carousel always opens facing their current selection.
 * Falls back to the saved carousel state if they have no house yet.
 *
 * @param targetSlot  The slot to open (-1 for the local player, or a
 *                    0-based AI slot index).
 * @return            The carousel index to slide to on activation.
 */
int HouseSelectScene::getInitialCarouselIndex(int targetSlot) {
    int slot = (targetSlot == -1) ? _network->getLocalPlayerNumber() : targetSlot;
    Player* player = _gameState->getPlayerBySlot(slot);
    if (player && !player->getHouseName().empty()) {
        const auto& allHouses = _houseLoader.getAllOrdered();
        for (int i = 0; i < (int)allHouses.size(); i++) {
            if (allHouses[i].id == player->getHouseName()) {
                return i;
            }
        }
    }
    return _slotStates[targetSlot].carouselIndex;
}

#pragma mark -
#pragma mark Swipe Gesture Handling

/**
 * Records the touch-down position to begin tracking a potential swipe.
 *
 * Called every frame from update(). On the first frame a touch is
 * detected while no swipe is already in progress, stores the starting
 * X coordinate (screen space) in _swipeTouchStartX and sets _isSwiping.
 * No-op on subsequent frames or when a gesture is already active.
 *
 * @param input  The input controller for this frame.
 */
void HouseSelectScene::handleSwipeBegin(InputController& input) {
    if (_isSwiping) return;

    if ((!input.isTouching() && !input.isMouseDown()) || _isAnimating) {
        _swipeHoldFrames      = 0;
        _swipeTouchInitialPos = cugl::Vec2::ZERO;
        return;
    }

    if (_swipeHoldFrames == 0) {
        _swipeTouchInitialPos = input.getTouchStart();
    }

    Vec2 worldCurrent     = screenToWorldCoords(input.getDragPos());
    Vec2 worldStart       = screenToWorldCoords(_swipeTouchInitialPos);
    float horizontalDelta = std::abs(worldCurrent.x - worldStart.x);
    float verticalDelta   = std::abs(worldCurrent.y - worldStart.y);

    if (horizontalDelta > verticalDelta && horizontalDelta > 5.0f) {
        _swipeHoldFrames++;
    } else {
        _swipeHoldFrames = 0;
    }

    if (_swipeHoldFrames >= SWIPE_HOLD_FRAMES) {
        _swipeTouchStartX     = screenToWorldCoords(_swipeTouchInitialPos).x;
        _swipeContainerStartX = _houseSelectionCardContainer->getPosition().x;
        _isSwiping            = true;
        _swipeHoldFrames      = 0;
    }
}

/**
 * Moves the card container directly under the finger each frame while
 * a swipe is active. Computes the delta from the touch-down position and
 * applies it to the container's position at the start of the drag.
 * Clamps the container so it cannot be dragged past the first or last card.
 *
 * @param input  The input controller for this frame.
 */
void HouseSelectScene::handleSwipeTracking(InputController& input) {
    if (!_isSwiping) return;
    if (!input.isTouching() && !input.isMouseDown()) return;

    Vec2 worldPos     = screenToWorldCoords(input.getDragPos());
    float fingerDelta = worldPos.x - _swipeTouchStartX;
    float rawX        = _swipeContainerStartX + fingerDelta;

    // Clamp between the first and last card's target container X.
    float maxX     = _houseToTargetX[0] + (ROLE_CARD_WIDTH * 2.0f);
    float minX     = _houseToTargetX[(int)_houseCards.size() - 1] - ROLE_CARD_WIDTH;
    float clampedX = std::max(minX, std::min(maxX, rawX));

    Vec2 pos = _houseSelectionCardContainer->getPosition();
    _houseSelectionCardContainer->setPosition(Vec2(clampedX, pos.y));
}

/**
 * Called on finger lift. Delegates to snapToNearestHouse() to find and
 * animate to the closest card to the current container position.
 * Clears all swipe tracking state before returning.
 *
 * @param input  The input controller for this frame.
 */
void HouseSelectScene::handleSwipeRelease(InputController& input) {
    if (!input.touchEnded()) return;
    if (!_isSwiping) return;

    float releaseContainerX = _houseSelectionCardContainer->getPosition().x;
    snapToNearestHouse(releaseContainerX);

    _isSwiping            = false;
    _swipeTouchStartX     = 0.0f;
    _swipeContainerStartX = 0.0f;
}

/**
 * Finds the card whose X position in _xPosToHouse is closest to
 * `releaseContainerX`, updates _currentIndex to that card's index,
 * updates the glow overlays and dot indicators, and initiates a lerp
 * animation to that card's exact centred container position.
 *
 * @param releaseContainerX  The container's X position at the moment
 *                           the finger lifted, in the container's
 *                           parent's local space.
 */
void HouseSelectScene::snapToNearestHouse(float releaseContainerX) {
    int   nearestIndex = 0;
    float nearestDist  = FLT_MAX;

    for (auto& [containerX, index] : _xPosToHouse) {
        float dist = std::abs(releaseContainerX - containerX);
        if (dist < nearestDist) {
            nearestDist  = dist;
            nearestIndex = index;
        }
    }

    _currentIndex   = nearestIndex;
    _isAnimating    = true;
    Vec2 currentPos = _houseSelectionCardContainer->getPosition();
    _slideTarget    = Vec2(_houseToTargetX[nearestIndex], currentPos.y);

    _leftButton->setVisible(_currentIndex > 0);
    _rightButton->setVisible(_currentIndex < (int)_houseCards.size() - 1);

    for (int i = 0; i < (int)_houseCards.size(); i++) {
        auto glow = _houseCards[i]->getChildByName("glowOverlayHero");
        if (glow) glow->setVisible(i == nearestIndex);
    }

    updateCarouselDots(nearestIndex);
    updateSelectedIcon(nearestIndex);
    updateText(_selectButton, (_selectedHouse && isCurrentHouseSelected()) ? "DESELECT" : "SELECT");
}

/**
 * Returns true if the house currently shown in the carousel matches
 * the house committed by the player in the active slot. Used to
 * determine whether the select button should display "DESELECT" instead
 * of "SELECT" when the player is facing their own selection.
 *
 * @return true if the current carousel house matches the committed house.
 */
bool HouseSelectScene::isCurrentHouseSelected() const {
    int slot = (_targetSlot == -1) ? _network->getLocalPlayerNumber() : _targetSlot;
    Player* player = _gameState->getPlayerBySlot(slot);
    if (!player || player->getHouseName().empty()) return false;
    const auto& allHouses = _houseLoader.getAllOrdered();
    if (_currentIndex < 0 || _currentIndex >= (int)allHouses.size()) return false;
    return allHouses[_currentIndex].id == player->getHouseName();
}

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

    _lockButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.lock"));

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.back"));

    // Player and Teammate Icon Widgets
    _playerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.playerSelectIcon"));
    _leftPlayerIcon = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>((
        _assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconLeft")));
    _rightPlayerIcon = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>((
        _assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconRight")));
    _upPlayerIcon = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>((
        _assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconUp")));

    if (_playerIcon) {
        _playerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("emptyLocalIcon"));
        
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
}

/**
 * Attaches input listeners to the house select buttons.
 *
 * This method assigns callbacks for starting the game, returning to the
 * previous menu, and navigating the role selection carousel.
 */
void HouseSelectScene::setupListeners() {
    
    _lockButton->addListener([this](const std::string& name, bool down) {
        if (!down) return;

        HouseLoader::HouseDef selectedHouse = _houseLoader.getAllOrdered()[_currentIndex];

        if (!_locked) {
            bool taken = _network->isHouseTaken(selectedHouse.id);

            // In AI slot mode, also block the host's own locked house
            if (!taken && _targetSlot != -1) {
                int localIndex = _network->getLocalPlayerNumber();
                const auto& networkedPlayers = _network->getNetworkedPlayers();
                if (localIndex >= 0 && localIndex < (int)networkedPlayers.size()) {
                    taken = (networkedPlayers[localIndex].houseID == selectedHouse.id);
                }
            }

            if (taken) return;
        }

        _locked = !_locked;

        if (_locked) {
            updateSelectedIcon(_currentIndex, false);
            updateText(_lockButton, "UNLOCK");
            _playerIconGlow->setVisible(true);
            _status = Status::LOCKED;
            commitHouseLock(selectedHouse);
        } else {
            updateText(_lockButton, "LOCK");
            _playerIconGlow->setVisible(false);
            _status = Status::WAITING;
            commitHouseUnlock();
        }
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
        _lockButton = nullptr;
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
            _status = WAITING;

            if (_pendingReset) {
                // Full wipe — clear all persisted slot states
                _slotStates.clear();
                _pendingReset = false;
            }

            // Restore state for the slot we're opening, or use defaults
            SlotState& state = _slotStates[_targetSlot];
            _locked = state.locked;

            // Restore lock button label and glow
            updateText(_lockButton, _locked ? "UNLOCK" : "LOCK");
            _playerIconGlow->setVisible(_locked);

            // Jump carousel to the saved index (no animation on restore)
            _isAnimating = false;
            refreshLocalPlayerIcon();
            slideTo(state.carouselIndex);

            _lockButton->activate();
            _leftButton->activate();
            _rightButton->activate();
            _backButton->activate();
        } else {
            // Save current state before deactivating
            SlotState& state = _slotStates[_targetSlot];
            state.carouselIndex = _currentIndex;
            state.locked        = _locked;

            _targetSlot = -1;
            _lockButton->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backButton->deactivate();
            _lockButton->setDown(false);
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
 */
void HouseSelectScene::update(float timestep) {
    _network->getNetworkUpdates();

    // Check kick BEFORE clearQueues wipes the flag
    if (!_network->isHost() && _network->wasSessionTerminated()) {
        _network->clearQueues();
        _network->disconnect();
        _status = Status::ABORT;
        return;
    }
    
    // Forward to game scene if host started while we were here
    if (!_network->isHost() && _network->checkGameStarted()) {
        _network->clearQueues();
        _status = Status::GAMESCENE_START;
        return;
    }
    
    updateNetworkOrder();   // this will call getNetworkUpdates + clearQueues internally
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
    
    updateBossBGImage(_network->getEnemy());
}

/**
 * Reconfigures the lock button for this scene
 *
 * This is necessary because what the buttons do depends on the state of the
 * networking.
 */
void HouseSelectScene::configureLockButton() {
    updateText(_lockButton,"Lock");
    _lockButton->activate();
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
    if (!_locked){
        updateSelectedIcon(newIndex);
    }
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
        activeIcon->setScale(0.92);
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

    const HouseLoader::HouseDef& selectedHouse = _houseLoader.getAllOrdered()[currentIndex];
    std::string key = selectedHouse.id + "SIcon";
    auto texture = _assets->get<cugl::graphics::Texture>(key);

    if (_targetSlot == -1) {
        // Normal mode — update the local player's own icon diamond
        _playerIconImage->setTexture(texture != nullptr
            ? texture
            : _assets->get<cugl::graphics::Texture>("emptyLocalIcon"));

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
 */
void HouseSelectScene::updateNetworkOrder() {
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    _network->getNetworkUpdates();
    const auto& networkedPlayers = _network->getNetworkedPlayers();

    if (networkedPlayers.empty()) return;

    for (int i = 0; i < (int)networkedPlayers.size(); i++) {
        _gameState->setRealPlayer(
            i,
            networkedPlayers[i].username,
            networkedPlayers[i].houseID
        );
    }

    // Sync AI slot house selections from the host's authoritative map
    int realPlayerCount = (int)networkedPlayers.size();
    int totalSlots = (int)_gameState->getPlayers().size();
    for (int i = realPlayerCount; i < totalSlots; i++) {
        std::string aIHouse = _network->getAIHouse(i);
        if (!aIHouse.empty()) {
            _gameState->setRealPlayer(
                i,
                _gameState->getPlayerBySlot(i)->getPlayerName(),
                aIHouse
            );
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
        activeIcon->setScale(0.92);
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
        const auto& networkedPlayers = _network->getNetworkedPlayers();
        if (localIndex >= 0 && localIndex < (int)networkedPlayers.size()) {
            const std::string& hostHouse = networkedPlayers[localIndex].houseID;
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
    const auto& networkedPlayers = _network->getNetworkedPlayers();
    std::string localHouse = (localIndex >= 0 && localIndex < (int)networkedPlayers.size())
        ? networkedPlayers[localIndex].houseID
        : "";

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
    const auto& networkedPlayers = _network->getNetworkedPlayers();
    if (localIndex < 0 || localIndex >= (int)networkedPlayers.size()) return false;
    return !networkedPlayers[localIndex].houseID.empty();
}

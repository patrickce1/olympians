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
 * @param assets              The loaded asset manager used to retrieve scene resources
 * @param networkController   The network controller used for multiplayer communication
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

    _backOut = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.back"));

    // actuall image, make into widget for access
    _playerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.playerSelectIcon"));
    _leftPlayerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconLeft"));
    _rightPlayerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconRight"));
    _upPlayerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.teamSelectIconUp"));
    
    if (_playerIcon) {
        _playerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("emptyLocalIcon"));
        
        _playerIconGlow = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("lockedGlow"));
    }
    
    if (_leftPlayerIcon) {
        _leftPlayerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
            _leftPlayerIcon->getChildByName("emptyLocalIcon"));
        CULog("leftPlayerIconImage found: %d", _leftPlayerIconImage != nullptr);
    }
    
    if (_rightPlayerIcon) {
        _rightPlayerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
            _rightPlayerIcon->getChildByName("emptyLocalIcon"));
        CULog("rightPlayerIconImage found: %d", _rightPlayerIconImage != nullptr);
    }
    
    if (_upPlayerIcon) {
        _upPlayerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
            _upPlayerIcon->getChildByName("emptyLocalIcon"));
        CULog("upPlayerIconImage found: %d", _upPlayerIconImage != nullptr);
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
        _locked = !_locked;

        if (_locked) {
            // Get the selected house using carousel index
            HouseLoader::HouseDef selectedHouse = _houseLoader.getAllOrdered()[_currentIndex];
            
            // Update UI
            updateSelectedIcon(_currentIndex, true);
            updateText(_lockButton, "UNLOCK");
            _playerIconGlow->setVisible(true);
            
            // Update local status
            _status = Status::LOCKED;
            
            // Broadcast selection over network
            _network->broadcastSelectedHouse(selectedHouse.id);
            
            // Host sets their own houseID directly since sendToHost doesn't loop back
            if (_network->isHost()) {
                _network->setLocalHouse(selectedHouse.id);
            }
        } else {
            // Update UI
            updateText(_lockButton, "LOCK");
            _playerIconGlow->setVisible(false);
            
            // Update local status
            _status = Status::WAITING;
        }
    });

    _backOut->addListener([this](const std::string& name, bool down) {
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
        _backOut = nullptr;
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
            _lockButton->activate();
            _leftButton->activate();
            _rightButton->activate();
            _backOut->activate();
        } else {
            _lockButton->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backOut->deactivate();
            
            // If any were pressed, reset them
            _lockButton->setDown(false);
            _backOut->setDown(false);
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
    updateNetworkOrder();
    updateTeammateIcons();
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

    if (selectedHouse.id == "Athena") {
        _playerIconImage->setTexture(_assets->get<cugl::graphics::Texture>("athenaSIcon"));
    } else {
        _playerIconImage->setTexture(_assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
    }

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

    for (int i = 0; i < (int)networkedPlayers.size(); i++) {
        _gameState->setRealPlayer(
            i,
            networkedPlayers[i].username,
            networkedPlayers[i].houseID
        );
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

    std::vector<std::shared_ptr<cugl::scene2::PolygonNode>> iconImages = {
        _rightPlayerIconImage, _upPlayerIconImage, _leftPlayerIconImage
    };

    for (int i = 1; i <= 3; i++) {
        int slot = (localIndex + i) % totalSlots;
        auto image = iconImages[i - 1];
        if (!image) continue;

        std::string house = players[slot]->getHouseName();
        if (house == "Athena") {
            image->setTexture(_assets->get<cugl::graphics::Texture>("athenaSIcon"));
        } else {
            image->setTexture(_assets->get<cugl::graphics::Texture>("playerIcon"));
        }
    }
}

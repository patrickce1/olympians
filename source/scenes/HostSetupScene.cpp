#include "HostSetupScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Role card width */
#define ROLE_CARD_WIDTH 251
/** Interpolation smoothing factor*/
#define SMOOTHING_FACTOR 0.2f
/** Finger movement dampening while dragging carousel */
#define SWIPE_DRAG_RESISTANCE 0.55f
/** Fraction of card width required to commit a swipe */
#define SWIPE_COMMIT_THRESHOLD 0.33f

#pragma mark -
#pragma mark Provided Methods
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
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool HostSetupScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up asset manager, network controller, and enemy loader
    _assets = assets;
    _network = networkController;
    loadBosses();
    
    Size dimen = getSize();
    
    // Acquire the scene built by the asset loader and resize it the scene
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("hostSetupScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    setupUI();
    setupListeners();
    
    _status = Status::WAIT;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the host setup UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * start button, back button, host name text field, carousel navigation
 * buttons, and the role carousel container. It also initializes the
 * carousel item list and configures the placeholder label.
 */
void HostSetupScene::setupUI() {
    _errorPopup = _assets->get<scene2::SceneNode>("hostSetupScene.errorPopup");
    if (_errorPopup) {
        auto overlay = std::dynamic_pointer_cast<scene2::PolygonNode>(
            _errorPopup->getChildByName("overlayBG"));
        overlay->setContentSize(getSize());
        overlay->setAnchor(Vec2::ANCHOR_CENTER);
        overlay->setPosition(getSize() / 2);
        _errorPopup->setVisible(false);
    }

    _startGame = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.start"));

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.back"));
    
    _joinButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.join"));

    _hostName = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("hostSetupScene.hostName.text"));

    _leftButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.bossCarousel.directionButtons.leftScroll"));

    _rightButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.bossCarousel.directionButtons.rightScroll"));

    _bossSelectionCardContainer = _assets->get<scene2::SceneNode>("hostSetupScene.bossCarousel.bossCardContainer");

    if (_bossSelectionCardContainer) {
        auto numCards = _bossSelectionCardContainer->getChildCount();
        for (int i = 0; i < numCards; i++) {
            _bossCards.push_back(_bossSelectionCardContainer->getChild(i));
        }
        _carouselBasePos = _bossSelectionCardContainer->getPosition();
        _carouselBaseIndex = _currentIndex;
    }

    std::shared_ptr<cugl::scene2::Label> placeName =
        std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>("hostSetupScene.hostName.placeholder"));

    placeName->setText("ENTER NAME");

    _hostName->addTypeListener([placeName](const std::string& name, const std::string& value) {
        placeName->setVisible(value.empty());
    });
    
    auto bossCarouselDotsContainer = _assets->get<scene2::SceneNode>("hostSetupScene.bossSelectionCarouselIcons");
    
    if (bossCarouselDotsContainer) {
        auto numDots = bossCarouselDotsContainer->getChildCount();
        for (int i = 0; i < numDots; i++) {
            _bossCarouselDotIndicators.push_back(bossCarouselDotsContainer->getChild(i));
        }
    }
}

/**
 * Attaches input listeners to the host setup buttons.
 *
 * This method assigns callbacks for starting the game, returning to the
 * previous menu, and navigating the role selection carousel.
 */
void HostSetupScene::setupListeners() {
    _startGame->addListener([this](const std::string& name, bool down) {
        if (down) {
            if(_hostName->getText() != ""){
                _network->hostRoom();
                _network->setPlayerName(_hostName->getText());
                
                // Get the selected boss using carousel index
                EnemyLoader::EnemyDef selectedBoss = _enemyLoader.getAllOrdered()[_currentIndex];
                _network->setEnemy(selectedBoss.id);
                _network->broadcastBossSelection(selectedBoss.id);
                
                _status = Status::START;
            }
        }
    });

    _backButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
    
    _joinButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::CLIENT;
            _joinButton->setDown(false);
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
void HostSetupScene::dispose() {
    if (_active) {
        removeAllChildren();
        _startGame = nullptr;
        _backButton = nullptr;
        _joinButton = nullptr;
        _hostName = nullptr;
        _bossCards.clear();
        _leftButton = nullptr;
        _rightButton = nullptr;
        _bossSelectionCardContainer = nullptr;
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
void HostSetupScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        auto touch = Input::get<Touchscreen>();
        
        if (value) {
            _status = WAIT;
            _activeTouch = -1;
             _isTouchDragging = false;
             
             _touchKey = touch->acquireKey(); //Get the key for the touch.
             //Add all listeners.
             //Detect touch
             touch->addBeginListener(_touchKey, [this](const TouchEvent& event, bool focus){
                 this->beginCarouselSwipe(event);
             });
             //Allow for the smooth movement
             touch->addMotionListener(_touchKey, [this](const TouchEvent& event, const Vec2& prev, bool focus){
                 this->updateCarouselSwipe(event);
             });
             touch->addEndListener(_touchKey, [this](const TouchEvent& event, bool focus){
                 this->endCarouselSwipe(event);
             });
             
            _isAnimating = false;
            Vec2 pos = _bossSelectionCardContainer->getPosition();
            float startX = getTargetXForIndex(_currentIndex);
            _bossSelectionCardContainer->setPosition(Vec2(startX, pos.y));
            _slideTarget = Vec2(startX, pos.y);
            updateCarouselDots(1);
            
            _startGame->activate();
            _leftButton->activate();
            _rightButton->activate();
            _hostName->activate();
            _backButton->activate();
            _joinButton->activate();
        } else {
            //Dispose of the listeners.
            touch->removeBeginListener(_touchKey);
            touch->removeMotionListener(_touchKey);
            touch->removeEndListener(_touchKey);
            
            _startGame->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backButton->deactivate();
            _hostName->deactivate();
            _joinButton->deactivate();
            
            // If any were pressed, reset them
            _startGame->setDown(false);
            _backButton->setDown(false);
            _leftButton->setDown(false);
            _rightButton->setDown(false);
            _joinButton->setDown(false);
        }
    }
}

/**
 * Begins tracking a swipe gesture for carousel drag.
 *
 * The gesture is ignored while snap animation is active.
 *
 * @param event  The touch begin event.
 */
void HostSetupScene::beginCarouselSwipe(const cugl::TouchEvent& event) {
   //Don't realize the swip if the carousel is getting in position or there is none.
    if (_isAnimating || !_bossSelectionCardContainer) {
        return;
    }
    //Register the touch and determine the origin of the card.
    _activeTouch = event.touch;
    _touchStartPos = event.position;
    _touchStartContainerPos = _bossSelectionCardContainer->getPosition();
    _isTouchDragging = true;
}

/**
 * Updates carousel x-position during an active swipe.
 *
 * Movement is damped to feel less slippery and clamped to endpoint anchors
 * so the user cannot drag past the first/last boss card.
 *
 * @param event  The touch motion event.
 */
void HostSetupScene::updateCarouselSwipe(const cugl::TouchEvent& event) {
    //Don't run if there isn't an active touch, we are already dragging, or animating.
    if (!_isTouchDragging || event.touch != _activeTouch || _isAnimating || !_bossSelectionCardContainer) {
        return;
    }
    //Difference in x between current finger loaction and the start.
    const float rawDx = event.position.x - _touchStartPos.x;
    //Dampened to not feel slippery.
    const float dx = rawDx * SWIPE_DRAG_RESISTANCE;
    //
    const int lastIndex = (int)_bossCards.size() - 1;
    if (lastIndex < 0) {
        return;
    }

    float newX = _touchStartContainerPos.x + dx;

    Vec2 pos = _bossSelectionCardContainer->getPosition();
    _bossSelectionCardContainer->setPosition(Vec2(newX, pos.y));
}

/**
 * Finishes swipe tracking and resolves to a snapped card index.
 *
 * @param event  The touch end event.
 */
void HostSetupScene::endCarouselSwipe(const cugl::TouchEvent& event) {
    if (event.touch != _activeTouch) {
        return;
    }

    if (_isTouchDragging) {
        snapToNearestIndex();
    }

    //End the touch
    _isTouchDragging = false;
    _activeTouch = -1;
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
void HostSetupScene::updateText(const std::shared_ptr<scene2::Button>& button, const std::string text) {
    auto label = std::dynamic_pointer_cast<scene2::Label>(button->getChildByName("up")->getChildByName("label"));
    label->setText(text);
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void HostSetupScene::update(float timestep) {
    // Auto-dismiss the error popup after ERROR_DISPLAY_TIME seconds.
    if (_errorPopup && _errorPopup->isVisible()) {
        _errorTimer += timestep;
        if (_errorTimer >= ERROR_DISPLAY_TIME) {
            _errorPopup->setVisible(false);
            _errorTimer = 0.0f;
        }
    }
    
    if (_isAnimating) {
        Vec2 current = _bossSelectionCardContainer->getPosition();
        Vec2 next = current.lerp(_slideTarget, SMOOTHING_FACTOR); // 0.2 = smoothing factor

        if (current.distance(_slideTarget) < 1.0f) {
            _bossSelectionCardContainer->setPosition(_slideTarget);
            _isAnimating = false;
        } else {
            _bossSelectionCardContainer->setPosition(next);
        }
    }
}

/**
 * Reconfigures the start button for this scene
 *
 * This is necessary because what the buttons do depends on the state of the
 * networking.
 */
void HostSetupScene::configureStartButton() {
    updateText(_startGame,"Start Game");
    _startGame->activate();
}

/**
 * Initiates a slide animation to center the item at `newIndex`.
 *
 * Does nothing if an animation is already in progress or if the
 * index is out of bounds. Otherwise computes the target container
 * position and stores it in `_slideTarget`.
 *
 * @param newIndex The index of the item to slide to.
 */
void HostSetupScene::slideTo(int newIndex) {
    if (_isAnimating) return;
    if (newIndex < 0 || newIndex >= _bossCards.size()) return;

    _isAnimating = true;

    Vec2 currentPos = _bossSelectionCardContainer->getPosition();
    
    _slideTarget = Vec2(getTargetXForIndex(newIndex), currentPos.y);
    _currentIndex = newIndex;
    
    // Set the visibility of all glow overlays to false and the currentIndex card's to true
    for (int i = 0; i < _bossCards.size(); i++) {
        auto card = _bossCards[i];
        if (card) {
            auto glow = card->getChildByName("glowOverlay");
            if (glow){
                glow->setVisible(false);
                if (i == newIndex) {
                    glow->setVisible(true);
                }
            }
        }
    }

    updateCarouselDots(newIndex);
}

/**
 * Returns the absolute target x-position for the given card index.
 *
 * @param index  The card index in the carousel.
 *
 * @return the absolute x-position anchor for that index.
 */
float HostSetupScene::getTargetXForIndex(int index) const {
    //converting an index into an absolute x position.
    float carouselXAnchor = _carouselBasePos.x;
    int stepsFromBase = index - _carouselBaseIndex;
    //pixel offset from the anchor card to the target card
    float pixelOffset = (stepsFromBase * ROLE_CARD_WIDTH);
    return (carouselXAnchor - pixelOffset) + (ROLE_CARD_WIDTH / 2.0f);
}

/**
 * Resolves swipe result to a discrete selection.
 *
 * Small drags snap back to current index. Drags past threshold commit one
 * step in swipe direction and are clamped to valid index range.
 */
void HostSetupScene::snapToNearestIndex() {
    if (!_bossSelectionCardContainer || _bossCards.empty()) {
        return;
    }
    
    const float currentX = _bossSelectionCardContainer->getPosition().x;
    const float anchorX = getTargetXForIndex(_currentIndex);
    //Distance of current x from base of the carousel.
    const float delta = currentX - anchorX;
    const float threshold = ROLE_CARD_WIDTH * SWIPE_COMMIT_THRESHOLD;

    int target = _currentIndex;
    if (std::abs(delta) >= threshold) {
        // Right drag (delta > 0) should move to previous card; left drag to next.
        if (delta > 0){
            target = (_currentIndex - 1);
        }
        else {
            target = (_currentIndex + 1);
        }
        //Stay in bounds
        if (target < 0){
            target = 0;
        }
        
        int last = (int)_bossCards.size() - 1;
        if (target > last){
            target = last;
        }
    }

    slideTo(target);
}

/**
 * Updates the circular indicators at the bottom of what card in the carousel
 * we are currently at.
 *
 * @param currentIndex The index of the card we are at.
 */
void HostSetupScene::updateCarouselDots(int currentIndex) {
    for (int i = 0; i < _bossCarouselDotIndicators.size(); i++) {
        auto node = _bossCarouselDotIndicators[i];
        
        auto fill   = node->getChildByName("fill");
        
        if (i == currentIndex) {
            fill->setColor(Color4("#9d7137ff"));
        } else {
            fill->setColor(Color4("#4c3214ff"));
        }
    }
}

/** Loads boss definitions from the enemies JSON to use in selection. */
bool HostSetupScene::loadBosses() {
    // Load animation registry first so state durations can be calculated
    if (!_enemyLoader.loadAnimationRegistry(_assets)) {
        CULog("HostSetupScene: Failed to load animation registry from enemyAnimations.json");
        // Non-fatal - continue anyway
    }
    
    const std::string enemiesJsonPath = "json/enemies.json";
    if (!_enemyLoader.loadFromFile(enemiesJsonPath)) {
        CULog("HostSetupScene: Failed to load enemies.json");
        return false;
    }
    return true;
}

/**
 * Shows the "Host disconnected" error popup.
 *
 * Mirrors the showError() pattern from ClientScene. Sets the errorLabel
 * text, makes the popup visible, and resets the auto-dismiss timer so
 * update() will hide it after ERROR_DISPLAY_TIME seconds.
 */
void HostSetupScene::showHostDisconnectedError() {
    if (_errorPopup) {
        auto label = std::dynamic_pointer_cast<scene2::Label>(
            _errorPopup->getChildByName("errorLabel"));
        if (label) {
            label->setText("Host disconnected.\nReturning to Quest Select");
        }
        _errorPopup->setVisible(true);
        _errorTimer = 0.0f;
    }
}

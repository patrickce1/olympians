#include "BossSelectScene.h"

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
 * Initializes the boss selection scene.
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
 *
 * @return true if the scene was successfully initialized; false otherwise
 */
bool BossSelectScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
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
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("bossSelectScene");
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
 * Retrieves and stores references to the BossSelectScene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * lock button, back button, carousel navigation
 * buttons, and the role carousel container. It also initializes the
 * carousel item list.
 */
void BossSelectScene::setupUI() {

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("bossSelectScene.back"));
    
    _lockButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("bossSelectScene.lock"));

    _leftButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("bossSelectScene.bossCarousel.directionButtons.leftScroll"));

    _rightButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("bossSelectScene.bossCarousel.directionButtons.rightScroll"));

    _bossSelectionCardContainer = _assets->get<scene2::SceneNode>("bossSelectScene.bossCarousel.bossCardContainer");

    if (_bossSelectionCardContainer) {
        auto numCards = _bossSelectionCardContainer->getChildCount();
        for (int i = 0; i < numCards; i++) {
            _bossCards.push_back(_bossSelectionCardContainer->getChild(i));
        }
//        _carouselBaseIndex = _currentIndex;
        _baseCarouselPosition = _bossSelectionCardContainer->getPosition();
    }
    
    auto bossCarouselDotsContainer = _assets->get<scene2::SceneNode>("bossSelectScene.bossSelectionCarouselIcons");
    
    if (bossCarouselDotsContainer) {
        auto numDots = bossCarouselDotsContainer->getChildCount();
        for (int i = 0; i < numDots; i++) {
            _bossCarouselDotIndicators.push_back(bossCarouselDotsContainer->getChild(i));
        }
    }
}

/**
 * Attaches input listeners to the boss select buttons.
 *
 * This method assigns callbacks for starting the game, returning to the
 * previous menu, and navigating the role selection carousel.
 */
void BossSelectScene::setupListeners() {
    
    _backButton->addListener([this](const std::string& name, bool down) {
        if (!down) {
            _status = Status::ABORT;
        }
    });
    
    _lockButton->addListener([this](const std::string& name, bool down) {
        if (!down) {
            EnemyLoader::EnemyDef selectedBoss = _enemyLoader.getAllOrdered()[_currentIndex];
            _network->setEnemy(selectedBoss.id);
            _network->broadcastBossSelection(selectedBoss.id);
            
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
void BossSelectScene::dispose() {
    if (_active) {
        removeAllChildren();
        _backButton = nullptr;
        _lockButton = nullptr;
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
void BossSelectScene::setActive(bool value) {
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
            float startX = _baseCarouselPosition.x + (ROLE_CARD_WIDTH / 2.0f);
            _bossSelectionCardContainer->setPosition(Vec2(startX, pos.y));
            _slideTarget = Vec2(startX, pos.y);
            updateCarouselDots(1);
            
            _leftButton->activate();
            _rightButton->activate();
            _backButton->activate();
            configureLockButton();
        } else {
            //Dispose of the listeners.
            touch->removeBeginListener(_touchKey);
            touch->removeMotionListener(_touchKey);
            touch->removeEndListener(_touchKey);
            
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backButton->deactivate();
            _lockButton->deactivate();
            
            // If any were pressed, reset them
            _backButton->setDown(false);
            _leftButton->setDown(false);
            _rightButton->setDown(false);
            _lockButton->setDown(false);
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
void BossSelectScene::beginCarouselSwipe(const cugl::TouchEvent& event) {
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
void BossSelectScene::updateCarouselSwipe(const cugl::TouchEvent& event) {
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
void BossSelectScene::endCarouselSwipe(const cugl::TouchEvent& event) {
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
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void BossSelectScene::update(float timestep) {
    // Kick client if host terminated the session
    if (!_network->isHost()) {
        if (_network->checkConnection() != NetworkController::Status::CONNECTED) {
            _status = Status::ABORT;
            return;
        }
        _network->getNetworkUpdates();
        if (_network->wasSessionTerminated()) {
            _network->clearQueues();
            _network->disconnect();
            _status = Status::ABORT;
            return;
        }
    }
    
    // forward to game scene if host started while we were here
    if (_network->checkGameStarted()) {
        _network->clearQueues();
        _status = Status::GAMESCENE_START;
        return;
    }
    
    if (_isAnimating) {
        Vec2 bossCardContainerPos = _bossSelectionCardContainer->getPosition();
        Vec2 interpolatedPos = bossCardContainerPos.lerp(_slideTarget, SMOOTHING_FACTOR); // 0.2 = smoothing factor

        if (bossCardContainerPos.distance(_slideTarget) < 1.0f) {
            _bossSelectionCardContainer->setPosition(_slideTarget);
            _isAnimating = false;
        } else {
            _bossSelectionCardContainer->setPosition(interpolatedPos);
        }
    }
}

/**
 * Reconfigures the lock button for this scene
 *
 * This is necessary because what the buttons do depends on the state of the
 * networking.
 */
void BossSelectScene::configureLockButton() {
    if (_network->isHost()) {
        _lockButton->activate();
        _lockButton->SceneNode::setColor(Color4::WHITE);
    } else {
        _lockButton->deactivate();
        _lockButton->SceneNode::setColor(Color4(255, 255, 255, 125));
    }
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
void BossSelectScene::slideTo(int newIndex) {
    if (_isAnimating) return;
    if (newIndex < 0 || newIndex >= _bossCards.size()) return;

    _isAnimating = true;

    Vec2 currentPos = _bossSelectionCardContainer->getPosition();
    
    //Set where the current slide should take us.
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
float BossSelectScene::getTargetXForIndex(int index) const {
    //converting an index into an absolute x position.
    float carouselXAnchor = _baseCarouselPosition.x;
    int stepsFromBase = index - _carouselBaseIndex;
    //pixel offset from the anchor card to the target card
    float pixelOffset = (stepsFromBase * ROLE_CARD_WIDTH);
    return (carouselXAnchor - pixelOffset) + (ROLE_CARD_WIDTH / 2.0f);}

/**
 * Resolves swipe result to a discrete selection.
 *
 * Small drags snap back to current index. Drags past threshold commit one
 * step in swipe direction and are clamped to valid index range.
 */
void BossSelectScene::snapToNearestIndex() {
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
void BossSelectScene::updateCarouselDots(int currentIndex) {
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
bool BossSelectScene::loadBosses() {
    // Load animation registry first so state durations can be calculated
    if (!_enemyLoader.loadAnimationRegistry(_assets)) {
        CULog("BossSelectScene: Failed to load animation registry from enemyAnimations.json");
        // Non-fatal - continue anyway
    }
    
    const std::string enemiesJsonPath = "json/enemies.json";
    if (!_enemyLoader.loadFromFile(enemiesJsonPath)) {
        CULog("BossSelectScene: Failed to load enemies.json");
        return false;
    }
    return true;
}


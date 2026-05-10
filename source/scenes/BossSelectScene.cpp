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
/** Minimum number of frames for a swipe to be registered*/
static constexpr int SWIPE_HOLD_FRAMES = 4;

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
        _baseCarouselPosition = _bossSelectionCardContainer->getPosition();
    }
    
    auto bossCarouselDotsContainer = _assets->get<scene2::SceneNode>("bossSelectScene.bossSelectionCarouselIcons");
    
    if (bossCarouselDotsContainer) {
        auto numDots = bossCarouselDotsContainer->getChildCount();
        for (int i = 0; i < numDots; i++) {
            _bossCarouselDotIndicators.push_back(bossCarouselDotsContainer->getChild(i));
        }
    }
    
    CULog("=== Boss Cards ===");
    for (int i = 0; i < (int)_bossCards.size(); i++) {
        CULog("Card %d (%s): localX=%.1f", i, _bossCards[i]->getName().c_str(), _bossCards[i]->getPosition().x);
    }
    
    float startX = _baseCarouselPosition.x + (ROLE_CARD_WIDTH / 2.0f);
    _xPosToBoss[startX] = 0;
    _bossToTargetX[0]= startX;
    for (int i = 1; i < (int)_bossCards.size(); i++) {
        float containerX     = startX - (i * ROLE_CARD_WIDTH);
        _xPosToBoss[containerX] = i;
        _bossToTargetX[i]       = containerX;
        CULog("Card %d (%s): containerX=%.1f",
              i, _bossCards[i]->getName().c_str(), containerX);
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
        if (down) {
            _status = Status::ABORT;
        }
    });
    
    _lockButton->addListener([this](const std::string& name, bool down) {
        if (down) {
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
        if (value) {
            _isSwiping = false;
            _status = WAIT;
            _currentIndex = 1;
            _isAnimating = false;
            Vec2 pos = _bossSelectionCardContainer->getPosition();
            float startX = _baseCarouselPosition.x + (ROLE_CARD_WIDTH / 2.0f);
            CULog("setActive startX=%.1f  _baseCarouselPosition.x=%.1f", startX, _baseCarouselPosition.x);
            _bossSelectionCardContainer->setPosition(Vec2(startX, pos.y));
            _slideTarget = Vec2(startX, pos.y);
            updateCarouselDots(1);
            _swipeTouchInitialPos = cugl::Vec2::ZERO;
             _swipeContainerStartX = 0.0f;
            _swipeHoldFrames = 0;
            
            _leftButton->setVisible(true);
            _rightButton->setVisible(true);
            _leftButton->activate();
            _rightButton->activate();
            _backButton->activate();
            configureLockButton();
        } else {
            _isSwiping = false;
            _swipeContainerStartX = 0.0f;
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
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 * @param input         The input controller instance
 */
void BossSelectScene::update(float timestep, InputController& input) {
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
    
    // Forward to pre game scene if host started while we were here
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
        return;
    }
    
    // Process swipe gestures in three phases every frame so that begin,
    // tracking, and release are never skipped within the same update cycle.
    handleSwipeBegin(input);
    handleSwipeTracking(input);
    handleSwipeRelease(input);
    
    if (_isAnimating) {
        Vec2 bossCardContainerPos = _bossSelectionCardContainer->getPosition();
        Vec2 interpolatedPos = bossCardContainerPos.lerp(_slideTarget, SMOOTHING_FACTOR); // 0.2 = smoothing factor

        // Compare only X distance since we only slide horizontally.
        float xDist = std::abs(bossCardContainerPos.x - _slideTarget.x);
        if (xDist < 1.0f) {
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
        _lockButton->setVisible(true);
    } else {
        _lockButton->deactivate();
        _lockButton->setVisible(false);
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

    float shiftAmount = ROLE_CARD_WIDTH;
    
    int deltaIndex = newIndex - _currentIndex;
    Vec2 currentPos = _bossSelectionCardContainer->getPosition();
    float targetX = currentPos.x - (deltaIndex * shiftAmount);
    
    _slideTarget = Vec2(targetX, currentPos.y);
    _currentIndex = newIndex;
    
    if (_currentIndex == 0) {
        _leftButton->setVisible(false);
    } else if (_currentIndex == _bossCards.size() - 1) {
        _rightButton->setVisible(false);
    } else {
        _rightButton->setVisible(true);
        _leftButton->setVisible(true);
    }
    
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
            fill->setColor(Color4("#4c3214ff"));
        } else {
            fill->setColor(Color4("#9d7137ff"));
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

#pragma mark -
#pragma mark Swipe Gesture Handling

/**
 * Records the touch-down position to begin tracking a swipe gesture.
 *
 * Captures both the finger's starting X and the container's current X
 * so handleSwipeTracking() can offset from both without drift.
 *
 * @param input  The input controller for this frame.
 */
void BossSelectScene::handleSwipeBegin(InputController& input) {
    if (_isSwiping) return;

    if ((!input.isTouching() && !input.isMouseDown()) || _isAnimating) {
        _swipeHoldFrames      = 0;
        _swipeTouchInitialPos = cugl::Vec2::ZERO;
        return;
    }

    if (_swipeHoldFrames == 0) {
        _swipeTouchInitialPos = input.getTouchStart();
    }

    // Convert to world space for consistent coordinate comparison.
    Vec2 worldCurrent = screenToWorldCoords(input.getDragPos());
    Vec2 worldStart   = screenToWorldCoords(_swipeTouchInitialPos);

    float horizontalDelta = std::abs(worldCurrent.x - worldStart.x);
    float verticalDelta   = std::abs(worldCurrent.y - worldStart.y);

    if (horizontalDelta > verticalDelta && horizontalDelta > 5.0f) {
        _swipeHoldFrames++;
    } else {
        _swipeHoldFrames = 0;
    }

    if (_swipeHoldFrames >= SWIPE_HOLD_FRAMES) {
        // Store the touch start X in screen space — tracking converts it
        // to world space each frame so the delta stays correct.
        _swipeTouchStartX     = screenToWorldCoords(_swipeTouchInitialPos).x;
        _swipeContainerStartX = _bossSelectionCardContainer->getPosition().x;
        _isSwiping            = true;
        _swipeHoldFrames      = 0;
    }
}

/**
 * Moves the card container directly under the finger each frame.
 *
 * Computes the horizontal delta between the finger's current position
 * and its touch-down position, then applies that delta to the
 * container's position at the start of the drag. This keeps the strip
 * locked exactly to the finger with no smoothing or lag.
 *
 * Clamps the container so it cannot travel past the first or last card,
 * preventing empty space from appearing at either end.
 *
 * No-op when no swipe is active or no touch contact exists this frame.
 *
 * @param input  The input controller for this frame.
 */
void BossSelectScene::handleSwipeTracking(InputController& input) {
    if (!_isSwiping) return;
    if (!input.isTouching() && !input.isMouseDown()) return;

    // Both _swipeTouchStartX and getDragPos() are now in world space,
    // so the delta is correct without any further coordinate conversion.
    Vec2 worldPos     = screenToWorldCoords(input.getDragPos());
    float fingerDelta = worldPos.x - _swipeTouchStartX;
    float rawX        = _swipeContainerStartX + fingerDelta;

    float maxX     = _bossToTargetX[0] + ROLE_CARD_WIDTH * 2.0f;
    float minX     = _bossToTargetX[(int)_bossCards.size() - 1] + ROLE_CARD_WIDTH;
    float clampedX = std::max(minX, std::min(maxX, rawX));

    Vec2 pos = _bossSelectionCardContainer->getPosition();
    _bossSelectionCardContainer->setPosition(Vec2(clampedX, pos.y));
}

/**
 * Called on finger lift. Reads the container's current X and delegates
 * to snapToNearestBoss() to animate to the closest card.
 * Clears all swipe tracking state before returning.
 *
 * @param input  The input controller for this frame.
 */
void BossSelectScene::handleSwipeRelease(InputController& input) {
    if (!input.touchEnded()) return;

    if (!_isSwiping) {
        return;
    }

    float releaseContainerX = _bossSelectionCardContainer->getPosition().x;
    CULog("handleSwipeRelease: containerX=%.1f", releaseContainerX);
    snapToNearestBoss(releaseContainerX);

    _isSwiping            = false;
    _swipeTouchStartX     = 0.0f;
    _swipeContainerStartX = 0.0f;
}

/**
 * Finds the card whose X position in _xPosToBoss is closest to the
 * given container X, then animates the container to that card's centred
 * position and updates _currentIndex, glow overlays, and dot indicators.
 *
 * The container's current X is compared against each key in _xPosToBoss
 * (which stores each card's raw local X). The closest key wins. The
 * container's target position is then computed as the offset needed to
 * bring that card's local X to the centre of the viewport.
 *
 * @param releaseContainerX  The container's X at the moment the finger
 *                           lifted, in the container parent's local space.
 */
void BossSelectScene::snapToNearestBoss(float releaseContainerX) {
    CULog("=== snapToNearestBoss ===");
    CULog("releaseContainerX=%.1f", releaseContainerX);
    CULog("_bossToTargetX[0] (Circe)    = %.1f", _bossToTargetX[0]);
    CULog("_bossToTargetX[1] (Cyclops)  = %.1f", _bossToTargetX[1]);
    CULog("_bossToTargetX[2] (Cerberus) = %.1f", _bossToTargetX[2]);
    CULog("_bossToTargetX[3] (Gaia)     = %.1f", _bossToTargetX[3]);
    CULog("maxX (clamp) = %.1f", _bossToTargetX[0]);
    CULog("minX (clamp) = %.1f", _bossToTargetX[(int)_bossCards.size() - 1]);

    for (auto& [containerX, index] : _xPosToBoss) {
        CULog("  _xPosToBoss key=%.1f -> index=%d  dist=%.1f",
              containerX, index, std::abs(releaseContainerX - containerX));
    }
    int   nearestIndex = 0;
    float nearestDist  = FLT_MAX;

    for (auto& [containerX, index] : _xPosToBoss) {
        float dist = std::abs(releaseContainerX - containerX);
        if (dist < nearestDist) {
            nearestDist  = dist;
            nearestIndex = index;
        }
    }

    _currentIndex = nearestIndex;
    _isAnimating  = true;
    
    CULog(" Index swiped to index=%d ", nearestIndex);

    Vec2 currentPos = _bossSelectionCardContainer->getPosition();
    _slideTarget    = Vec2(_bossToTargetX[nearestIndex], currentPos.y);

    _leftButton->setVisible(_currentIndex > 0);
    _rightButton->setVisible(_currentIndex < (int)_bossCards.size() - 1);

    for (int i = 0; i < (int)_bossCards.size(); i++) {
        auto glow = _bossCards[i]->getChildByName("glowOverlay");
        if (glow) glow->setVisible(i == nearestIndex);
    }

    updateCarouselDots(nearestIndex);
}

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
/** Minimum number of frames for a swipe to be registered*/
static constexpr int SWIPE_HOLD_FRAMES = 4;

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
        _errorPopup->setVisible(false);
    }

    _startGame = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.start"));

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.back"));
    
    _joinButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.join"));
    
    _settingsButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("hostSetupScene.settingsTab"));

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
        _baseCarouselPosition = _bossSelectionCardContainer->getPosition();
    }
    
    auto bossCarouselDotsContainer = _assets->get<scene2::SceneNode>("hostSetupScene.bossSelectionCarouselIcons");
    
    if (bossCarouselDotsContainer) {
        auto numDots = bossCarouselDotsContainer->getChildCount();
        for (int i = 0; i < numDots; i++) {
            _bossCarouselDotIndicators.push_back(bossCarouselDotsContainer->getChild(i));
        }
    }
    
    // Manual setup for Card Mappings to Boss Indexes
    float startX = _baseCarouselPosition.x + (ROLE_CARD_WIDTH / 2.0f);
    _xPosToBoss[startX] = 1;
    _bossToTargetX[1]   = startX;
    _xPosToBoss[startX + ROLE_CARD_WIDTH] = 0;
    _bossToTargetX[0]   = startX + ROLE_CARD_WIDTH;
    _xPosToBoss[startX - ROLE_CARD_WIDTH] = 2;
    _bossToTargetX[2]   = startX - ROLE_CARD_WIDTH;
    _xPosToBoss[startX - (2 * ROLE_CARD_WIDTH)] = 3;
    _bossToTargetX[3]   = startX - (2 * ROLE_CARD_WIDTH);
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
            const std::string savedName = SavedDataManager::get().getPlayerName();
            if (!savedName.empty()) {
                _network->hostRoom();
                _network->setPlayerName(savedName);
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
    
    _settingsButton->addListener([this](const std::string& name, bool down) {
        if (!down) _pendingSettings = true;
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
        _bossCards.clear();
        _leftButton = nullptr;
        _rightButton = nullptr;
        _settingsButton = nullptr;
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
        if (value) {
            _status = WAIT;
            _isAnimating = false;
            _isSwiping            = false;
            _swipeContainerStartX = 0.0f;
            _swipeTouchInitialPos = cugl::Vec2::ZERO;
            _swipeHoldFrames      = 0;
            Vec2 pos = _bossSelectionCardContainer->getPosition();
            float startX = _baseCarouselPosition.x + (ROLE_CARD_WIDTH / 2.0f);;
            if (!SavedDataManager::get().getTutorialCompleted()){
                _currentIndex = 3;
                startX -= 2.0f * (ROLE_CARD_WIDTH);
                updateCarouselDots(3);
            } else {
                _currentIndex = 1;
                updateCarouselDots(1);
            }
            _bossSelectionCardContainer->setPosition(Vec2(startX, pos.y));
            _slideTarget = Vec2(startX, pos.y);
            _tutorialSlideDelay = !SavedDataManager::get().getTutorialCompleted() ? 35 : 0;
            
            // Reset all glow overlays and illuminate only the starting card (index 1).
            for (int i = 0; i < (int)_bossCards.size(); i++) {
                auto glow = _bossCards[i]->getChildByName("glowOverlay");
                if (glow) glow->setVisible(i == _currentIndex);
            }
            
            _leftButton->setVisible(true);
            _rightButton->setVisible(_currentIndex!=3);
            _startGame->activate();
            _leftButton->activate();
            _rightButton->activate();
            _backButton->activate();
            _joinButton->activate();
            _settingsButton->activate();
            updateTutorialLocks();
    
            //Tutorial completion pop up
            if (_pendingTutorialCompletePopup) {
                _pendingTutorialCompletePopup = false;
                if (_errorPopup) {
                    auto label = std::dynamic_pointer_cast<scene2::Label>(
                        _errorPopup->getChildByName("errorLabel"));
                    if (label) label->setText("Tutorial can be replayed in settings");
                    _errorPopup->setVisible(true);
                    _errorTimer = 0.0f;
                }
            }
        } else {
            _isSwiping            = false;
            _swipeContainerStartX = 0.0f;
            _startGame->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backButton->deactivate();
            _joinButton->deactivate();
            _settingsButton->deactivate();
            
            // If any were pressed, reset them
            _startGame->setDown(false);
            _backButton->setDown(false);
            _leftButton->setDown(false);
            _rightButton->setDown(false);
            _joinButton->setDown(false);
            _settingsButton->setDown(false);
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
 * @param input         The input controller instance
 */
void HostSetupScene::update(float timestep, InputController& input) {
    //Forced Tutorial Start from settings
    if (_pendingTutorialStart) {
        _pendingTutorialStart = false;
        slideTo(0); // Circe
        //Start game and set the bots' houses
        _startGame->setDown(true);
        _startGame->setDown(false);
    }
    
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
    handleSwipeBegin(input);
    handleSwipeTracking(input);
    handleSwipeRelease(input);
    updateTutorialLocks();
    
    if (!SavedDataManager::get().getTutorialCompleted() && (_currentIndex != 0 && !_isAnimating)){
        if (_tutorialSlideDelay > 0) {
            _tutorialSlideDelay--;
        } else {
            slideTo(_currentIndex - 1);
        }
    }
}

/**
 * Returns true if the user has requested to open settings, then resets the flag.
 */
bool HostSetupScene::shouldOpenSettings() {
    bool ifPendingSettings = _pendingSettings;
    _pendingSettings = false;
    return ifPendingSettings;
};

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
void HostSetupScene::updateCarouselDots(int currentIndex) {
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
 * Enables or disables all interactive input controls.
 *
 * Called with false when a join attempt starts so the player cannot spam
 * the button, and called with true when the scene resets to IDLE.
 *
 * @param enabled  Whether the controls should accept input.
 */
void HostSetupScene::setInputEnabled(bool enabled) {
    if (enabled) {
        _startGame->activate();
        _leftButton->activate();
        _rightButton->activate();
        _backButton->activate();
        _joinButton->activate();
        _settingsButton->activate();
    } else {
        _startGame->deactivate();
        _leftButton->deactivate();
        _rightButton->deactivate();
        _backButton->deactivate();
        _joinButton->deactivate();
        _settingsButton->deactivate();
    }
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
void HostSetupScene::handleSwipeBegin(InputController& input) {
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
void HostSetupScene::handleSwipeTracking(InputController& input) {
    if (!_isSwiping) return;
    if (!input.isTouching() && !input.isMouseDown()) return;

    // Both _swipeTouchStartX and getDragPos() are now in world space,
    // so the delta is correct without any further coordinate conversion.
    Vec2 worldPos     = screenToWorldCoords(input.getDragPos());
    float fingerDelta = worldPos.x - _swipeTouchStartX;
    float rawX        = _swipeContainerStartX + fingerDelta;
    float maxX     = _bossToTargetX[0] + (ROLE_CARD_WIDTH * 2.0f);
    float minX     = _bossToTargetX[(int)_bossCards.size() - 1] - (ROLE_CARD_WIDTH);
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
void HostSetupScene::handleSwipeRelease(InputController& input) {
    if (!input.touchEnded()) return;

    if (!_isSwiping) {
        return;
    }

    float releaseContainerX = _bossSelectionCardContainer->getPosition().x;
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
void HostSetupScene::snapToNearestBoss(float releaseContainerX) {
    int nearestIndex = 0;
    float nearestDist  = FLT_MAX;

    for (auto& [containerX, index] : _xPosToBoss) {
        float dist = std::abs(releaseContainerX - containerX);
        if (dist < nearestDist) {
            nearestDist  = dist;
            nearestIndex = index;
        }
    }

    _currentIndex   = nearestIndex;
    _isAnimating    = true;
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

/**
 * Applies lock visuals to every non-Circe card when the tutorial has not
 * yet been completed, and removes those visuals once it has. Also disables
 * the START button when the carousel is resting on a locked card so the
 * player cannot launch a boss they shouldn't access yet.
 *
 * Called from setActive(true) and every frame in update() so the state
 * stays in sync if tutorialCompleted changes mid-session.
 */
void HostSetupScene::updateTutorialLocks() {
    bool tutorialDone = SavedDataManager::get().getTutorialCompleted();

    const auto& allBosses = _enemyLoader.getAllOrdered();
    for (int i = 0; i < (int)_bossCards.size(); i++) {
        auto card = _bossCards[i];
        if (!card) continue;

        bool isCirce = (i < (int)allBosses.size() && allBosses[i].id == "circe");
        bool locked  = !tutorialDone && !isCirce;

        auto overlay = card->getChildByName("lockedOverlay");
        if (overlay) overlay->setVisible(locked);
        
        if (locked) {
            card->setColor(cugl::Color4(80, 80, 80, 255));
        } else {
            card->setColor(cugl::Color4::WHITE);
        }
    }

    // Disable START when the current card is tutorial-locked
    bool currentIsCirce = (_currentIndex < (int)allBosses.size() &&
                           allBosses[_currentIndex].id == "circe");
    bool currentLocked  = !tutorialDone && !currentIsCirce;

    if (currentLocked) {
        _startGame->deactivate();
    } else if (isActive()) {
        _startGame->activate();
    }
}

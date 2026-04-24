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
        _baseCarouselPosition = _bossSelectionCardContainer->getPosition();
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
        if (value) {
            _status = WAIT;
            
            _currentIndex = 1;
            _isAnimating = false;
            Vec2 pos = _bossSelectionCardContainer->getPosition();
            float startX = _baseCarouselPosition.x + (ROLE_CARD_WIDTH / 2.0f);
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
    if (newIndex < 0 || newIndex >= (int)_bossCards.size()) return;

    _isAnimating = true;

    float shiftAmount = ROLE_CARD_WIDTH;
    
    int deltaIndex = newIndex - _currentIndex;
    Vec2 currentPos = _bossSelectionCardContainer->getPosition();
    float targetX = currentPos.x - (deltaIndex * shiftAmount);
    
    _slideTarget = Vec2(targetX, currentPos.y);
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

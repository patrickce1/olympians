#include "CodexScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852


#pragma mark -
#pragma mark Provided Methods

/**
 * Initializes the codex scene.
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
bool CodexScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up asset manager, network controller, and enemy loader
    _assets = assets;
    _network = networkController;
    
    Size dimen = getSize();
    
    // Acquire the scene built by the asset loader and resize it the scene
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("codexScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

//    setupUI();
//    setupListeners();
    
    _status = Status::WAIT;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the CodexScene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * item buttons, back button, navigation
 * buttons, and the item container. It also initializes the
 * grid item list.
 */
void CodexScene::setupUI() {

//    _backButton = std::dynamic_pointer_cast<scene2::Button>(
//        _assets->get<scene2::SceneNode>("bossSelectScene.back"));
//    
//    _lockButton = std::dynamic_pointer_cast<scene2::Button>(
//        _assets->get<scene2::SceneNode>("bossSelectScene.lock"));
//
//    _leftButton = std::dynamic_pointer_cast<scene2::Button>(
//        _assets->get<scene2::SceneNode>("bossSelectScene.bossCarousel.directionButtons.leftScroll"));
//
//    _rightButton = std::dynamic_pointer_cast<scene2::Button>(
//        _assets->get<scene2::SceneNode>("bossSelectScene.bossCarousel.directionButtons.rightScroll"));
//
//    _bossSelectionCardContainer = _assets->get<scene2::SceneNode>("bossSelectScene.bossCarousel.bossCardContainer");
//
//    if (_bossSelectionCardContainer) {
//        auto numCards = _bossSelectionCardContainer->getChildCount();
//        for (int i = 0; i < numCards; i++) {
//            _bossCards.push_back(_bossSelectionCardContainer->getChild(i));
//        }
//        _baseCarouselPosition = _bossSelectionCardContainer->getPosition();
//    }
//    
//    auto bossCarouselDotsContainer = _assets->get<scene2::SceneNode>("bossSelectScene.bossSelectionCarouselIcons");
//    
//    if (bossCarouselDotsContainer) {
//        auto numDots = bossCarouselDotsContainer->getChildCount();
//        for (int i = 0; i < numDots; i++) {
//            _bossCarouselDotIndicators.push_back(bossCarouselDotsContainer->getChild(i));
//        }
//    }
}

/**
 * Attaches input listeners to the codex buttons.
 */
void CodexScene::setupListeners() {
    
//    _backButton->addListener([this](const std::string& name, bool down) {
//        if (down) {
//            _status = Status::ABORT;
//        }
//    });
//    
//    _lockButton->addListener([this](const std::string& name, bool down) {
//        if (down) {
//            EnemyLoader::EnemyDef selectedBoss = _enemyLoader.getAllOrdered()[_currentIndex];
//            _network->setEnemy(selectedBoss.id);
//            _network->broadcastBossSelection(selectedBoss.id);
//            
//            _status = Status::ABORT;
//        }
//    });
//
//    _leftButton->addListener([this](const std::string& name, bool down){
//        if (!down) slideTo(_currentIndex - 1);
//    });
//
//    _rightButton->addListener([this](const std::string& name, bool down){
//        if (!down) slideTo(_currentIndex + 1);
//    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void CodexScene::dispose() {
    if (_active) {
//        removeAllChildren();
//        _backButton = nullptr;
//        _lockButton = nullptr;
//        _bossCards.clear();
//        _leftButton = nullptr;
//        _rightButton = nullptr;
//        _bossSelectionCardContainer = nullptr;
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
void CodexScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = WAIT;
            
//            _leftButton->activate();
//            _rightButton->activate();
//            _backButton->activate();
        } else {
//            _leftButton->deactivate();
//            _rightButton->deactivate();
//            _backButton->deactivate();
//            _lockButton->deactivate();
            
//            // If any were pressed, reset them
//            _backButton->setDown(false);
//            _leftButton->setDown(false);
//            _rightButton->setDown(false);
//            _lockButton->setDown(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void CodexScene::update(float timestep) {
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
}



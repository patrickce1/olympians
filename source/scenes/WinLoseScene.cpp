#include "WinLoseScene.h"

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
bool WinLoseScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
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
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("winLoseScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    setupUI();
    setupListeners();
    
    _status = Status::IDLE;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the WinLoseScene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * images and the return button. When stats are added more background UI
 * will be added.
 */
void WinLoseScene::setupUI() {
    _returnButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("winLoseScene.return"));
    
    _victoryImage = _assets->get<scene2::SceneNode>("winLoseScene.winner");
    
    _defeatImage = _assets->get<scene2::SceneNode>("winLoseScene.loser");
}

/**
 * Attaches input listeners to the return button.
 */
void WinLoseScene::setupListeners() {
    _returnButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void WinLoseScene::dispose() {
    if (_active) {
        removeAllChildren();
        _returnButton = nullptr;
        _victoryImage = nullptr;
        _defeatImage = nullptr;
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
void WinLoseScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _returnButton->activate();
        } else {
            _returnButton->deactivate();
            
            // If any were pressed, reset them
            _returnButton->setDown(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void WinLoseScene::update(float timestep) {
    if (_didWin) {
        _victoryImage->setVisible(true);
        _defeatImage->setVisible(false);
    } else {
        _victoryImage->setVisible(false);
        _defeatImage->setVisible(true);
    }
    
    _network->getNetworkUpdates();
    
    // Forward to pre game scene if host started while we were here
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
        return;
    }
}


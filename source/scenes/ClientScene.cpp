#include "ClientScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852


/**
 * Initializes the scene contents, and starts the game
 *
 * The constructor does not allocate any objects or memory.  This allows
 * us to have a non-pointer reference to this scene, reducing our
 * memory allocation.  Instead, allocation happens in this method.
 *
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool ClientScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up the input handler
    _assets = assets;
    _network = networkController;
    
    Size dimen = getSize();
    
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("clientScene");
    
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    // Setup UI and respective listeners
    setupUI();
    setupListeners();
    
    _status = Status::IDLE;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the client scene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * enter button, back button, game ID text field, and player name text field.
 * It also initializes the placeholder labels for the input fields.
 */
void ClientScene::setupUI() {
    // Assign pointers to active buttons and text-fields
    _enterGame = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.enter"));

    _backOut = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.back"));

    _gameId = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("clientScene.center.gameID.text"));

    _playerId = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("clientScene.center.playerName.text"));

    // Create placeholder text for text-field
    std::shared_ptr<cugl::scene2::Label> placeID = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.gameID.placeholder"));
    placeID->setText("Enter Game ID");
    
    std::shared_ptr<cugl::scene2::Label> placeName = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.playerName.placeholder"));
    placeName->setText("Enter Name");
    
    // Set the placeholders to invsible when typing starts
    _gameId->addTypeListener([this, placeID](const std::string& name, const std::string& value) {
        placeID->setVisible(value.empty());
    });
    
    _playerId->addTypeListener([this, placeName](const std::string& name, const std::string& value) {
        placeName->setVisible(value.empty());
    });
}

/**
 * Attaches input listeners to the client scene UI controls.
 *
 * This method assigns callbacks for entering the game or returning to the
 * previous menu. It also attaches typing listeners to the game ID and player
 * name text fields to toggle the visibility of their placeholder labels.
 */
void ClientScene::setupListeners() {

    _enterGame->addListener([this](const std::string& name, bool down) {
        if (down) {
            _network->joinRoom(_gameId->getText());
            _status = Status::START;
            //if (_network->checkConnection() == NetworkController::Status::CONNECTED ||
            //    _network->checkConnection() == NetworkController::Status::WAITING) {
            //    _status = Status::START;
            //}
            //else {
            //    //some pop-up saying connection failed idk...
            //    //for now changing the text in the box but we should change this
            //    _gameId->setText("failed");
            //}
            
        }
    });

    _backOut->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void ClientScene::dispose() {
    if (_active) {
        removeAllChildren();
        _active = false;
    }
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
void ClientScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _enterGame->activate();
            _gameId->activate();
            _backOut->activate();
            _playerId->activate();
            // Don't reset the room id
        } else {
            _gameId->deactivate();
            _enterGame->deactivate();
            _backOut->deactivate();
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backOut->setDown(false);
        }
    }
}

/**
 * Checks that the network connection is still active.
 *
 * Even if you are not sending messages all that often, you need to be calling
 * this method regularly. This method is used to determine the current state
 * of the scene.
 *
 * @return true if the network connection is still active.
 */
void ClientScene::updateText(const std::shared_ptr<scene2::Button>& button, const std::string text) {
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
void ClientScene::update(float timestep) {
    // IMPLEMENT ME
}


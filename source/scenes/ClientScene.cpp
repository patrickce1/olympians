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
    initKeypad();
    
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
    
    _hostButton = std::dynamic_pointer_cast<scene2::Button>( _assets->get<scene2::SceneNode>("clientScene.joinHeader.host"));

    _gameId = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("clientScene.center.gameID.text"));

    _playerId = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("clientScene.center.playerName.text"));

    // Create placeholder text for text-field
    _placeID = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.gameID.placeholder"));
    _placeID->setText("ENTER GAME ID");
    
    
    std::shared_ptr<cugl::scene2::Label> placeName = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.playerName.placeholder"));
    placeName->setText("ENTER NAME");
    
    // Set the placeholders to invsible when typing starts
    _playerId->addTypeListener([this, placeName](const std::string& name, const std::string& value) {
        placeName->setVisible(value.empty());
    });
}

/**
 * Initializes keypad buttons, activates them, and attaches input listeners.
 *
 * This method retrieves button nodes from the asset manager, binds digit
 * and backspace actions to their respective handlers, and stores buttons
 * in a collection for batch state control.
 */
void ClientScene::initKeypad() {
    for (int i = 0; i <= 9; i++) {
        auto button = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("clientScene.keypad.key" + std::to_string(i)));
        
        button->addListener([this, i](const std::string& name, bool down) {
            if (down) appendDigit(i);
        });
        
        _keypadButtons.push_back(button);
    }

    auto backspace = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("clientScene.keypad.backspace"));
    backspace->addListener([this](const std::string& name, bool down) {
        if (down) removeLastChar();
    });
    
    _keypadButtons.push_back(backspace);
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
            if(_gameId->getText() != "" && _playerId->getText() != ""){
                _network->joinRoom(_gameId->getText());
                _network->setPlayerName(_playerId->getText());
                _status = Status::START;
            } else {
                _enterGame->setDown(true);
            }
        }
    });

    _backOut->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
    
    _hostButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::HOST;
            _hostButton->setDown(false);
        }
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void ClientScene::dispose() {
    if (_active) {
        removeAllChildren();
        _enterGame = nullptr;
        _backOut = nullptr;
        _hostButton = nullptr;
        _gameId = nullptr;
        _playerId = nullptr;
        _active = false;
        _keypadButtons.clear();
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
void ClientScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _enterGame->activate();
            _backOut->activate();
            _hostButton->activate();
            _playerId->activate();
            for (auto& button : _keypadButtons) {
                button->activate(); 
            }
        } else {
            _playerId->deactivate();
            _enterGame->deactivate();
            _backOut->deactivate();
            _hostButton->deactivate();
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backOut->setDown(false);
            _hostButton->setDown(false);
            for (auto& button : _keypadButtons) {
                button->deactivate();
                button->setDown(false);
            }
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

/**
 * Appends a numeric digit to the input buffer and updates the UI.
 *
 * @param digit The digit (0–9) to append to the input buffer.
 */
void ClientScene::appendDigit(int digit) {
    if (_inputBuffer.size() >= 6) return;

    _inputBuffer += std::to_string(digit);
    _gameId->setText(_inputBuffer);
    _placeID->setVisible(_inputBuffer.empty());
}

/**
 * Removes the last character from the input buffer and updates the UI.
 */
void ClientScene::removeLastChar() {
    if (!_inputBuffer.empty()) {
        _inputBuffer.pop_back();
        _gameId->setText(_inputBuffer);
        _placeID->setVisible(_inputBuffer.empty());
    }
}

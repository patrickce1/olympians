#include "ClientScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852

/** How long (seconds) to wait for the connection before declaring failure */
#define JOIN_TIMEOUT  3.0f

/** How long (seconds) to show the error popup before auto-dismissing */
#define ERROR_DISPLAY_TIME  2.5f

/** Speed of the loading circle in Radians per second */
#define LOADING_SPIN_SPEED  2.0f

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
    _joinTimer = 0.0f;
    _errorTimer = 0.0f;
    
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

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.back"));
    
    _hostButton = std::dynamic_pointer_cast<scene2::Button>( _assets->get<scene2::SceneNode>("clientScene.joinHeader.host"));

    _gameId = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("clientScene.center.gameID.text"));

    _playerName = std::dynamic_pointer_cast<scene2::TextField>(
        _assets->get<scene2::SceneNode>("clientScene.center.playerName.text"));

    // Create placeholder text for text-field
    _textFieldPlaceholder = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.gameID.placeholder"));
    _textFieldPlaceholder->setText("ENTER GAME ID");
    
    std::shared_ptr<cugl::scene2::Label> playerNamePlaceholder = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.playerName.placeholder"));
    playerNamePlaceholder->setText("ENTER NAME");
    
    // Set the placeholders to invisible when typing starts
    _playerName->addTypeListener([this, playerNamePlaceholder](const std::string& name, const std::string& value) {
        playerNamePlaceholder->setVisible(value.empty());
    });
    
    // Error popup node
    _errorPopup = _assets->get<scene2::SceneNode>("clientScene.errorPopup");
    if (_errorPopup) {
        auto overlay = std::dynamic_pointer_cast<scene2::PolygonNode>(_errorPopup->getChildByName("overlayBG"));
        overlay->setContentSize(getSize());
        overlay->setAnchor(Vec2::ANCHOR_CENTER);
        overlay->setPosition(getSize()/2);
        _errorPopup->setVisible(false);
    }
    
    _loading = _assets->get<scene2::SceneNode>("clientScene.loadingOverlay");
    if (_loading) {
        auto overlay = _loading->getChildByName("overlayBG");
        overlay->setContentSize(getSize());
        overlay->setAnchor(Vec2::ANCHOR_CENTER);
        overlay->setPosition(getSize()/2);
        
        _spinner = _loading->getChildByName("spinner");
        _loading->setVisible(false);
    }
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
 * The enter button now initiates a join attempt (Status::JOINING) rather
 * than immediately transitioning to Status::START. The actual transition to
 * START happens in update() once the network confirms a successful connection.
 * If the connection fails or times out, the scene resets to IDLE and shows
 * an error popup.
 */
void ClientScene::setupListeners() {

    _enterGame->addListener([this](const std::string& name, bool down) {
        if (down) {
            if (_status == Status::JOINING) return;  // already attempting, ignore
            
            if (_gameId->getText() != "" && _playerName->getText() != "") {
                // Begin an async join attempt — do NOT set START yet.
                // update() will poll the connection and decide the outcome.
                _network->joinRoom(_gameId->getText());
                _joinTimer = 0.0f;
                _status = Status::JOINING;
                _pendingInputDisable = true;
            } else {
                _enterGame->setDown(true);
            }
        }
    });

    _backButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            // If we were in the middle of a join attempt, cancel it cleanly.
            if (_status == Status::JOINING) {
                _network->disconnect();
            }
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
        _backButton = nullptr;
        _hostButton = nullptr;
        _gameId = nullptr;
        _playerName = nullptr;
        _errorPopup = nullptr;
        _active = false;
        _keypadButtons.clear();
        _loading = nullptr;
        _spinner = nullptr;
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
            _joinTimer = 0.0f;
            _errorTimer = 0.0f;
            if (_errorPopup) _errorPopup->setVisible(false);
            _enterGame->activate();
            _backButton->activate();
            _hostButton->activate();
            _playerName->activate();
            for (auto& button : _keypadButtons) {
                button->activate();
            }
        } else {
            _playerName->deactivate();
            _enterGame->deactivate();
            _backButton->deactivate();
            _hostButton->deactivate();
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backButton->setDown(false);
            _hostButton->setDown(false);
            for (auto& button : _keypadButtons) {
                button->deactivate();
                button->setDown(false);
            }
        }
    }
}

/**
 * Updates the scene each frame.
 *
 * When Status::JOINING is active, this method polls the network connection
 * state every frame:
 *   - CONNECTED  → transitions to Status::START (SceneLoader picks this up).
 *   - FAILED     → shows the error popup and resets to IDLE after a short delay.
 *   - WAITING    → keeps polling until JOIN_TIMEOUT seconds have elapsed,
 *                  after which the connection is cancelled and treated as FAILED.
 *
 * When Status::ERROR is active, this method counts down ERROR_DISPLAY_TIME
 * seconds and then auto-dismisses the popup and returns to IDLE so the player
 * can try again.
 *
 * @param timestep  The amount of time (in seconds) since the last frame.
 */
void ClientScene::update(float timestep) {
    if (_pendingInputDisable) {
        _pendingInputDisable = false;
        setInputEnabled(false);
    }
    
    if (_status == Status::JOINING) {
        _joinTimer += timestep;
        
        if (_loading && _loading->isVisible()) {
            float angle = _spinner->getAngle();
            _spinner->setAngle(angle + LOADING_SPIN_SPEED * timestep);
        }

        NetworkController::Status connStatus = _network->checkConnection();

        if (connStatus == NetworkController::Status::CONNECTED) {
            _network->registerDisconnectCallback();
            _network->setPlayerName(_playerName->getText());
            _status = Status::START;  // go to lobby — validity checked there
        } else if (_joinTimer >= JOIN_TIMEOUT) {
            // Only fail on timeout — not on FAILED state
            CULog("ClientScene: join timed out");
            hideLoadingSpinner();
            _network->disconnect();
            showError("Could not connect.\nPlease check the code and try again.");
        } else {
            showLoadingSpinner();
        }
    }

    if (_status == Status::ERROR_DISPLAY) {
        _errorTimer += timestep;
        if (_errorTimer >= ERROR_DISPLAY_TIME) {
            dismissError();
        }
    }
}

// ---------------------------------------------------------------------------
#pragma mark - Private Helpers
// ---------------------------------------------------------------------------

/**
 * Enables or disables all interactive input controls.
 *
 * Called with false when a join attempt starts so the player cannot spam
 * the button, and called with true when the scene resets to IDLE.
 *
 * @param enabled  Whether the controls should accept input.
 */
void ClientScene::setInputEnabled(bool enabled) {
    if (enabled) {
        _enterGame->activate();
        _backButton->activate();
        _hostButton->activate();
        _playerName->activate();
        for (auto& btn : _keypadButtons) btn->activate();
        _enterGame->setDown(false);
    } else {
        _enterGame->deactivate();
        _hostButton->deactivate();
        _playerName->deactivate();
        for (auto& btn : _keypadButtons) btn->deactivate();
        // Keep _backButton active so the user can cancel the join attempt.
    }
}

/**
 * Displays the error popup with the given message and switches to
 * Status::ERROR_DISPLAY so update() can auto-dismiss it.
 *
 * If no "clientScene.errorPopup" node was found during setupUI(), the
 * message is printed to the console and the scene resets immediately.
 *
 * @param message  Human-readable error text to show.
 */
void ClientScene::showError(const std::string& message) {
    CULog("ClientScene error: %s", message.c_str());

    if (_errorPopup) {
        // Optionally update an inner label if you have one named "errorLabel".
        auto label = std::dynamic_pointer_cast<scene2::Label>(
            _errorPopup->getChildByName("errorLabel"));
        if (label) {
            label->setText(message);
        }
        _errorPopup->setVisible(true);
    }

    _errorTimer = 0.0f;
    _status = Status::ERROR_DISPLAY;
}

/**
 * Hides the error popup and returns the scene to IDLE so the player can
 * correct their input and try again.
 */
void ClientScene::dismissError() {
    if (_errorPopup) {
        _errorPopup->setVisible(false);
    }
    setInputEnabled(true);
    _status = Status::IDLE;
}

/**
 * Shows the loading spinner and re-enables input controls.
 *
 * Called when a join attempt begins so the player has visual feedback
 * that the connection is in progress. The spinner node (_loading) is
 * made visible and input is re-enabled so the player can still cancel
 * via the back button.
 *
 * Does nothing if the spinner is already visible.
 */
void ClientScene::showLoadingSpinner() {
    if (_loading->isVisible()) return;
    _loading->setVisible(true);
    setInputEnabled(true);
    _isSpinning = true;
}

/**
 * Hides the loading spinner.
 *
 * Called when a join attempt concludes — either successfully (transitioning
 * to the lobby) or on failure (showing the error popup). Should always be
 * paired with a prior call to showLoadingSpinner().
 */
void ClientScene::hideLoadingSpinner() {
    _loading->setVisible(false);
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
 * Appends a numeric digit to the input buffer and updates the UI.
 *
 * @param digit The digit (0–9) to append to the input buffer.
 */
void ClientScene::appendDigit(int digit) {
    if (_inputBuffer.size() >= 6) return;

    _inputBuffer += std::to_string(digit);
    _gameId->setText(_inputBuffer);
    _textFieldPlaceholder->setVisible(_inputBuffer.empty());
}

/**
 * Removes the last character from the input buffer and updates the UI.
 */
void ClientScene::removeLastChar() {
    if (!_inputBuffer.empty()) {
        _inputBuffer.pop_back();
        _gameId->setText(_inputBuffer);
        _textFieldPlaceholder->setVisible(_inputBuffer.empty());
    }
}

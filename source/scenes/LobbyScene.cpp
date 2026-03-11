#include "LobbyScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852


/**
 * Initializes the controller contents, and starts the game
 *
 * The constructor does not allocate any objects or memory.  This allows
 * us to have a non-pointer reference to this controller, reducing our
 * memory allocation.  Instead, allocation happens in this method.
 *
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool LobbyScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
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
    
        std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("lobbyScene");
    
        scene->setContentSize(dimen);
        scene->doLayout(); // Repositions the HUD
    
        // Setup UI and listeners
        setupUI();
        setupListeners();
    
        _status = Status::IDLE;
    
        addChild(scene);
        setActive(false);
        return true;
    }

/**
 * Retrieves and stores references to the lobby UI elements.
 *
 * This method looks up important UI components from the scene graph,
 * including the start button, back button, and game ID label, and stores
 * them for later interaction.
 */
void LobbyScene::setupUI() {
        _enterGame = std::dynamic_pointer_cast<scene2::Button>(
            _assets->get<scene2::SceneNode>("lobbyScene.start"));

        _backOut = std::dynamic_pointer_cast<scene2::Button>(
            _assets->get<scene2::SceneNode>("lobbyScene.back"));

        _gameId = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>("lobbyScene.header.gameID"));

        _container = _assets->get<scene2::SceneNode>("lobbyScene.tableArea");

        if (_container) {
            for (int i = 0; i <= 3; i++) {
                std::string cardName = "playerCard" + std::to_string(i);
                auto card = _container->getChildByName(cardName);
                auto label = std::dynamic_pointer_cast<scene2::Label>(
                    card->getChildByName("username")
                );

                _playerSlots.push_back(label);
            }
        }
    }

/**
 * Attaches input listeners to the lobby UI buttons.
 *
 * This method assigns button callbacks that update the lobby scene status
 * when the user presses the start or back buttons.
 */
void LobbyScene::setupListeners() {
    _enterGame->addListener([this](const std::string& name, bool down) {
        if (down) {
            //only host can start the game
            if (_network->isHost()) {
                _network->broadcastGameStart();
            }
            _status = Status::START;
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
void LobbyScene::dispose() {
    if (_active) {
        removeAllChildren();
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
void LobbyScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _enterGame->activate();
            _backOut->activate();
        } else {
            _backOut->deactivate();
            _enterGame->deactivate();
            
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backOut->setDown(false);
        }
    }
}

void LobbyScene::updateLobbyText(std::vector<NetworkedPlayer> onlinePlayers) {
    for (int i = 0; i < _playerSlots.size(); i++) {
        if (i < onlinePlayers.size()) {
            _playerSlots[i]->setText(onlinePlayers[i].username);
        }
        else {
            _playerSlots[i]->setText("AI Player");
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
void LobbyScene::update(float timestep) {
    //get the room once we are fully connected
    if (_network->checkConnection() == NetworkController::Status::CONNECTED) {
        _gameId->setText(_network->getRoom());
        _network->broadcastJoinedLobby();
        _network->getNetworkUpdates();
    }
    else {
        _gameId->setText("waiting...");
    }

    if (!_network->isHost()) {
        _network->getNetworkUpdates();
        bool gameStarted = _network->checkGameStarted();
        if (gameStarted) {
            _status = START;
        }
    }

    updateLobbyText(_network->getNetworkedPlayers());
}


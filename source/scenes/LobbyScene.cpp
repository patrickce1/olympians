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

    _bossImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(_assets->get<scene2::SceneNode>("lobbyScene.tableArea.bossCircle.bossLobbyImage"));
    
    _playerInfoContainer = _assets->get<scene2::SceneNode>("lobbyScene.tableArea");

    if (_playerInfoContainer) {
        for (int i = 0; i <= 3; i++) {
            std::string cardName = "playerCard" + std::to_string(i);
            auto card = _playerInfoContainer->getChildByName(cardName);
            auto label = std::dynamic_pointer_cast<scene2::Label>(
                card->getChildByName("username")
            );
            auto image = std::dynamic_pointer_cast<scene2::Button>(
                card->getChildByName("playerIcon")
            );

            _playerSlots.push_back(label);
            _playerImages.push_back(image);
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
        if (down && _network->isHost() && _network->allPlayersSelectedHouse()) {
            _network->broadcastGameStart();
            _status = Status::START;
        }
    });

    _backOut->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
    
    // Add listeners to all player icon buttons to open the house select screen
    for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages) {
        icon->addListener([this](const std::string& name, bool down) {
            if (down) {
                CULog("down");
                _status = Status::SELECT;
            }
        });
    }
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void LobbyScene::dispose() {
    if (_active) {
        removeAllChildren();
        _playerSlots.clear();
        _playerImages.clear();
        _enterGame = nullptr;
        _backOut = nullptr;
        _gameId = nullptr;
        _bossImage = nullptr;
        _playerInfoContainer = nullptr;
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
            _enterGame->deactivate();
            _backOut->activate();
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->activate();
            }
        } else {
            _backOut->deactivate();
            _enterGame->deactivate();
            for (std::shared_ptr<cugl::scene2::Button> icon : _playerImages){
                icon->deactivate();
                icon->setDown(false);
            }
            
            // If any were pressed, reset them
            _enterGame->setDown(false);
            _backOut->setDown(false);
        }
    }
}

/** Updates the player handles based on updates to the lobby state */
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
 * Updates the player icon images based on the current lobby state.
 *
 * Iterates through the list of player slots and assigns the appropriate
 * icon texture for each connected player based on their selected house.
 * If a slot does not correspond to an active player, a default icon is used.
 *
 * @param onlinePlayers  The list of players currently in the lobby,
 *                       including their selected house information.
 */
void LobbyScene::updateLobbyPlayerIcons(std::vector<NetworkedPlayer> onlinePlayers) {
    for (int i = 0; i < _playerImages.size(); i++) {
        auto image = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(_playerImages[i]->getChildByName("playerIconImg")); // <- change Json to match
        if (image){
            if (i < onlinePlayers.size()) {
                if (onlinePlayers[i].houseID == "Athena") {
                    image->setTexture(_assets->get<cugl::graphics::Texture>("athenaSIcon"));
                } else if (onlinePlayers[i].houseID == "Ares") {
                    image->setTexture(_assets->get<cugl::graphics::Texture>("aresSIcon"));
                } else if (onlinePlayers[i].houseID == "Poseidon") {
                    image->setTexture(_assets->get<cugl::graphics::Texture>("poseidonSIcon"));
                } else {
                    image->setTexture(_assets->get<cugl::graphics::Texture>("emptySIcon"));
                }
            }
            else {
                image->setTexture(_assets->get<cugl::graphics::Texture>("emptySIcon"));
            }
        }
    }
}

void LobbyScene::updateLobbyBossImage(std::string enemyID) {
    if (enemyID == "" && _currentBoss == "") {
        return;
    } else if (enemyID == _currentBoss) {
        return;
    }
    
    _currentBoss = enemyID;
    if (_currentBoss == "enemy1") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("cyclopsLobbyImage"));
    } else if (_currentBoss == "cerberus") {
        _bossImage->setTexture(_assets->get<cugl::graphics::Texture>("cerberusLobbyImage"));
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
        // change boss icon to the currently chosen boss
        updateLobbyBossImage(_network->getEnemy());
    }
    else {
        _gameId->setText("#####");
    }

    if (!_network->isHost()) {
        _network->getNetworkUpdates();
        bool gameStarted = _network->checkGameStarted();
        if (gameStarted) {
            _status = START;
        }
    }
    
    // Only the host can start; only enable the button when all players have locked in a house.
    if (_network->isHost() && _network->allPlayersSelectedHouse()) {
            _enterGame->activate();
    } else {
        _enterGame->deactivate();
    }

    updateLobbyText(_network->getNetworkedPlayers());
    updateLobbyPlayerIcons(_network->getNetworkedPlayers());
}


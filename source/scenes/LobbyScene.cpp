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
bool LobbyScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
                     const std::shared_ptr<NetworkController>& networkController,
                     GameState* gameState){
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    _gameState = gameState;
    
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

/**
 * Updates the username labels in the lobby UI to match the given player list.
 * The list is expected to already be in display order (local player last)
 * as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read names from.
 */
void LobbyScene::updateLobbyText(std::vector<Player*> players) {
    for (int i = 0; i < _playerSlots.size(); i++) {
        _playerSlots[i]->setText(players[i]->getPlayerName());
    }
}

/**
 * Updates the player icon images in the lobby UI based on each player's
 * selected house. The list is expected to already be in display order
 * (local player last) as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read house names from.
 */
void LobbyScene::updateLobbyPlayerIcons(std::vector<Player*> players) {
    for (int i = 0; i < _playerImages.size(); i++) {
        auto image = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
            _playerImages[i]->getChildByName("playerIconImg"));
        if (image) {
            if (players[i]->getHouseName() == "Athena") {
                image->setTexture(_assets->get<cugl::graphics::Texture>("athenaSIcon"));
            } else {
                image->setTexture(_assets->get<cugl::graphics::Texture>("playerIcon"));
            }
        }
    }
}

/**
 * Remaps the full player list from GameState so the local player always
 * appears last (bottom slot of the UI). Walks the circular player array
 * starting one step to the right of the local player, so that left/right
 * neighbour relationships are preserved visually. Includes both real and
 * AI players since both are stored in GameState.
 *
 * This is purely a display remapping — no game or network state is changed.
 *
 * @return  A reordered list of raw Player pointers with the local player last.
 */
std::vector<Player*> LobbyScene::remapPlayersForDisplay() {
    int localIndex = _network->getLocalPlayerNumber();
    const auto& players = _gameState->getPlayers();
    int totalSlots = (int)players.size();

    std::vector<Player*> remapped;
    remapped.reserve(totalSlots);

    for (int i = 1; i < totalSlots + 1; i++) {
        int slot = (localIndex + i) % totalSlots;
        remapped.push_back(players[slot].get());
    }

    return remapped;
}

/**
 * Syncs the game state player names and houses with the current
 * networked player list. Called every frame during the lobby so that
 * _gameState reflects the latest connected player info before the
 * game scene activates.
 */
void LobbyScene::updateNetworkOrder() {
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    const auto& networkedPlayers = _network->getNetworkedPlayers();
    for (int i = 0; i < (int)networkedPlayers.size(); i++) {
        _gameState->setRealPlayer(
            i,
            networkedPlayers[i].username,
            networkedPlayers[i].houseID
        );
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
    
    // Only the host can start; only enable the button when all players have locked in a house.
    if (_network->isHost() && _network->allPlayersSelectedHouse()) {
            _enterGame->activate();
    } else {
        _enterGame->deactivate();
    }
    
    // Remap for display only — network order is unchanged
    updateNetworkOrder();
    std::vector<Player*> displayOrder = remapPlayersForDisplay();
    
    updateLobbyText(displayOrder);
    updateLobbyPlayerIcons(displayOrder);
    _network->clearQueues();
    
}


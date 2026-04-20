#include "PreGameEntryScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Loading Bar Timer */
#define LOADING_TIMER  5.0f

/**
 * Initializes the scene contents, and starts the scene
 *
 * In previous labs, this method "started" the scene.  But in this
 * case, we only use to initialize the scene user interface.  We
 * do not activate the user interface yet, as an active user
 * interface will still receive input EVEN WHEN IT IS HIDDEN.
 *
 * That is why we have the method {@link #setActive}.
 *
 * @param assets             The (loaded) assets for this game mode
 * @param networkController  The network controller shared across all scenes
 * @param gameState          The state of the game
 * @param itemController     The item controller needed to init AI players
 *                           when assignMissingHousesForAI() runs at game start
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool PreGameEntryScene::init(const std::shared_ptr<cugl::AssetManager>& assets,
          const std::shared_ptr<NetworkController>& networkController,
          GameState* gameState,
          ItemController* itemController) {
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
    
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("preGameEntryScene");
    
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD
    
    // Setup UI and listeners
    setupUI();
    
    _status = Status::IDLE;
    
    // Store item controller so assignMissingHousesForAI() can init AI
    // behavior when the host presses Begin Quest.
    _itemController = itemController;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the pre game entry scene UI elements.
 *
 * This method looks up important UI components from the scene graph,
 * including the loading bar, player labels, and house tiles, and storing
 * them for later interaction.
 */
void PreGameEntryScene::setupUI() {
    _loadingBar = std::dynamic_pointer_cast<scene2::ProgressBar>(
        _assets->get<scene2::SceneNode>("preGameEntryScene.loadingBar.preGameBarFill"));
    
    auto bottomSection = _assets->get<scene2::SceneNode>("preGameEntryScene.bottomSection");
    auto topSection = _assets->get<scene2::SceneNode>("preGameEntryScene.topSection");
    
    if (bottomSection && topSection) {
        auto extractFromSection = [&](const std::shared_ptr<scene2::SceneNode>& tileBlock) {
            auto tile = std::dynamic_pointer_cast<scene2::PolygonNode>(tileBlock->getChildByName("emptyTile"));
            if (tile) {
                _playerTiles.push_back(tile);
            }
        };

        extractFromSection(bottomSection->getChildByName("rightPlayerTile"));
        extractFromSection(topSection->getChildByName("topPlayerTile"));
        extractFromSection(topSection->getChildByName("leftPlayerTile"));
        extractFromSection(bottomSection->getChildByName("localPlayerTile"));
    }
    
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void PreGameEntryScene::dispose() {
    if (_active) {
        removeAllChildren();
        _playerNames.clear();
        _playerTiles.clear();
        _houseNames.clear();
        _loadingBar = nullptr;
        _active = false;
    }
    _network = nullptr;
}

/**
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.
 *
 * @param value whether the scene is currently active
 */
void PreGameEntryScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _loadingProgress = 0.0f;

            if (_loadingBar) {
                _loadingBar->setProgress(0.0f);
            }
        }
    }
}

/**
 * The method called to update the scene.
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void PreGameEntryScene::update(float timestep) {
    if (!_active || !_loadingBar) return;

    // Increase progress based on time
    _loadingProgress += timestep / LOADING_TIMER;

    if (_loadingProgress > 1.0f) {
        _loadingProgress = 1.0f;
    }

    _loadingBar->setProgress(_loadingProgress);

    // When done loading
    if (_loadingProgress >= 1.0f) {
        _status = Status::START;
    }
    
    std::vector<Player*> displayOrder = remapPlayersForDisplay();
    updateEntryScreenTiles(displayOrder);
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
std::vector<Player*> PreGameEntryScene::remapPlayersForDisplay() {
    int localIndex = _network->getLocalPlayerNumber();
    const auto& players = _gameState->getPlayers();
    int totalTiles = (int)players.size();

    std::vector<Player*> remappedPlayerTiles;
    remappedPlayerTiles.reserve(totalTiles);

    for (int i = 1; i < totalTiles + 1; i++) {
        int slot = (localIndex + i) % totalTiles;
        remappedPlayerTiles.push_back(players[slot].get());
    }

    return remappedPlayerTiles;
};

/**
 * Updates the player tile images in the pre game entry UI based on each player's
 * selected house. The list is expected to already be in display order
 * (local player last) as produced by remapPlayersForDisplay().
 *
 * @param players  The display-ordered list of players to read house names from.
 */
void PreGameEntryScene::updateEntryScreenTiles(std::vector<Player*> players) {
    for (int i = 0; i < _playerTiles.size(); i++) {
        auto tile = _playerTiles[i];
        if (tile) {
            std::string key = players[i]->getHouseName() + "Box";
            
            if (_assets->get<cugl::graphics::Texture>(key) != nullptr) {
                tile->setTexture(_assets->get<cugl::graphics::Texture>(key));
            } else {
                tile->setTexture(_assets->get<cugl::graphics::Texture>("emptyBox"));
            }
        }
    }
};


#include "PreGameEntryScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Loading Bar Timer */
#define LOADING_TIMER  0.5f

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
    
};

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
        }
    }
}

/**
 * The method called to update the scene.
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void PreGameEntryScene::update(float timestep) {
    
}


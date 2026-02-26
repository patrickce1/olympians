#include <cugl/cugl.h>
#include <iostream>
#include <sstream>

#include "GameScene.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Example height for now, change as needed */
#define SCENE_HEIGHT  852

#pragma mark -
#pragma mark Constructors
/**
 * Initializes the controller contents, and starts the game
 *
 * In previous labs, this method "started" the scene.  But in this
 * case, we only use to initialize the scene user interface.  We
 * do not activate the user interface yet, as an active user
 * interface will still receive input EVEN WHEN IT IS HIDDEN.
 *
 * That is why we have the method {@link #setActive}.
 *
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool GameScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    }
    else if (!Scene2::initWithHint(Size(0, SCENE_HEIGHT))) {
        return false;
    }

    // Start up the input handler
    _assets = assets;
    
    // The comments are outline of how loading a scene from json should work. This DOES NOT WORK YET. Danielle should set this up
    // Acquire the scene built by the asset loader and resize it the scene. 
    scene = _assets->get<scene2::SceneNode>("gameScene");
    
    if (!scene) {
        printf("Scene NOT here!");
        return false;
    }

    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD
    
    // Elements setup from assets
    _gameArea  = scene->getChildByName("game_area");
    _inventory = scene->getChildByName("inventory");
    
    if (_gameArea) {

        _attackArea = _gameArea->getChildByName("attack_area");

        // AI player slots
        _playerSlots.push_back(_gameArea->getChildByName("player"));
        _playerSlots.push_back(_gameArea->getChildByName("player_3"));
        _playerSlots.push_back(_gameArea->getChildByName("player_4"));
    }
    
    if (_attackArea) {
        // attack_area widget contains:
        //   "attack_area" (image region for attack input)
        //   "enemy" (boss widget)
        _bossNode = _attackArea->getChildByName("enemy");
    }
    
    // static items
    if (_inventory) {
        _abilityIcons.push_back(_inventory->getChildByName("heal"));
        _abilityIcons.push_back(_inventory->getChildByName("heal_2"));
        _abilityIcons.push_back(_inventory->getChildByName("attack"));
        _abilityIcons.push_back(_inventory->getChildByName("attack_4"));
        _abilityIcons.push_back(_inventory->getChildByName("attack_5"));
        _abilityIcons.push_back(_inventory->getChildByName("attack_6"));
    }
    

    addChild(scene);

    // CHANGE third argument to data-driven later
    if (!_itemController.init(_assets)) {
        CULog("GameScene: failed to initialize ItemController");
        return false;
    }
    
    // Spawn GameScene Enemy
    const std::string enemyJsonPath = "json/enemies.json";
    if (!_enemyLoader.loadFromFile(enemyJsonPath)) {
        CULog("GameScene: failed to load enemies from '%s' (continuing without enemy)",
              enemyJsonPath.c_str());
        _enemy.reset();
    } else {
        const std::string enemyId = "enemy1";  // or "dummy"

        if (!_enemyLoader.has(enemyId)) {
            CULog("GameScene: enemy id '%s' not found in '%s' (continuing without enemy)",
                  enemyId.c_str(), enemyJsonPath.c_str());
            _enemy.reset();
        } else {
            _enemy = std::make_unique<Enemy>(enemyId, _enemyLoader);
            CULog("GameScene: spawned enemy '%s'", enemyId.c_str());
        }
    }

    setActive(false);
    return true;
}

void GameScene::update(float dt) {
    if (!_active) {
        return;
    }

    _itemController.update(dt, _players);
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void GameScene::dispose() {
    if (_active) {
        removeAllChildren();
        _gameArea = nullptr;
        _inventory = nullptr;
        _attackArea = nullptr;
        _bossNode = nullptr;

        _playerSlots.clear();
        _abilityIcons.clear();
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
void GameScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            //put anything that needs to be activated as part of the scene
        }
        else {
            //deactivate any children that are part of the scene
        }
    }
}

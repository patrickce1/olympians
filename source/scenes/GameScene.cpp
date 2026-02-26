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
    Size dimen = getSize();
    
    // The comments are outline of how loading a scene from json should work. This DOES NOT WORK YET. Danielle should set this up
    // Acquire the scene built by the asset loader and resize it the scene. 
    scene = _assets->get<scene2::SceneNode>("gameScene");
    
    if (!scene) {
        printf("Scene NOT here!");
        return false;
    }

    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD
    for (auto& icon : _abilityIcons) {
        if (!icon) continue;
        icon->setAnchor(Vec2(0.5f, 0.5f));
    }
    // Elements setup from assets
    _gameArea  = scene->getChildByName("gameArea");
    _inventory = scene->getChildByName("inventory");
    
    if (_gameArea) {

        _attackArea = _gameArea->getChildByName("attackArea");

        // AI player slots
        _playerSlots.push_back(_gameArea->getChildByName("player"));
        _playerSlots.push_back(_gameArea->getChildByName("player3"));
        _playerSlots.push_back(_gameArea->getChildByName("player4"));
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
        _abilityIcons.push_back(_inventory->getChildByName("heal2"));
        _abilityIcons.push_back(_inventory->getChildByName("attack"));
        _abilityIcons.push_back(_inventory->getChildByName("attack4"));
        _abilityIcons.push_back(_inventory->getChildByName("attack5"));
        _abilityIcons.push_back(_inventory->getChildByName("attack6"));
    }

    addChild(scene);

    if (!_itemController.init(_assets)) {
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

void GameScene::update(float dt, InputController& input) {
    if (!_active) return;
    
    if (_activeIcon && (input.touchEnded())){
        _activeIcon = nullptr;
    }

    if (!_activeIcon && input.isTouching()) {
        // Flip y since touch is top-left origin, scene is bottom-left origin
        float h = getSize().height;
        cugl::Vec2 touchFlipped = cugl::Vec2(input.getTouchStart().x, h - input.getTouchStart().y);
        cugl::Vec2 touchInInventory = _inventory->parentToNodeCoords(touchFlipped);

        for (auto& icon : _abilityIcons) {
            if (!icon) continue;
            cugl::Rect bbox = icon->getBoundingBox();
//            float padX = bbox.size.width / 2;
//            float padY = bbox.size.height / 2;
//            bbox.origin.x -= padX;
//            bbox.origin.y -= padY;
//            bbox.size.width += padX * 1;
//            bbox.size.height += padY * 1.25;
            if (bbox.contains(touchInInventory)) {
                _activeIcon = icon;
                _dragOffset = icon->getPosition() - touchInInventory;
                break;
            }
        }
    }

    if (_activeIcon && input.isTouching()) {
        float h = getSize().height;
        cugl::Vec2 dragFlipped = cugl::Vec2(input.getDragPos().x, h - input.getDragPos().y);
        cugl::Vec2 dragInInventory = _inventory->parentToNodeCoords(dragFlipped);
        _activeIcon->setPosition(dragInInventory + _dragOffset);
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

void GameScene::render() {
    Scene2::render();
    
    auto batch = getSpriteBatch();
    batch->setPerspective(getCamera()->getCombined());
    batch->begin();
    batch->setColor(Color4::RED);
    
    for (auto& icon : _abilityIcons) {
        if (!icon) continue;
        Rect bbox = icon->getBoundingBox();
//        float padX = bbox.size.width / 2;
//        float padY = bbox.size.height / 2;
//        bbox.origin.x -= padX;
//        bbox.origin.y -= padY;
//        bbox.size.width += padX * 1;
//        bbox.size.height += padY * 1.25;
////
//        Vec2 origin = _inventory->nodeToParentCoords(bbox.origin);
//        Vec2 corner = _inventory->nodeToParentCoords(bbox.origin + bbox.size);
//        Rect screenBox(origin, Size(corner.x - origin.x, corner.y - origin.y));
        
        Path2 path(bbox);
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
    
    batch->end();
}

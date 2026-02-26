#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__
#include <cugl/cugl.h>
#include <vector>
#include "Player.h"
#include "ItemController.h"
#include "Enemy.h"
#include "EnemyLoader.h"

/**
 * This class represents the core game scene
 * Add things to this class as necessary
 */
class GameScene : public cugl::scene2::Scene2 {
public:
    /*getters that represent game state should go here*/

protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    
    std::shared_ptr<cugl::scene2::SceneNode> scene;
    
    std::shared_ptr<cugl::scene2::SceneNode> _gameArea;
    std::shared_ptr<cugl::scene2::SceneNode> _attackArea;
    std::shared_ptr<cugl::scene2::SceneNode> _bossNode;
    
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _playerSlots;

    std::shared_ptr<cugl::scene2::SceneNode> _inventory;
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _abilityIcons;

    std::vector<Player> _players;
    ItemController _itemController;
    EnemyLoader _enemyLoader;
    std::unique_ptr<Enemy> _enemy;


public:
#pragma mark -
#pragma mark Constructors
    /**
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    GameScene() : cugl::scene2::Scene2() {}

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any 
     * static resources, like the input controller.
     */
    ~GameScene() { dispose(); }

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;

    /**
     * Initializes the controller contents.
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
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.  Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
     *
     * @param value whether the scene is currently active
     */
    virtual void setActive(bool value) override;

    //everything that needs to be updated. Anything that isn't a graphics call goes here
    virtual void update(float dt) override;

};

#endif /* __GAME_SCENE_H__ */

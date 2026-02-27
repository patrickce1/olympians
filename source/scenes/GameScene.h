#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__
#include <cugl/cugl.h>
#include <vector>
#include "Player.h"
#include "ItemController.h"
#include "Enemy.h"
#include "EnemyLoader.h"
#include "InputController.h"


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
    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> scene;

    /** The node representing the main game area, top half of screen. */
    std::shared_ptr<cugl::scene2::SceneNode> _gameArea;

    /** The node representing the attack interaction area, the red zone. */
    std::shared_ptr<cugl::scene2::SceneNode> _attackArea;

    /** The node representing the boss character in the scene, the centerpiece. */
    std::shared_ptr<cugl::scene2::SceneNode> _bossNode;

    /** The collection of UI slots used to display teammate-related nodes. */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _playerSlots;

    /** The node representing the player's inventory UI container. */
    std::shared_ptr<cugl::scene2::SceneNode> _inventory;
    
    /** The node representing the active (placeholder) item that the player is holding**/
    std::shared_ptr<cugl::scene2::SceneNode> _activeIcon;
    /**Distance that the active item has moved.**/
    cugl::Vec2 _dragOffset;

    /** The collection of item icon nodes displayed in the inventory. */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _abilityIcons;

    std::vector<Player> _players;
    ItemController _itemController;
    EnemyLoader _enemyLoader;
    std::unique_ptr<Enemy> _enemy;
    
    InputController::Action _glowAction = InputController::Action::NONE;
    float _glowTimer = 0;
    float _glowDuration = 0.3f;

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
     * Resets the scene.
     */
    void reset() override;
    
    std::shared_ptr<cugl::scene2::SceneNode> _resetBtn;


    std::vector<cugl::Vec2> _abilityOriginalPos;
    
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
    void update(float dt,InputController& input);
    
    void render() override;


};


#endif /* __GAME_SCENE_H__ */

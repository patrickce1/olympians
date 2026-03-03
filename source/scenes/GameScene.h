#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__
#include <cugl/cugl.h>
#include <vector>
#include <unordered_map>
#include "Player.h"
#include "InputController.h"
#include "ItemController.h"
#include "Enemy.h"
#include "EnemyLoader.h"
#include "CharacterLoader.h"
#include "PlayerAI.h"
#include "EasyPlayerAI.h"

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
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

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

    /** The collection of item icon nodes displayed in the inventory. */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _abilityIcons;
    
    /** The collection of item widgets mapping itemId to its corresponding widget */
    std::unordered_map<ItemInstance::ItemId, std::shared_ptr<cugl::scene2::SceneNode>> _itemWidgets;
    
    /** The character loader to load in player characters for this GameScene instance */
    CharacterLoader _characterLoader;
    
    /** Pointer to the player belonging to the local machine. Set once in init(), never reallocated. */
    Player* _localPlayer = nullptr;
    
    /** The collection of players in this party */
    std::vector<Player> _players;
  
    /** AI controllers for bot-controlled players, stored as base class pointers
     *  to allow mixed difficulty levels in the same collection */
    std::vector<std::unique_ptr<PlayerAI>> _playerAIs;
    
    /** Handles touch input for the human player */
    InputController _input;
    
    /** The ItemController for this GameScene instance*/
    ItemController _itemController;
    
    /** The enemy loader to load in the enemies for this GameScene instance */
    EnemyLoader _enemyLoader;
    
    /** The current enemy that the players are facing */
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
    
    /**
     * Called after the network session and assigns all local machines to a network-given player slot
     */
    void setLocalPlayer(int assignedIndex);
    /**
     * Handles the human player dropping an item on the boss zone to attack.
     */
    void handleAttack();

    /**
     * Handles the human player dropping an item on the left ally zone to support.
     */
    void handleSupportLeft();

    /**
     * Handles the human player dropping an item on the right ally zone to support.
     */
    void handleSupportRight();

    /**
     * Handles the human player passing an item to the left neighbor.
     */
    void handlePassLeft();

    /**
     * Handles the human player passing an item to the right neighbor.
     */
    void handlePassRight();
    
    

    //everything that needs to be updated. Anything that isn't a graphics call goes here
    virtual void update(float dt) override;
    
    /** Create and return an item Widget with a given ItemInstance */
    std::shared_ptr<cugl::scene2::SceneNode> createItemWidget(const ItemInstance& item);
    
    /** Return the world position for an item widget's initial inventory position */
    cugl::Vec2 getInitialInventoryPosition() const;
    
    /** Sync player inventory and item widgets displayed on screen */
    void syncInventoryWidgets();

};

#endif /* __GAME_SCENE_H__ */

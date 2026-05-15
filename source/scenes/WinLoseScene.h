#ifndef __WIN_LOSE_SCENE_H__
#define __WIN_LOSE_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "../EnemyLoader.h"
#include "../NetworkController.h"

/**
 * This class provides the interface to make the win lose scene.
 */
class WinLoseScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /** basic state  */
        IDLE,
        /** back to lobby */
        ABORT,
        /** forward to pre gamescene*/
        PRE_GAMESCENE_START
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    
    /** The scene node defined by the JSON */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The network controller shared across all scenes */
    std::shared_ptr<NetworkController> _network;
    
    /** The return button for the boss select scene */
    std::shared_ptr<cugl::scene2::Button> _returnButton;
    
    /** The base images for showing victory */
    std::shared_ptr<cugl::scene2::SceneNode> _victoryImage;
    
    /** The base images for showing defeat */
    std::shared_ptr<cugl::scene2::SceneNode> _defeatImage;
    
    std::shared_ptr<cugl::scene2::Button>    _continueButton;   // phase 1
    std::shared_ptr<cugl::scene2::SceneNode> _teamStats;
    std::shared_ptr<cugl::scene2::SceneNode> _statsHeader;
    std::shared_ptr<cugl::scene2::SceneNode> _indivStats;
    std::shared_ptr<cugl::scene2::SceneNode> _successLabel;
    
    // Team stats values
    std::shared_ptr<cugl::scene2::Label> _teamTotalDmg;
    std::shared_ptr<cugl::scene2::Label> _teamTotalHeal;

    // Utility stars (each star has an empty/fill child to toggle)
    std::shared_ptr<cugl::scene2::SceneNode> _utilStar[3];

    // Individual player rows (indexed 0–3)
    std::shared_ptr<cugl::scene2::Label> _playerDmg[4];
    std::shared_ptr<cugl::scene2::Label> _playerHeal[4];
    std::shared_ptr<cugl::scene2::Label> _playerUtility[4];
    std::shared_ptr<cugl::scene2::Label> _playerName[4];
    
    struct PlayerStats {
        std::string displayName;  // e.g. "ATHENA | help_me123"
        int damage;
        int heals;
        int utility;
    };
    
    // Timeline
    std::shared_ptr<cugl::ActionTimeline> _timeline;

    // Track if timeline is running
    bool _animating = false;
    
    int _phase = 1;
    bool _pendingPhase2 = false;
    
    /** The current status */
    Status _status;
    
    /** Whether we did win or lose*/
    bool _didWin;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new win/lose scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    WinLoseScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~WinLoseScene() { dispose(); }
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;
    
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
     * @param networkController The network controller shared across all scenes
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController);
    
    /**
     * Retrieves and stores references to the WinLoseScene UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * images and the return button. When stats are added more background UI
     * will be added.
     */
    void setupUI();
    
    void setupStatsUI();
    
    /**
     * Attaches input listeners to the return button.
     */
    void setupListeners();
    
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
     * Returns the scene status.
     *
     * Any value other than IDLE will transition to a new scene.
     *
     * @return the scene status
     *
     */
    Status getStatus() const { return _status; }

    /**
     * The method called to update the scene.
     *
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep) override;
    
    /**
     * Sets the whether we won or lost.
     */
    void setDidWin(bool value) { _didWin = value; }
    

private:
    void showPhase(int phase);
    
    void setStats(const PlayerStats players[4]);
    
    std::string formatNumber(int value);
    
    void runPhase1Intro();
    
    void transitionToPhase2();
    
    void runPhase2Intro();
    
    void resetVisualState();
};

#endif /* __WIN_LOSE_SCENE_H__ */


#ifndef __CLIENT_SCENE_H__
#define __CLIENT_SCENE_H__

#include <cugl/cugl.h>
#include "../NetworkController.h"

/**
 * This class provides the interface to join an existing game.
 *
 * Most games have a since "matching" scene whose purpose is to initialize the
 * network controller.  We have separate the host from the client to make the
 * code a little more clear.
 */
class ClientScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /** Client has not yet entered a room */
        IDLE,

        /** Connection confirmed — SceneLoader transitions to lobby */
        START,

        /** Client is connecting to the host */
        JOINING,
        
        /** Connected to host; waiting one frame to confirm lobby has space */
        CONNECTED,

        /** Join attempt failed — error popup is displayed before resetting to IDLE */
        ERROR_DISPLAY,

        /** Game was aborted; back to main menu */
        ABORT,

        /** Client switches to host game screen */
        HOST
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The network controller shared across all scenes*/
    std::shared_ptr<NetworkController> _network;
    
    /** Menu buttons. **/
    /** The menu button for entering a game */
    std::shared_ptr<cugl::scene2::Button> _enterGame;
    /** The back button for the menu scene */
    std::shared_ptr<cugl::scene2::Button> _backButton;
    /** The game id label (for updating) */
    std::shared_ptr<cugl::scene2::TextField> _gameId;
    /** The game id placeholder label */
    std::shared_ptr<cugl::scene2::Label> _textFieldPlaceholder;
    /** The game id label (for updating) */
    std::shared_ptr<cugl::scene2::TextField> _playerName;
    /** The host game button for the client scene */
    std::shared_ptr<cugl::scene2::Button> _hostButton;
    /** The settings button to display settings menu */
    std::shared_ptr<cugl::scene2::Button> _settingsButton;
    /** Stores the current user input for the gameID as a numeric string.*/
    std::string _inputBuffer = "";
    /** Collection of all keypad buttons fir gameID (digits + backspace). */
    std::vector<std::shared_ptr<cugl::scene2::Button>> _keypadButtons;
    /** Optional error-popup node (may be nullptr if absent from JSON scene). */
    std::shared_ptr<cugl::scene2::SceneNode> _errorPopup;
    /** Loading overlay node */
    std::shared_ptr<cugl::scene2::SceneNode> _loading;
    /** Loading spinning circle node */
    std::shared_ptr<cugl::scene2::SceneNode> _spinner;
    /** Seconds elapsed since the current join attempt began. */
    float _joinTimer;
    /** Seconds elapsed since the error popup was shown. */
    float _errorTimer;
    /** The current status */
    Status _status;
    /** Whether the Input is pending to be disabled*/
    bool _pendingInputDisable = false;
    /** Whether the loading circle is spinning. */
    bool _isSpinning = false;
    /** Set to true when the user taps the settings button */
    bool _pendingSettings = false;
    
public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new client scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    ClientScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~ClientScene() { dispose(); }
    
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
     * Retrieves and stores references to the client scene UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * enter button, back button, game ID text field, and player name text field.
     * It also initializes the placeholder labels for the input fields.
     */
    void setupUI();
    
    /**
     * Attaches input listeners to the client scene UI controls.
     *
     * This method assigns callbacks for entering the game or returning to the
     * previous menu. It also attaches typing listeners to the game ID and player
     * name text fields to toggle the visibility of their placeholder labels.
     */
    void setupListeners();
    
    /**
     * Initializes keypad buttons, activates them, and attaches input listeners.
     *
     * This method retrieves button nodes from the asset manager, binds digit
     * and backspace actions to their respective handlers, and stores buttons
     * in a collection for batch state control.
     */
    void initKeypad();
    
    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.  Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
     *
     * @param value                     Whether the scene is active.
     * @param preserveGameId  If true, the game ID input field and buffer are
     *                       left untouched on activation. Pass true when the
     *                       client voluntarily navigated back from the lobby
     *                       so they don't have to retype the code. Pass false
     *                       (default) on host-disconnect kickouts so the stale
     *                       room code is cleared.
     */
    virtual void setActive(bool value, bool preserveGameId = false);
    
    /**
     * Returns the scene status.
     *
     * Any value other than WAIT will transition to a new scene.
     *
     * @return the scene status
     *
     */
    Status getStatus() const { return _status; }
    
    /**
     * Updates the scene each frame. Polls the network while joining and
     * manages the error-popup countdown.
     * @param timestep  Seconds since the last frame.
     */
    void update(float timestep);
    
    /**
     * Returns true if the user has requested to open settings, then resets the flag.
     */
    bool shouldOpenSettings();
    
    /**
     * Enables or disables all interactive input controls.
     * @param enabled  Whether controls should accept input.
     */
    void setInputEnabled(bool enabled);
    
private:
    /**
     * Updates the text in the given button.
     *
     * Techincally a button does not contain text. A button is simply a scene graph
     * node with one child for the up state and another for the down state. So to
     * change the text in one of our buttons, we have to descend the scene graph.
     * This method simplifies this process for you.
     *
     * @param button    The button to modify
     * @param text      The new text value
     */
    void updateText(const std::shared_ptr<cugl::scene2::Button>& button, const std::string text);
    
    /**
     * Appends a numeric digit to the input buffer and updates the UI.
     *
     * @param digit The digit (0–9) to append to the input buffer.
     */
    void appendDigit(int digit);
    
    /**
     * Removes the last character from the input buffer and updates the UI.
     */
    void removeLastChar();
 
    /**
     * Displays the error popup with the given message and enters ERROR_DISPLAY.
     * @param message  Human-readable error text.
     */
    void showError(const std::string& message);
 
    /** Hides the error popup and resets to IDLE. */
    void dismissError();
    
    /**
     * Shows the loading spinner and re-enables input controls.
     *
     * Called when a join attempt begins so the player has visual feedback
     * that the connection is in progress. The spinner node (_loading) is
     * made visible and input is re-enabled so the player can still cancel
     * via the back button.
     *
     * Does nothing if the spinner is already visible.
     */
    void showLoadingSpinner();
    
    /**
     * Hides the loading spinner.
     *
     * Called when a join attempt concludes — either successfully (transitioning
     * to the lobby) or on failure (showing the error popup). Should always be
     * paired with a prior call to showLoadingSpinner().
     */
    void hideLoadingSpinner();
};

#endif /* __CLIENT_SCENE_H__ */

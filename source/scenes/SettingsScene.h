#ifndef __SETTINGS_SCENE_H__
#define __SETTINGS_SCENE_H__

#include <cugl/cugl.h>

/**
 * A persistent overlay scene for application settings.
 *
 * This scene is initialized once after assets are loaded and remains
 * alive for the entire application lifetime. Any scene can show it
 * by calling setActive(true).
 */
class SettingsScene : public cugl::scene2::Scene2 {

protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The text-field where you can change your username */
    std::shared_ptr<cugl::scene2::TextField> _usernameField;

    /** The back button to close the settings scene */
    std::shared_ptr<cugl::scene2::Button> _backButton;

    /** The audio/sfx slider */
    std::shared_ptr<cugl::scene2::Slider> _sfxSlider;

    /** The music slider */
    std::shared_ptr<cugl::scene2::Slider> _musicSlider;

    /** The toggle button for screen effects */
    std::shared_ptr<cugl::scene2::Button> _effectsButton;

    /** The toggle button for haptics */
    std::shared_ptr<cugl::scene2::Button> _hapticsButton;

    /** The save button to save the data and close the settings scene */
    std::shared_ptr<cugl::scene2::Button> _saveButton;

    /** The current SFX/audio volume, in [0,1] */
    float _sfxVolume = 1.0f;

    /** The current music volume, in [0,1] */
    float _musicVolume = 1.0f;

    /** Whether screen effects are enabled */
    bool _effectsEnabled = true;

    /** Whether haptics are enabled */
    bool _hapticsEnabled = true;

    /**
     * Whether init() has been successfully called.
     * Used by dispose() to guard against double-free.
     */
    bool _initialized = false;

    /** The player's display name */
    std::string _username;

    /** Set to true when the user has requested to close the overlay.
     *  SceneLoader polls this each frame via consumeClose() rather than
     *  reacting inside the listener, which avoids mid-frame setActive crashes.
     */
    bool _pendingClose = false;
    
    /** Called when the music slider changes, passes new multiplier value */
    std::function<void(float)> _onMusicVolumeChange;

    /** Called when the SFX slider changes, passes new multiplier value */
    std::function<void(float)> _onSFXVolumeChange;

public:
#pragma mark -
#pragma mark Constructors

    /**
     * Creates a new settings scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    SettingsScene() : cugl::scene2::Scene2() {}

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~SettingsScene() { dispose(); }

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This is called automatically by the destructor. It checks _initialized
     * to avoid double-freeing resources if dispose() is called manually first.
     */
    void dispose() override;

    /**
     * Initializes the scene contents.
     *
     * In previous labs, this method "started" the scene. But in this
     * case, we only use it to initialize the scene user interface. We
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
     * Retrieves and stores references to the settings scene UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * save button, back button, text fields, and toggle buttons.
     * It also initializes the placeholder labels for the input fields.
     */
    void setupUI();

    /**
     * Attaches input listeners to the settings scene UI controls.
     *
     * This method assigns callbacks for the sliders, toggle buttons,
     * and back/save buttons. The back and save buttons both invoke
     * _onClose after persisting state.
     */
    void setupListeners();

#pragma mark -
#pragma mark Scene Lifecycle

    /**
     * Sets whether the scene is currently active.
     *
     * This method should be used to toggle all the UI elements. Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
     *
     * @param value whether the scene is currently active
     */
    virtual void setActive(bool value) override;

    /**
     * Updates the scene each frame.
     *
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep) override;


#pragma mark -
#pragma mark Settings & Persistence

    /**
     * Serializes current settings to a JSON file in the app's save directory.
     *
     * Called automatically when the user presses the save button.
     */
    void saveSettings();

    /**
     * Deserializes settings from the JSON file in the app's save directory.
     *
     * Called at the end of init(). If no save file exists, default
     * values are kept and no error is raised.
     */
    void loadSettings();
    
    /**
     * Returns true if the user has pressed back or save, then resets the flag.
     * Call this from SceneLoader::update() after _settingsScene.update().
     */
    bool consumeClose() {
        bool val = _pendingClose;
        _pendingClose = false;
        return val;
    }
    
    /**
     * Registers a callback invoked whenever the music volume slider changes.
     *
     *  @param cb  Receives the new multiplier in [0,1]
     */
    void setOnMusicVolumeChange(std::function<void(float)> cb) { _onMusicVolumeChange = cb; }

    /**
     * Registers a callback invoked whenever the SFX volume slider changes.
     *
     *  @param cb  Receives the new multiplier in [0,1]
     */
    void setOnSFXVolumeChange(std::function<void(float)> cb) { _onSFXVolumeChange = cb; }

private:
#pragma mark -
#pragma mark Helpers

    /**
     * Enables or disables all interactive input controls.
     *
     * Called internally by setActive() to activate or deactivate
     * every button, slider, and text field in one place.
     *
     * @param enabled  Whether controls should accept input
     */
    void setInputEnabled(bool enabled);
};

#endif /* __SETTINGS_SCENE_H__ */

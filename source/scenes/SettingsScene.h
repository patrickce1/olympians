#ifndef __SETTINGS_SCENE_H__
#define __SETTINGS_SCENE_H__

#include <cugl/cugl.h>

/**
[TBD]
 */
class SettingsScene: public cugl::scene2::Scene2 {
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    
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
    std::shared_ptr<cugl::scene2::Button> _backButton;
};

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
     */
    void dispose() override;
    
    /**
     * Initializes the scene contents
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
     * This method assigns callbacks for adjusting a slider or returning to the
     * previous menu. It also attaches typing listeners to the username text field
     *  to toggle the visibility of its placeholder label.
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
     * Updates the scene each frame.
     */
    void update(float timestep);

    void render(const std::shared_ptr<cugl::SpriteBatch>& batch) override;

    void setVisible(bool visible);
    
private:
    /**
     * Enables or disables all interactive input controls.
     * @param enabled  Whether controls should accept input.
     */
    void setInputEnabled(bool enabled);

    void onClose();
};

#endif /* __SETTINGS_SCENE_H__ */

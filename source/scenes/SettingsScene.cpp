#include "SettingsScene.h"

using namespace cugl;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852

/**
 * Initializes the scene contents, and starts the game
 *
 * The constructor does not allocate any objects or memory.  This allows
 * us to have a non-pointer reference to this scene, reducing our
 * memory allocation.  Instead, allocation happens in this method.
 *
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool SettingsScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up the input handler
    _assets = assets;
    
    Size dimen = getSize();
    
    _scene = _assets->get<scene2::SceneNode>("settingsScene");
    
    _scene->setContentSize(dimen);
    _scene->doLayout(); // Repositions the HUD

    // Setup UI and respective listeners
    setupUI();
    setupListeners();
    
    addChild(_scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the settings scene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * save button, back button, text fields, and toggle buttons.
 * It also initializes the placeholder labels for the input fields.
 */
void SettingsScene::setupUI() {
    // Assign pointers to active buttons and text-fields
    _usernameField = std::dynamic_pointer_cast<scene2::TextField>(
       _assets->get<scene2::SceneNode>("settingsScene.username.text"));
    
    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _scene->getChildByName("back"));
    
    _sfxSlider = std::dynamic_pointer_cast<scene2::Slider>(
        _scene->getChildByName("audioSlider"));
    
    _musicSlider = std::dynamic_pointer_cast<scene2::Slider>(
        _scene->getChildByName("musicSlider"));
    
    _effectsButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("settingsScene.vibrations.toggleButton"));
    _effectsButton->setDown(true);
    
    _hapticsButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("settingsScene.haptics.toggleButton"));
    _hapticsButton->setDown(true);
    
    _saveButton = std::dynamic_pointer_cast<scene2::Button>(
        _scene->getChildByName("save"));
    
    auto usernamePlaceholder = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("settingsScene.username.placeholder"));
    usernamePlaceholder->setText("ENTER NAME");
    
    // Set the placeholder to invisible when typing starts
    _usernameField->addTypeListener([this, usernamePlaceholder](const std::string& name, const std::string& value) {
        usernamePlaceholder->setVisible(value.empty());
    });
    
    auto overlay = _scene->getChildByName("background");
    overlay->setContentSize(getSize());
    overlay->setAnchor(Vec2::ANCHOR_CENTER);
    overlay->setPosition(getSize()/2);
}

/**
 * Attaches input listeners to the settings scene UI controls.
 *
 * This method assigns callbacks for adjusting a slider or returning to the
 * previous menu. It also attaches typing listeners to the username text field
 *  to toggle the visibility of its placeholder label.
 */
void SettingsScene::setupListeners() {
    // Back button — hide the overlay
    _backButton->addListener([this](const std::string& name, bool down) {
        if (!down) _pendingClose = true;
    });

    // Persist all current settings to disk then close the scene
    _saveButton->addListener([this](const std::string& name, bool down) {
        if (!down) {
            saveSettings();
            _pendingClose = true;
        }
    });

    _sfxSlider->addListener([this](const std::string& name, float value) {
        _sfxVolume = value;
        if (_onSFXVolumeChange) _onSFXVolumeChange(value);
    });

    _musicSlider->addListener([this](const std::string& name, float value) {
        _musicVolume = value;
        if (_onMusicVolumeChange) _onMusicVolumeChange(value);
    });

    // Effects toggle
    _effectsButton->addListener([this](const std::string& name, bool down) {
        if (!down) _effectsEnabled = !_effectsEnabled;
    });

    // Haptics toggle
    _hapticsButton->addListener([this](const std::string& name, bool down) {
        if (!down) _hapticsEnabled = !_hapticsEnabled;
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void SettingsScene::dispose() {
    if (_active) {
        removeAllChildren();
        _usernameField = nullptr;
        _backButton = nullptr;
        _sfxSlider = nullptr;
        _musicSlider = nullptr;
        _effectsButton = nullptr;
        _hapticsButton = nullptr;
        _active = false;
        _saveButton = nullptr;
    }
}

/**
 * Sets whether the scene is currently active.
 *
 * When activated, pre-populates the username field with the player name
 * currently stored in SavedDataManager so returning players always see
 * their existing name. When deactivated, resets button states and
 * disables all input controls.
 *
 * @param value whether the scene is currently active.
 */
void SettingsScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        setInputEnabled(value);
        if (value) {
            // Pre-populate the name field with the currently saved name
            // so the player can see and edit their existing value
            const std::string& saved = SavedDataManager::get().getPlayerName();
            if (_usernameField && !saved.empty()) {
                _usernameField->setText(saved);
                auto placeholder = std::dynamic_pointer_cast<scene2::Label>(
                    _assets->get<scene2::SceneNode>("settingsScene.username.placeholder"));
                if (placeholder) placeholder->setVisible(false);
            }
        } else {
            _saveButton->setDown(false);
            _backButton->setDown(false);
        }
    }
}

/**
 * Updates the scene each frame.
 *
 * Currently a no-op — the settings scene is fully event-driven.
 * Reserved for future use (e.g. animated transitions).
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void SettingsScene::update(float timestep) {
}

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
void SettingsScene::setInputEnabled(bool enabled) {
    if (enabled) {
        _usernameField->activate();
        _backButton->activate();
        _sfxSlider->activate();
        _musicSlider->activate();
        _effectsButton->activate();
        _hapticsButton->activate();
        _saveButton->activate();
    } else {
        _usernameField->deactivate();
        _backButton->deactivate();
        _sfxSlider->deactivate();
        _musicSlider->deactivate();
        _effectsButton->deactivate();
        _hapticsButton->deactivate();
        _saveButton->deactivate();
    }
}

/**
 * Persists the current settings to disk via SavedDataManager.
 *
 * Reads the current text from the username field and stores it in
 * SavedDataManager, then calls save() to write savedData.json. Called
 * by the save button listener immediately before closing the scene.
 *
 * Extend this method to persist slider values and toggle states once
 * those fields are added to the SavedDataManager schema.
 */
void SettingsScene::saveSettings() {
    if (_usernameField) {
        const std::string name = _usernameField->getText();
        // Only overwrite the saved name if the field is non-empty;
        // leaving it blank should not erase a previously saved name
        if (!name.empty()) {
            SavedDataManager::get().setPlayerName(name);
        }
    }
    SavedDataManager::get().save();
}

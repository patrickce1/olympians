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
        _assets->get<scene2::SceneNode>("clientScene.center.playerName.text"));
    
    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.back"));
    
    _sfxSlider = std::dynamic_pointer_cast<scene2::Slider>(
        _assets->get<scene2::SceneNode>("clientScene.back"));;
    
    _musicSlider = std::dynamic_pointer_cast<scene2::Slider>(
        _assets->get<scene2::SceneNode>("clientScene.back"));;
    
    /** The toggle button for screen effects */
    _effectsButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.back"));
    
    /** The toggle button for haptics */
    _hapticsButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.back"));
    
    _saveButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("clientScene.back"));
    
    std::shared_ptr<cugl::scene2::Label> usernamePlaceholder = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("clientScene.center.playerName.placeholder"));
    usernamePlaceholder->setText("ENTER NAME");
    
    // Set the placeholders to invisible when typing starts
    _usernameField->addTypeListener([this, usernamePlaceholder](const std::string& name, const std::string& value) {
        usernamePlaceholder->setVisible(value.empty());
    });
}

/**
 * Attaches input listeners to the settings scene UI controls.
 *
 * This method assigns callbacks for adjusting a slider or returning to the
 * previous menu. It also attaches typing listeners to the username text field
 *  to toggle the visibility of its placeholder label.
 */
void SettingsScene::setupListeners() {
    
};

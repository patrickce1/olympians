#include "MenuScene.h"

using namespace cugl;
using namespace cugl::scene2;

#define MENU_HEIGHT 852

/**
 * Initializes this scene from loaded assets.
 *
 * Expected assets include a Scene2 node named `menuScene` containing child
 * buttons `play`, `settings`, and `items` under a `menu` node.
 *
 * @param assets    The loaded asset manager
 *
 * @return true if initialization succeeds; false otherwise.
 */
bool MenuScene::init(const std::shared_ptr<cugl::AssetManager>& assets, AudioController* audio) {
    if (assets == nullptr) {
        return false;
    }
    if (!Scene2::initWithHint(Size(0, MENU_HEIGHT))) {
        return false;
    }

    // Retrieve menuScene.json
    _assets = assets;
    _audio = audio;
    _scene = _assets->get<scene2::SceneNode>("menuScene");
    if (!_scene) {
        CULog("MenuScene: missing scene2 asset 'menuScene'");
        return false;
    }

    _scene->setContentSize(getSize());
    _scene->doLayout();

    auto menuNode = _scene->getChildByName("menu");
    if (!menuNode) {
        CULog("MenuScene: missing node 'menu'");
        return false;
    }

    // Retrieve and set up buttons
    _playButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("play"));
    _settingsButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("settings"));

    if (!_playButton || !_settingsButton) {
        CULog("MenuScene: expected buttons 'play' and 'settings'");
        return false;
    }

    _settingsButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            if (_audio) _audio->playSoundUnique("tabswap");
            CULog("MenuScene: Settings pressed (placeholder)");
            _status = Status::OPEN_SETTINGS;
        }
    });
    
    // Enter name pop up
    _namePopup = _scene->getChildByName("namePopup");
    if (!_namePopup) {
        CULog("MenuScene: missing 'namePopup' node — onboarding disabled");
    } else {
        _namePopup->setVisible(false);

        auto contents = _namePopup->getChildByName("contents");
        auto nameFieldContainer = contents ? contents->getChildByName("nameField") : nullptr;
        if (nameFieldContainer) {
            _nameField = std::dynamic_pointer_cast<scene2::TextField>(
                nameFieldContainer->getChildByName("text"));

            auto placeholder = std::dynamic_pointer_cast<scene2::Label>(
                nameFieldContainer->getChildByName("placeholder"));
            if (placeholder) {
                placeholder->setText("ENTER NAME");
                // Hide the placeholder label as soon as the user starts typing
                if (_nameField) {
                    _nameField->addTypeListener([placeholder](const std::string&, const std::string& value) {
                        placeholder->setVisible(value.empty());
                    });
                }
            }
        }

        _nameSaveButton = std::dynamic_pointer_cast<scene2::Button>(contents ? contents->getChildByName("save") : nullptr);

        if (_nameSaveButton) {
            _nameSaveButton->addListener([this](const std::string&, bool down) {
                if (!down) {
                    std::string name = _nameField ? _nameField->getText() : "";

                    // Require a non-empty name before proceeding
                    if (name.empty()) return;

                    if (_audio) _audio->playSoundUnique("page_turn");

                    // Persist immediately — safe, no UI changes here
                    SavedDataManager::get().setPlayerName(name);
                    SavedDataManager::get().save();

                    // Defer all UI deactivation to update() via PENDING_SAVE.
                    // Calling deactivate() here corrupts the mouse release listener
                    // iterator we are currently inside, causing EXC_BAD_ACCESS.
                    _status = Status::PENDING_SAVE;
                }
            });
        }
    }

    // Confirm pop up
    _confirmPopup = _scene->getChildByName("confirmPopup");
    if (!_confirmPopup) {
        CULog("MenuScene: missing 'confirmPopup' node");
    } else {
        _confirmPopup->setVisible(false);
    }
    
    _playButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            if (SavedDataManager::get().hasPlayerName()) {
                if (_audio) _audio->playSoundUnique("page_turn");
                _status = Status::START_GAME;
            } else {
                _status = Status::PENDING_ONBOARDING;
            }
        }
    });

    addChild(_scene);
    setActive(false);
    return true;
}

/**
 * Disposes all resources allocated by this scene.
 *
 * This clears the scene graph references, button handles, and any queued
 * menu action.
 */
void MenuScene::dispose() {
    removeAllChildren();
    _playButton        = nullptr;
    _settingsButton    = nullptr;
    _namePopup         = nullptr;
    _nameField         = nullptr;
    _nameSaveButton    = nullptr;
    _confirmPopup      = nullptr;
    _scene             = nullptr;
    _assets            = nullptr;
    _status            = Status::NONE;
    _overlayState      = OverlayState::HIDDEN;
    _active            = false;
}

/**
 * Sets whether this scene is currently active.
 *
 * When active, menu buttons receive input. When inactive, they are
 * explicitly deactivated.
 *
 * @param value whether this scene should be active
 */
void MenuScene::setActive(bool value) {
    if (isActive() == value) {
        return;
    }

    Scene2::setActive(value);
    if (value) {
        _status = Status::NONE;
        _overlayState = OverlayState::HIDDEN;
        _confirmTimer = 0.0f;

        // Always hide both popups and deactivate their inputs on
        // re-activation so no stale listener state carries over
        if (_namePopup)    _namePopup->setVisible(false);
        if (_confirmPopup) _confirmPopup->setVisible(false);
        if (_nameField)    _nameField->deactivate();
        if (_nameSaveButton) {
            _nameSaveButton->deactivate();
            _nameSaveButton->setDown(false);
        }

        // Only activate the main menu buttons
        if (_playButton)     _playButton->activate();
        if (_settingsButton) _settingsButton->activate();

    } else {
        // Deactivate everything — main menu and any open overlay inputs
        if (_playButton) {
            _playButton->deactivate();
            _playButton->setDown(false);
        }
        if (_settingsButton) {
            _settingsButton->deactivate();
            _settingsButton->setDown(false);
        }
        if (_nameField) {
            _nameField->deactivate();
        }
        if (_nameSaveButton) {
            _nameSaveButton->deactivate();
            _nameSaveButton->setDown(false);
        }
    }
}

/**
 * Updates this scene.
 *
 * This scene uses event listeners for button input, so update currently
 * performs no per-frame logic beyond activity checks.
 *
 * @param dt    The elapsed time since the previous frame, in seconds
 */
void MenuScene::update(float dt) {
    if (!_active) return;

    if (_status == Status::OPEN_SETTINGS) {
        if (_settingsButton) {
            _settingsButton->deactivate();
            _settingsButton->setDown(false);
        }
    }

    if (_status == Status::PENDING_ONBOARDING) {
        if (!_namePopup || !_nameField || !_nameSaveButton) {
            CULog("MenuScene: popup nodes missing, skipping onboarding");
            _status = Status::START_GAME;
        } else {
            _namePopup->setVisible(true);
            _nameField->activate();
            _nameSaveButton->activate();
            _overlayState = OverlayState::NAME_PROMPT;
            _status       = Status::NAME_ONBOARDING;
            _playButton->deactivate();
            _settingsButton->deactivate();
        }
    }

    if (_status == Status::PENDING_SAVE) {
        _namePopup->setVisible(false);
        _nameField->deactivate();
        _nameSaveButton->deactivate();
        if (_confirmPopup) _confirmPopup->setVisible(true);
        _confirmTimer = 0.0f;
        _overlayState = OverlayState::CONFIRM;
        _status       = Status::NAME_ONBOARDING;
    }

    if (_overlayState == OverlayState::CONFIRM) {
        _confirmTimer += dt;
        if (_confirmTimer >= CONFIRM_DISPLAY_TIME) {
            if (_confirmPopup) _confirmPopup->setVisible(false);
            _overlayState = OverlayState::HIDDEN;
            _status       = Status::START_GAME;
        }
    }
}

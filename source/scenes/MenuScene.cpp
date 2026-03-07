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
bool MenuScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    if (assets == nullptr) {
        return false;
    }
    if (!Scene2::initWithHint(Size(0, MENU_HEIGHT))) {
        return false;
    }

    // Retrieve menuScene.json
    _assets = assets;
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
    _joinButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("join"));

    if (!_playButton || !_settingsButton || !_joinButton) {
        CULog("MenuScene: expected buttons 'play', 'settings', and 'join'");
        return false;
    }

    // Give play button a function
    _playButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            CULog("MenuScene: Play pressed");
            _nextAction = Action::START_GAME;
        }
    });

    // Give settings button a function
    _settingsButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            CULog("MenuScene: Settings pressed (placeholder)");
            _nextAction = Action::OPEN_SETTINGS;
        }
    });

    // Give items button a function
    _joinButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            CULog("MenuScene: Join pressed (placeholder)");
            _nextAction = Action::JOIN_GAME;
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
    _playButton = nullptr;
    _settingsButton = nullptr;
    _joinButton = nullptr;
    _scene = nullptr;
    _assets = nullptr;
    _nextAction = Action::NONE;
    _active = false;
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
        if (_playButton) {
            _playButton->activate();
        }
        if (_settingsButton) {
            _settingsButton->activate();
        }
        if (_joinButton) {
            _joinButton->activate();
        }
    } else {
        if (_playButton) {
            _playButton->deactivate();
            _playButton->setDown(false);
        }
        if (_settingsButton) {
            _settingsButton->deactivate();
            _settingsButton->setDown(false);
        }
        if (_joinButton) {
            _joinButton->deactivate();
            _joinButton->setDown(false);
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
    if (!_active) {
        return;
    }
    (void)dt;
}

/**
 * Returns and clears the pending menu action.
 *
 * This allows `SceneLoader` to consume a one-shot transition request from
 * this scene each frame.
 *
 * @return the queued action, or Action::NONE if no action is pending
 */
MenuScene::Action MenuScene::consumeAction() {
    Action action = _nextAction;
    _nextAction = Action::NONE;
    return action;
}

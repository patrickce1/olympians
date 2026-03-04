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

    _playButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("play"));
    _settingsButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("settings"));
    _itemsButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("items"));

    if (!_playButton || !_settingsButton || !_itemsButton) {
        CULog("MenuScene: expected buttons 'play', 'settings', and 'items'");
        return false;
    }

    _playButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            CULog("MenuScene: Play pressed");
            _nextAction = Action::START_GAME;
        }
    });

    _settingsButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            CULog("MenuScene: Settings pressed (placeholder)");
            _nextAction = Action::OPEN_SETTINGS;
        }
    });

    _itemsButton->addListener([this](const std::string&, bool down) {
        if (!down) {
            CULog("MenuScene: Items pressed (placeholder)");
            _nextAction = Action::OPEN_ITEMS;
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
    _itemsButton = nullptr;
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
        if (_itemsButton) {
            _itemsButton->activate();
        }
    } else {
        if (_playButton) {
            _playButton->deactivate();
        }
        if (_settingsButton) {
            _settingsButton->deactivate();
        }
        if (_itemsButton) {
            _itemsButton->deactivate();
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

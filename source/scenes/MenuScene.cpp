#include "MenuScene.h"

using namespace cugl;
using namespace cugl::scene2;

#define MENU_HEIGHT 852

bool MenuScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    CULog("[MenuScene] starting init.");
    if (assets == nullptr) {
        return false;
    }
    if (!Scene2::initWithHint(Size(0, MENU_HEIGHT))) {
        return false;
    }

    _assets = assets;
    CULog("[MenuScene] assets loaded.");
    _scene = _assets->get<scene2::SceneNode>("menuScene");
    if (!_scene) {
        CULog("MenuScene: missing scene2 asset 'menuScene'");
        return false;
    }
    
    CULog("[MenuScene] menuScene.json received.");

    _scene->setContentSize(getSize());
    _scene->doLayout();

    auto menuNode = _scene->getChildByName("menu");
    if (!menuNode) {
        CULog("MenuScene: missing node 'menu'");
        return false;
    }
    
    CULog("[MenuScene] child menu loaded.");

    _playButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("play"));
    _settingsButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("settings"));
    _itemsButton = std::dynamic_pointer_cast<Button>(menuNode->getChildByName("items"));

    if (!_playButton || !_settingsButton || !_itemsButton) {
        CULog("MenuScene: expected buttons 'play', 'settings', and 'items'");
        return false;
    }
    
    CULog("[MenuScene] buttons loaded.");

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

void MenuScene::update(float dt) {
    if (!_active) {
        return;
    }
    (void)dt;
}

MenuScene::Action MenuScene::consumeAction() {
    Action action = _nextAction;
    _nextAction = Action::NONE;
    return action;
}

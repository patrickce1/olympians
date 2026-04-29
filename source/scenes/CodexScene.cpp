#include "CodexScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852


#pragma mark -
#pragma mark Provided Methods

/**
 * Initializes the codex scene.
 *
 * This method sets up all UI elements, binds necessary callbacks,
 * and stores references to shared resources such as the asset manager
 * and network controller. It prepares the scene for use but does not
 * make it active or responsive to input.
 *
 * Activation and input handling are controlled separately via setActive().
 *
 * @param assets                           The loaded asset manager used to retrieve scene resources
 * @param networkController   The network controller used for multiplayer communication
 *
 * @return true if the scene was successfully initialized; false otherwise
 */
bool CodexScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up asset manager, network controller, and enemy loader
    _assets = assets;
    _network = networkController;
    
    Size dimen = getSize();
    
    // Acquire the scene built by the asset loader and resize it the scene
    _scene = _assets->get<scene2::SceneNode>("codexScene");
    _scene->setContentSize(dimen);
    _scene->doLayout(); // Repositions the HUD

    setupUI();
    loadItemCodex();
    initItemButtons();
    setupListeners();
    
    _status = Status::WAIT;
    
    addChild(_scene);
    setActive(false);
    return true;
}

/**
 
 */
void CodexScene::initItemButtons() {
    for (int i = 0; i < _items.size(); i++) {
        auto button = std::dynamic_pointer_cast<cugl::scene2::Button>(
            _codexGrid->getChildByName(_items[i].id)
        );
        
        if (button == nullptr) {
            CULog("Could not find grid button for item: %s", _items[i].id.c_str());
            continue;
        }
        _itemNodes.push_back(button);
    }
    
    for (int i = 0; i < _items.size(); i++) {
        auto key = _itemNodes[i]->addListener([this, i](const std::string& name, bool down) {
            if (!down || !_active) return;
            if (down && _selectedIndex == -1) {
                _selectedIndex = i;
                showDetailPanel(_items[i]);
            }
            
        });
        _itemListenerKeys.push_back(key);
    }
}

/**
 
 */
bool CodexScene::loadItemCodex() {
    // Load the JSON asset
    std::shared_ptr<cugl::JsonValue> json = _assets->get<cugl::JsonValue>("itemCodex");
    if (json == nullptr) return false;

    std::shared_ptr<cugl::JsonValue> itemArray = json->get("items");
    
    for (int i = 0; i < itemArray->size(); i++) {
        std::shared_ptr<cugl::JsonValue> entry = itemArray->get(i);
        
        CodexItem item;
        item.id          = entry->getString("id");
        item.name        = entry->getString("name");
        item.imageLarge  = entry->getString("image_large");
        item.rarity      = entry->getString("rarity");
        item.house       = entry->getString("house");
        item.category    = entry->getString("category");
        item.effectLabel = entry->getString("effect_label");
        item.description = entry->getString("description");
        
        _items.push_back(item);
    }
    return true;
}

/**
 * Retrieves and stores references to the CodexScene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * item buttons, back button, navigation
 * buttons, and the item container. It also initializes the
 * grid item list.
 */
void CodexScene::setupUI() {

    _backButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("codexScene.back"));
    
    _scrollUp = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("codexScene.items.scrolldown"));
    
    _scrollDown = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("codexScene.items.scrollup"));
    
    _codexGrid = _assets->get<scene2::SceneNode>("codexScene.items.codex");
    
    _itemsNode = _assets->get<scene2::SceneNode>("codexScene.items");
    
    // Scroll panel
    _detailPanel = _assets->get<scene2::SceneNode>("codexScene.scroll");
    
    _nameLabel = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("codexScene.scroll.itemName"));
    
    _rarityLabel = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("codexScene.scroll.info.rarity.label"));
    
    _categoryLabel = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("codexScene.scroll.info.type.header"));
    
    _effectLabel = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("codexScene.scroll.info.type.description"));
    
    _descriptionLabel = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("codexScene.scroll.description"));
    
    
    // overlay content
    _darkOverlay = _assets->get<scene2::SceneNode>("codexScene.darkOverlay");
    
    _itemLarge = std::dynamic_pointer_cast<scene2::PolygonNode>(
            _assets->get<scene2::SceneNode>("codexScene.itemLarge"));
}

/**
 * Attaches input listeners to the codex buttons.
 */
void CodexScene::setupListeners() {
    
    _backButton->addListener([this](const std::string& name, bool down) {
        if (!down || !_active) return;
        if (down) {
            if (_status == Status::INFO) {
                _pendingHideDetail = true;
            } else {
                _status = Status::ABORT;
            }
        }
    });
    
    int numRows = (int)std::ceil(_items.size() / 3.0f);
    int visibleRows = (int)std::ceil(_pageHeight / _rowHeight);

    _maxRow = std::max(0, numRows - visibleRows);

    _currentRow = 0;

    _scrollUp->addListener([this](const std::string& name, bool down) {
        if (!down || !_active) return;
        if (down && _selectedIndex == -1) scroll(_currentRow-1);
    });

    _scrollDown->addListener([this](const std::string& name, bool down) {
        if (!down || !_active) return;
        if (down && _selectedIndex == -1) scroll(_currentRow+1);
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void CodexScene::dispose() {
    if (_active) {
        removeAllChildren();
        for (int i = 0; i < _itemNodes.size(); i++) {
            _itemNodes[i]->removeListener(_itemListenerKeys[i]);
        }
        _backButton->clearListeners();
        _scrollUp->clearListeners();
        _scrollDown->clearListeners();
        _backButton = nullptr;
        _scrollUp = nullptr;
        _scrollDown = nullptr;
        _codexGrid = nullptr;
        _itemNodes.clear();
        _itemLarge = nullptr;
        _detailPanel = nullptr;
        _darkOverlay = nullptr;
        _itemsNode = nullptr;
        _nameLabel = nullptr;
        _rarityLabel = nullptr;
        _categoryLabel = nullptr;
        _effectLabel = nullptr;
        _descriptionLabel = nullptr;

        _scene = nullptr;
        _items.clear();
        _active = false;
    }
    _network = nullptr;
}

/**
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.  Buttons
 * should be activated when it is made active and deactivated when
 * it is not.
 *
 * @param value whether the scene is currently active
 */
void CodexScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = WAIT;
            
            for (auto button : _itemNodes) {
                button->activate();
            }
            _scrollUp->activate();
            _scrollDown->activate();
            _backButton->activate();
            
        } else {
            for (auto button : _itemNodes) {
                button->deactivate();
                button->setDown(false);
            }
            _scrollUp->deactivate();
            _scrollDown->deactivate();
            _backButton->deactivate();
            
//            // If any were pressed, reset them
            _backButton->setDown(false);
            _scrollUp->setDown(false);
            _scrollDown->setDown(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void CodexScene::update(float timestep) {
    // Kick client if host terminated the session
    if (!_network->isHost()) {
        if (_network->checkConnection() != NetworkController::Status::CONNECTED) {
            _status = Status::ABORT;
            return;
        }
        _network->getNetworkUpdates();
        if (_network->wasSessionTerminated()) {
            _network->clearQueues();
            _network->disconnect();
            _status = Status::ABORT;
            return;
        }
    }
    
    // Forward to pre game scene if host started while we were here
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
        return;
    }
    
    if (_pendingShowDetail) {
        _pendingShowDetail = false;
        _status = Status::INFO;
        
        for (auto button : _itemNodes) button->deactivate();
        _scrollUp->deactivate();
        _scrollDown->deactivate();
        
        const CodexItem& item = _items[_pendingDetailIndex];
        _nameLabel->setText(item.name);
        _rarityLabel->setText(item.rarity);
        _categoryLabel->setText(item.category);
        _effectLabel->setText(item.effectLabel);
        _descriptionLabel->setText(item.description);
        // ... color and texture setup ...
        
        _darkOverlay->setVisible(true);
        _itemLarge->setVisible(true);
        _detailPanel->setVisible(true);
    }
    
    hideDetailPanel();
}

/**
 
 */
void CodexScene::showDetailPanel(const CodexItem& item) {
    _pendingShowDetail = true;
    _pendingDetailIndex = _selectedIndex;
}

void CodexScene::scroll(int newRow) {
    if (_isScrolling) return;
        if (newRow < 0 || newRow > _maxRow) return;

        _isScrolling = true;

        float shiftAmount = _rowHeight;

        int delta = newRow - _currentRow;

        Vec2 currentPos = _codexGrid->getPosition();
        float targetY = currentPos.y - (delta * shiftAmount);

        // apply movement
        _codexGrid->setPosition(currentPos.x, targetY);

        _currentRow = newRow;

        // update scroll UI
        _scrollUp->setVisible(_currentRow > 0);
        _scrollDown->setVisible(_currentRow < _maxRow);

        _isScrolling = false;
}

void CodexScene::hideDetailPanel() {
    if (_pendingHideDetail) {
        _pendingHideDetail = false;
        
        _darkOverlay->setVisible(false);
        _itemLarge->setVisible(false);
        _detailPanel->setVisible(false);
        _selectedIndex = -1;
        
        _status = Status::WAIT;
        
        for (auto button : _itemNodes) button->activate();
        _scrollUp->activate();
        _scrollDown->activate();
    }
}

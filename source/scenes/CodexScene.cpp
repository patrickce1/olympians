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
//    setupListeners();
    loadItemCodex();
    initItemButtons();
    
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
        
        button->addListener([this, i](const std::string& name, bool down) {
            if (down) {
                _selectedIndex = i;
                showDetailPanel(_items[i]);
            }
            
        });
        button->activate();
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
        _assets->get<scene2::SceneNode>("codexScene.items.scrollup"));
    
    _scrollDown = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("codexScene.items.scrolldown"));
    
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
        if (down) {
            _status = Status::ABORT;
        }
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void CodexScene::dispose() {
    if (_active) {
        removeAllChildren();
        _backButton = nullptr;
        _scrollUp = nullptr;
        _scrollDown = nullptr;
        _codexGrid = nullptr;
        _itemNodes.clear();
        _itemLarge = nullptr;
        _detailPanel = nullptr;
        _darkOverlay = nullptr;
        _itemsNode = nullptr;
        // ---- Detail Panel Labels ----
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
            
//            _leftButton->activate();
//            _rightButton->activate();
//            _backButton->activate();
        } else {
//            _leftButton->deactivate();
//            _rightButton->deactivate();
//            _backButton->deactivate();
//            _lockButton->deactivate();
            
//            // If any were pressed, reset them
//            _backButton->setDown(false);
//            _leftButton->setDown(false);
//            _rightButton->setDown(false);
//            _lockButton->setDown(false);
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
}

/**
 
 */
void CodexScene::showDetailPanel(const CodexItem& item) {
    _nameLabel->setText(item.name);
    _rarityLabel->setText(item.rarity);
    _categoryLabel->setText(item.category);
    _effectLabel->setText(item.effectLabel);
    _descriptionLabel->setText(item.description);
    
    if (item.category == "ATTACK") {
        _categoryLabel->setForeground(cugl::Color4("#AC0000ff"));
        _effectLabel->setForeground(cugl::Color4("#AC0000ff"));
    } else if (item.category == "SUPPORT") {
        _categoryLabel->setForeground(cugl::Color4("#047D04ff"));
        _effectLabel->setForeground(cugl::Color4("#047D04ff"));
    } else {
        _categoryLabel->setForeground(cugl::Color4("#2000ACff"));
        _effectLabel->setForeground(cugl::Color4("#2000ACff"));
    }

    auto texture = _assets->get<cugl::graphics::Texture>("itemLarge");
    _itemLarge->setTexture(texture);

    _darkOverlay->setVisible(true);
    _itemLarge->setVisible(true);
    _detailPanel->setVisible(true);
}

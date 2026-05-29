#include "CodexScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Max number of items per row */
#define ITEMS_PER_ROW  3
/** Interpolation smoothing factor*/
#define SMOOTHING_FACTOR 0.2f
/** Minimum vertical pixel distance to commit to a swipe direction. */
static constexpr float SWIPE_THRESHOLD = 40.0f;

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
 * @param assets       The loaded asset manager used to retrieve scene resources
 * @param networkController   The network controller used for multiplayer communication
 * @param audio    The audio controller used for various sounds.
 *
 * @return true if the scene was successfully initialized; false otherwise
 */
bool CodexScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController, AudioController* audio) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up asset manager, network controller, and enemy loader
    _assets = assets;
    _network = networkController;
    _audio = audio;
    
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
 * Initializes interactive buttons for the in-game item in a grid. These buttons
 * when clicked open up the respective detail panel explaining the use of
 * the item.
 */
void CodexScene::initItemButtons() {
    int i = 0; // index into _items
    int rowIndex = 0;
    
    while (i < _items.size()) {
        std::vector<std::shared_ptr<scene2::Button>> row;
        
        int itemsThisRow = ITEMS_PER_ROW;
        
        // special case: common is only 5 items
        if (rowIndex == 1) {
            itemsThisRow = 2;
        }
        
        for (int j = 0; j < itemsThisRow && i < _items.size(); j++, i++) {
            auto button = std::dynamic_pointer_cast<cugl::scene2::Button>(
                    _codexGrid->getChildByName(_items[i].id)
            );
            
            if (button == nullptr) {
                CULog("Could not find grid button for item: %s", _items[i].id.c_str());
                continue;
            }
            
            row.push_back(button);
        }
        
        _itemNodes.push_back(row);
        rowIndex++;
    }
    
    // button specific listeners
    int index = 0;
    for (auto& row : _itemNodes) {
        for (auto& button : row) {
            ButtonHelpers::addTapListener(button, [this, index] {
                if (!_active) return;
                if (_audio) _audio->playSoundUnique("page_turn");
                if (_selectedIndex == -1) {
                    _selectedIndex = index;
                    showDetailPanel(_items[index]);
                }
            });
            index++;
        }
    }
}

/**
 * Loads codex item data from JSON asset files.
 *
 * @return true if loading succeeded
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
    _darkOverlay = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("codexScene.darkOverlay"));
    
    _itemLarge = std::dynamic_pointer_cast<scene2::PolygonNode>(
            _assets->get<scene2::SceneNode>("codexScene.itemLarge"));
}

/**
 * Attaches input listeners to the codex buttons.
 */
void CodexScene::setupListeners() {
    ButtonHelpers::addTapListener(_backButton, [this] {
        if (!_active) return;
        if (_audio) _audio->playSoundUnique("page_turn");
        if (_status == Status::INFO) {
            _pendingHideDetail = true;
        } else {
            _status = Status::ABORT;
        }
    });

    int numRows = (int)_itemNodes.size();
    int visibleRows = 6;
    _maxRow  = std::max(0, numRows - visibleRows);
    _currentRow = 0;
    
    // Capture the grid's Y position at row 0 so tracking and snapping
    // can compute row positions as offsets from this base.
    if (_codexGrid) {
        _baseGridPositionY = _codexGrid->getPosition().y;
    }

    // Scroll buttons hidden — swiping handles scrolling instead.
    _scrollUp->setVisible(false);
    _scrollDown->setVisible(false);
    
    ButtonHelpers::addTapListener(_darkOverlay, [this] {
        if (!_active) return;
        if (_status == Status::INFO) {
            _pendingHideDetail = true;
        }
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void CodexScene::dispose() {
    if (_active) {
        removeAllChildren();
        _backButton->clearListeners();
        _scrollUp->clearListeners();
        _scrollDown->clearListeners();
        _darkOverlay->clearListeners();
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
    _audio = nullptr;
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
            
            updateButtonVisibility();
            // Scroll buttons are replaced by swipe — keep deactivated.
            _scrollUp->deactivate();
            _scrollDown->deactivate();
            _backButton->activate();
            _darkOverlay->activate();
            _swipeContainerStartY = 0.0f;
            _isSnapping  = false;
            _snapTarget = cugl::Vec2::ZERO;
            
        } else {
            for (auto& row : _itemNodes) {
                for (auto& button : row) {
                    button->deactivate();
                    button->setDown(false);
                }
            }
            _scrollUp->deactivate();
            _scrollDown->deactivate();
            _backButton->deactivate();
            _darkOverlay->deactivate();
            
            // If any were pressed, reset them
            _backButton->setDown(false);
            _scrollUp->setDown(false);
            _scrollDown->setDown(false);
            _darkOverlay->setDown(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 * @param input         The input controller instance
 */
void CodexScene::update(float timestep, InputController& input) {
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
    
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
        return;
    }
    
    if (_pendingShowDetail) {
        _pendingShowDetail = false;
        _status = Status::INFO;
        
        for (auto& row : _itemNodes) {
            for (auto& button : row) {
                button->deactivate();
            }
        }
        
        const CodexItem& item = _items[_pendingDetailIndex];
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
            _effectLabel->setForeground(cugl::Color4("#164E18ff"));
        } else {
            _categoryLabel->setForeground(cugl::Color4("#2000ACff"));
            _effectLabel->setForeground(cugl::Color4("#2000ACff"));
        }
        
        size_t effectLen = item.effectLabel.length();
        auto typeNode = _effectLabel->getParent();
        float totalWidth = 294.0f;
        float rarityWidth = 70.0f;
        float desiredWidth = std::min((float)effectLen * 8.0f, totalWidth - rarityWidth);
        desiredWidth = std::max(desiredWidth, 80.0f);
        typeNode->setContentSize(Size(desiredWidth, typeNode->getContentSize().height));
        typeNode->doLayout();
        typeNode->getParent()->doLayout();
        
        auto texture = _assets->get<cugl::graphics::Texture>(item.imageLarge);
        _itemLarge->setTexture(texture);
        _itemLarge->setScale(0.5f);
        _darkOverlay->setVisible(true);
        _itemLarge->setVisible(true);
        _detailPanel->setVisible(true);
    }
    
    hideDetailPanel();

    // Only process swipes when not in detail view — swipes shouldn't
    // scroll the grid while the detail panel is open.
    if (_status != Status::INFO) {
        handleSwipeBegin(input);
        handleSwipeTracking(input);
        handleSwipeRelease(input);
    }
    
    // Lerp the grid toward the snap target after a swipe release,
    // mirroring how BossSelectScene lerps its container to _slideTarget.
    if (_isSnapping) {
        Vec2 current = _codexGrid->getPosition();
        Vec2 next    = current.lerp(_snapTarget, SMOOTHING_FACTOR);

        if (current.distance(_snapTarget) < 1.0f) {
            _codexGrid->setPosition(_snapTarget);
            _isSnapping = false;
            updateButtonVisibility();
        } else {
            _codexGrid->setPosition(next);
        }
    }
}

/**
 * Displays the detail panel for a selected item.
 *
 * @param item The codex item to display
 */
void CodexScene::showDetailPanel(const CodexItem& item) {
    _pendingShowDetail = true;
    _pendingDetailIndex = _selectedIndex;
}

/**
 * Hides the currently open detail panel.
 */
void CodexScene::hideDetailPanel() {
    if (_pendingHideDetail) {
        _pendingHideDetail = false;
        
        _darkOverlay->setVisible(false);
        _itemLarge->setVisible(false);
        _detailPanel->setVisible(false);
        _selectedIndex = -1;
        
        _status = Status::WAIT;
        
        updateButtonVisibility();
        _scrollUp->activate();
        _scrollDown->activate();
    }
}

/**
 * Updates which item buttons are visible based on scroll position.
 */
void CodexScene::updateButtonVisibility() {
    const int VISIBLE_ROWS = 6;

    for (int r = 0; r < _itemNodes.size(); r++) {
        bool rowVisible = (r >= _currentRow && r < _currentRow + VISIBLE_ROWS);

        for (auto& button : _itemNodes[r]) {
            if (rowVisible) {
                button->activate();
            } else {
                button->deactivate();
            }
        }
    }
}

#pragma mark -
#pragma mark Swipe Gesture Handling

/**
 * Records the touch-down position to begin tracking a potential vertical swipe.
 * @param input  The input controller for this frame.
 */
void CodexScene::handleSwipeBegin(InputController& input) {
    if (_isSwiping) return;

    if (!input.isTouching() && !input.isMouseDown()) {
        _swipeHoldFrames      = 0;
        _swipeTouchInitialPos = cugl::Vec2::ZERO;
        return;
    }

    if (_swipeHoldFrames == 0) {
        _swipeTouchInitialPos = input.getTouchStart();
    }

    Vec2 worldCurrent = screenToWorldCoords(input.getDragPos());
    Vec2 worldStart   = screenToWorldCoords(_swipeTouchInitialPos);

    float horizontalDelta = std::abs(worldCurrent.x - worldStart.x);
    float verticalDelta   = std::abs(worldCurrent.y - worldStart.y);

    if (verticalDelta > horizontalDelta && verticalDelta > 5.0f) {
        _swipeHoldFrames++;
    } else {
        _swipeHoldFrames = 0;
    }

    if (_swipeHoldFrames >= SWIPE_HOLD_FRAMES) {
        // Store start positions in world space so tracking delta is correct.
        _swipeTouchStartY     = screenToWorldCoords(_swipeTouchInitialPos).y;
        _swipeContainerStartY = _codexGrid->getPosition().y;
        _isSwiping            = true;
        _swipeHoldFrames      = 0;

        // Deactivate buttons while dragging so taps don't fire mid-swipe.
        for (auto& row : _itemNodes) {
            for (auto& button : row) {
                button->deactivate();
            }
        }
    }
}

/**
 * Moves the grid container directly under the finger each frame,
 * mirroring how BossSelectScene tracks its card container. Computes
 * the vertical delta between the current drag position and the
 * touch-down position (both in world space), then applies that delta
 * to the grid's Y position at the start of the drag. Clamps so the
 * grid cannot scroll past row 0 or the last scrollable row.
 *
 * @param input  The input controller for this frame.
 */
void CodexScene::handleSwipeTracking(InputController& input) {
    if (!_isSwiping) return;
    if (!input.isTouching() && !input.isMouseDown()) return;

    Vec2  worldPos    = screenToWorldCoords(input.getDragPos());
    float fingerDelta = worldPos.y - _swipeTouchStartY;
    float rawY        = _swipeContainerStartY + fingerDelta;

    // Grid starts at _baseGridPositionY (row 0, highest Y).
    // Scrolling down shifts the grid up (Y decreases) to show later rows.
    float maxY     = _baseGridPositionY + 1.5*(_maxRow * _rowHeight);
    float minY     = _baseGridPositionY - 0.5*(_maxRow * _rowHeight); // last row
    float clampedY = std::max(minY, std::min(maxY, rawY));
    Vec2 pos = _codexGrid->getPosition();
    _codexGrid->setPosition(Vec2(pos.x, clampedY));
}

/**
 * On finger lift, reads the grid's current Y position and snaps to
 * the nearest row boundary, mirroring how BossSelectScene's
 * snapToNearestBoss reads the container's X and snaps to the nearest
 * card. Re-activates visible item buttons after snapping.
 *
 * @param input  The input controller for this frame.
 */
void CodexScene::handleSwipeRelease(InputController& input) {
    if (!input.touchEnded()) return;
    if (!_isSwiping) {
        _isSwiping = false;
        return;
    }

    float currentGridY = _codexGrid->getPosition().y;

    float traveled   = currentGridY - _baseGridPositionY;
    float rawRow     = traveled / _rowHeight;
    int nearestRow   = static_cast<int>(std::round(rawRow));
    nearestRow       = std::max(0, std::min(_maxRow, nearestRow));

    // Do not snap immediately — set a lerp target and let update()
    // animate the grid smoothly to the nearest row boundary.
    Vec2 currentPos = _codexGrid->getPosition();
    _snapTarget  = Vec2(currentPos.x, _baseGridPositionY + (nearestRow * _rowHeight));
    _isSnapping  = true;
    _currentRow  = nearestRow;

    // Buttons remain deactivated until the lerp settles in update().

    _isSwiping            = false;
    _swipeTouchStartY     = 0.0f;
    _swipeContainerStartY = 0.0f;
    _swipeTouchInitialPos = cugl::Vec2::ZERO;
}

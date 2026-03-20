#include "HouseSelectScene.h"

using namespace cugl;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** Role card width */
#define ROLE_CARD_WIDTH 300


#pragma mark -
#pragma mark Provided Methods
/**
 * Initializes the controller contents, and starts the game
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
bool HouseSelectScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    _assets = assets;
    _network = networkController;
    
    Size dimen = getSize();
    
    // Acquire the scene built by the asset loader and resize it the scene
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("houseSelectScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    setupUI();
    setupListeners();
    
    _status = Status::WAITING;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the host setup UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * start button, back button, host name text field, carousel navigation
 * buttons, and the role carousel container. It also initializes the
 * carousel item list and configures the placeholder label.
 */
void HouseSelectScene::setupUI() {

    _lockButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.lock"));

    _backOut = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.back"));

    // actually image, make into widget for access
    _playerIcon = (_assets->get<scene2::SceneNode>("houseSelectScene.selectorIcons.playerSelectIcon"));
    
    if (_playerIcon) {
        _playerIconImage = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("emptyLocalIcon"));
        
        _playerIconGlow = std::dynamic_pointer_cast<cugl::scene2::PolygonNode>(
                            _playerIcon->getChildByName("lockedGlow"));
    }
    
    _playerIconImage->setAnchor(cugl::Vec2::ANCHOR_CENTER);

    _leftButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.Carousel_buttons.directionbuttons.leftscroll"));

    _rightButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("houseSelectScene.Carousel_buttons.directionbuttons.rightscroll"));

    _container = _assets->get<scene2::SceneNode>("houseSelectScene.Carousel_buttons.heroCardcont");

    if (_container) {
        for (int i = 0; i < 9; i++) {
            _items.push_back(_container->getChild(i));
        }
    }
    
    auto indicatorContainer = _assets->get<scene2::SceneNode>("houseSelectScene.classSelectionCarouselIcons");
    
    if (indicatorContainer) {
        for (int i = 0; i < 9; i++) {
            _indicators.push_back(indicatorContainer->getChild(i));
        }
    }
}

/**
 * Attaches input listeners to the host setup buttons.
 *
 * This method assigns callbacks for starting the game, returning to the
 * previous menu, and navigating the role selection carousel.
 */
void HouseSelectScene::setupListeners() {
    
    _lockButton->addListener([this](const std::string& name, bool down) {
        if (!down) return;
        _locked = !_locked;

        if (_locked) {
            // Scene Updates
            updateSelectedIcon(_currentIndex);
            updateText(_lockButton, "UNLOCK");
            _playerIconGlow->setVisible(true);
            _status = Status::LOCKED;
        } else {
            updateText(_lockButton, "LOCK");
            _playerIconGlow->setVisible(false);
            _status = Status::WAITING;
        }
    });

    _backOut->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });

    _leftButton->addListener([this](const std::string& name, bool down){
        if (!down) slideTo(_currentIndex - 1);
    });

    _rightButton->addListener([this](const std::string& name, bool down){
        if (!down) slideTo(_currentIndex + 1);
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void HouseSelectScene::dispose() {
    removeAllChildren();
    _lockButton = nullptr;
    _backOut = nullptr;
    _playerIcon = nullptr;
    _playerIconImage = nullptr;
    _playerIconGlow = nullptr;
    _leftButton = nullptr;
    _rightButton = nullptr;
    _container = nullptr;
    _items.clear();
    _indicators.clear();
    _active = false;
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
void HouseSelectScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = WAITING;
            _lockButton->activate();
            _leftButton->activate();
            _rightButton->activate();
            _backOut->activate();
        } else {
            _lockButton->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backOut->deactivate();
            
            // If any were pressed, reset them
            _lockButton->setDown(false);
            _backOut->setDown(false);
            _leftButton->setDown(false);
            _rightButton->setDown(false);
        }
    }
}

/**
 * Updates the text in the given button.
 *
 * Techincally a button does not contain text. A button is simply a scene graph
 * node with one child for the up state and another for the down state. So to
 * change the text in one of our buttons, we have to descend the scene graph.
 * This method simplifies this process for you.
 *
 * @param button    The button to modify
 * @param text      The new text value
 */
void HouseSelectScene::updateText(const std::shared_ptr<scene2::Button>& button, const std::string text) {
    auto label = std::dynamic_pointer_cast<scene2::Label>(button->getChildByName("label"));
    label->setText(text);
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void HouseSelectScene::update(float timestep) {
    // The carousel move logic
    if (_isAnimating) {
        Vec2 current = _container->getPosition();
        Vec2 next = current.lerp(_slideTarget, 0.2f); // 0.2 = smoothing factor

        if (current.distance(_slideTarget) < 1.0f) {
            _container->setPosition(_slideTarget);
            _isAnimating = false;
        } else {
            _container->setPosition(next);
        }
    }
}

/**
 * Reconfigures the lock button for this scene
 *
 * This is necessary because what the buttons do depends on the state of the
 * networking.
 */
void HouseSelectScene::configureLockButton() {
    updateText(_lockButton,"Lock");
    _lockButton->activate();
}

/**
 * Initiates a slide animation to center the item at `newIndex`.
 *
 * Does nothing if an animation is already in progress or if the
 * index is out of bounds. Otherwise computes the target container
 * position and stores it in `_slideTarget`.
 *
 * @param newIndex The index of the item to slide to.
 */
void HouseSelectScene::slideTo(int newIndex) {
    if (_isAnimating) return;
    if (newIndex < 0 || newIndex >= _items.size()) return;

    _isAnimating = true;

    float shiftAmount = ROLE_CARD_WIDTH;
    
    int deltaIndex = newIndex - _currentIndex;
    Vec2 currentPos = _container->getPosition();
    float targetX = currentPos.x - (deltaIndex * shiftAmount);
    
    _slideTarget = Vec2(targetX, currentPos.y);
    _currentIndex = newIndex;
    
    for (int i = 0; i < _items.size(); i++) {
        auto card = _items[i];
        if (card) {
            auto glow = card->getChildByName("glowOverlayHero");
            if (glow){
                glow->setVisible(false);
                if (i == newIndex) {
                    glow->setVisible(true);
                }
            }
        }
    }
    
    updateIndicators(newIndex);
    if (!_locked){
        updateSelectedIcon(newIndex);
    }
}

/**
 * Updates the circular indicators at the bottom of what card in the carousel
 * we are currently at.
 *
 * @param currentIndex The index of the card we are at.
 */
void HouseSelectScene::updateIndicators(int currentIndex) {
    for (int i = 0; i < _indicators.size(); i++) {
        auto node = _indicators[i];
        
        auto fill   = node->getChildByName("fill");
        
        if (i == currentIndex) {
            fill->setColor(Color4("#4c3214ff"));
        } else {
            fill->setColor(Color4("#9d7137ff"));
        }
    }
}

/**
 * Updates the player's respective icon in the diamond based on the house card
 * they are currently on. If the player has locked their house, there is no change.
 *
 * @param currentIndex The index of the card we are at.
 */
void HouseSelectScene::updateSelectedIcon(int currentIndex) {
    if (!_playerIconImage) return;
    
    House house = getHouseFromIndex(currentIndex);
    if (house == House::ATHENA) {
        _playerIconImage->setTexture(_assets->get<cugl::graphics::Texture>("athenaSIcon"));
    } else {
        _playerIconImage->setTexture(_assets->get<cugl::graphics::Texture>("emptyLocalIcon"));
    }
}

/**
 * Gets House from the given index using the House enum
 *
 * @param index The index of the house
 */
House HouseSelectScene::getHouseFromIndex(int index) {
    // assuming the order doesn't change
    return static_cast<House>(index);
}


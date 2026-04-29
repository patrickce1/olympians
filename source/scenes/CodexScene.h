#ifndef __CODEX_SCENE_H__
#define __CODEX_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "../NetworkController.h"


struct CodexItem {
    std::string id;
    std::string name;
    std::string imageLarge;
    std::string rarity;
    std::string house;
    std::string category;
    std::string effectLabel;
    std::string description;
};

/**
 * This class provides the interface to make the item codex scene.
 */
class CodexScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /**  */
        WAIT,
        /** Looking at info in description box, if back button pressed, back to original state*/
        INFO,
        /** Selection was aborted; back to lobby */
        ABORT,
        /** Game scene has been started by host**/
        PRE_GAMESCENE_START
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The network controller shared across all scenes*/
    std::shared_ptr<NetworkController> _network;
    
    std::shared_ptr<cugl::scene2::SceneNode> _scene;
    
    /** The back button for the item codex scene */
    std::shared_ptr<cugl::scene2::Button> _backButton;
    
    /** The scroll down button in the scene */
    std::shared_ptr<cugl::scene2::Button> _scrollDown;
    
    /** The scroll up button in the scene */
    std::shared_ptr<cugl::scene2::Button> _scrollUp;
    
    std::vector<CodexItem> _items;
    int _selectedIndex = -1;          // which item is currently selected (-1 = none)
    
    /** The item codex list */
    std::vector<std::shared_ptr<cugl::scene2::Button>> _itemNodes;
    
    std::shared_ptr<cugl::scene2::SceneNode> _itemsNode;     // the viewport node
    std::shared_ptr<cugl::scene2::SceneNode> _codexGrid;     // the scrollable grid widget
    std::shared_ptr<cugl::scene2::SceneNode> _detailPanel;   // the scroll/detail widget
    std::shared_ptr<cugl::scene2::SceneNode> _darkOverlay;   // dark overlay when item selected
    std::shared_ptr<cugl::scene2::PolygonNode> _itemLarge;   // large item image display
    
    // ---- Detail Panel Labels ----
    std::shared_ptr<cugl::scene2::Label> _nameLabel;
    std::shared_ptr<cugl::scene2::Label> _rarityLabel;
    std::shared_ptr<cugl::scene2::Label> _categoryLabel;
    std::shared_ptr<cugl::scene2::Label> _effectLabel;
    std::shared_ptr<cugl::scene2::Label> _descriptionLabel;
    
    std::vector<unsigned int> _itemListenerKeys;


    float _rowHeight = 80.0f;   // adjust to your grid spacing
    float _pageHeight = 530.0f;  // visible area
    int _currentRow = 0;
    int _maxRow = 0;
    bool _isScrolling = false;
    
    bool _pendingShowDetail = false;
    int  _pendingDetailIndex = -1;
    
    bool _pendingHideDetail = false;
    
    /** The current status */
    Status _status;
    

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new item codex scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    CodexScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~CodexScene() { dispose(); }
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;
    
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
     * @param networkController The network controller shared across all scenes
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController);
    
    /**
     * Retrieves and stores references to the CodexScene UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * item buttons, back button, navigation
     * buttons, and the item container. It also initializes the
     * grid item list.
     */
    void setupUI();
    
    /**
     * Attaches input listeners to the codex buttons.
     */
    void setupListeners();
    
    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.  Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
     *
     * @param value whether the scene is currently active
     */
    virtual void setActive(bool value) override;
    
    /**
     * Returns the scene status.
     *
     * Any value other than WAIT or INFO will transition to a new scene.
     *
     * @return the scene status
     *
     */
    Status getStatus() const { return _status; }

    /**
     * The method called to update the scene.
     *
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep) override;
    
private:
    
    bool loadItemCodex();
        
    void initItemButtons();
    
    void showDetailPanel(const CodexItem& item);
    
    void scroll(int newRow);
    
    void hideDetailPanel();
    
    void updateButtonVisibility();

};

#endif /* __CODEX_SCENE_H__ */


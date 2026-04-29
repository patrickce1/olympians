#ifndef __CODEX_SCENE_H__
#define __CODEX_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "../NetworkController.h"
#include "../InputController.h"

/**
 * Represents a single entry in the codex (item encyclopedia).
 *
 * Each item stores metadata such as its name, rarity, category,
 * description, and associated visual assets.
 */
struct CodexItem {
    std::string id;             /** Unique identifier for the item */
    std::string name;           /** Display name of the item */
    std::string imageLarge;     /** Key for large item image texture */
    std::string rarity;         /** Rarity classification (e.g., Common, Rare) */
    std::string house;          /** Associated house (if applicable) */
    std::string category;       /** Functional category (e.g., ATTACK, SUPPORT) */
    std::string effectLabel;    /** Short effect description or label */
    std::string description;    /** Full lore/description text */
};

/**
 * This scene implements an in-game item encyclopedia (codex) UI.
  * It allows the player to browse items in a grid layout, scroll
  * through entries, and view detailed information in an animated
  * detail panel overlay.
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
    
    /** Root scene node for the codex UI */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;
    
    /** Back button used to exit codex or close item detail view */
    std::shared_ptr<cugl::scene2::Button> _backButton;
    
    /** Button used to scroll the grid downward */
    std::shared_ptr<cugl::scene2::Button> _scrollDown;
    
    /** Button used to scroll the grid upward */
    std::shared_ptr<cugl::scene2::Button> _scrollUp;
    
    /** All codex items loaded from data source */
    std::vector<CodexItem> _items;
    
    /** Index of currently selected item (-1 if none selected) */
    int _selectedIndex = -1;
    
    /** Interactive item buttons displayed in the grid */
    std::vector<std::shared_ptr<cugl::scene2::Button>> _itemNodes;
    
    /** Parent node containing the item grid viewport */
    std::shared_ptr<cugl::scene2::SceneNode> _itemsNode;
    
    /** Scrollable grid container for codex items */
    std::shared_ptr<cugl::scene2::SceneNode> _codexGrid;
    
    /** Detail panel shown when an item is selected */
    std::shared_ptr<cugl::scene2::SceneNode> _detailPanel;
    
    /** Darkened background overlay for focus mode */
    std::shared_ptr<cugl::scene2::SceneNode> _darkOverlay;
    
    /** Large preview image of selected item */
    std::shared_ptr<cugl::scene2::PolygonNode> _itemLarge;   // large item image display
    
    // ---- Detail Panel Labels ----
    /** Displays item name */
    std::shared_ptr<cugl::scene2::Label> _nameLabel;
    
    /** Displays item rarity */
    std::shared_ptr<cugl::scene2::Label> _rarityLabel;
    
    /** Displays item category */
    std::shared_ptr<cugl::scene2::Label> _categoryLabel;

    /** Displays item effect summary */
    std::shared_ptr<cugl::scene2::Label> _effectLabel;
    
    /** Displays full item description */
    std::shared_ptr<cugl::scene2::Label> _descriptionLabel;
    
    /** Listener keys for item button callbacks */
    std::vector<unsigned int> _itemListenerKeys;

    /** Height of a single grid row (used for scrolling calculations) */
    float _rowHeight = 80.0f;

    /** Current scroll row index */
    int _currentRow = 0;
    
    /** Maximum scrollable row index */
    int _maxRow = 0;

    /** Whether a scroll operation is currently in progress */
    bool _isScrolling;
    
    /** Pending request to show item detail panel */
    bool _pendingShowDetail = false;
    
    /** Index of item pending detail display */
    int  _pendingDetailIndex = -1;
    
    /** Pending request to hide the detail panel */
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
     * Attaches input listeners to UI elements such as item buttons
     * and navigation controls.
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
    
    /**
     * Loads codex item data from JSON asset files.
     *
     * @return true if loading succeeded
     */
    bool loadItemCodex();
        
    /**
     * Initializes interactive item buttons for the grid.
     */
    void initItemButtons();
    
    /**
     * Displays the detail panel for a selected item.
     *
     * @param item The codex item to display
     */
    void showDetailPanel(const CodexItem& item);
    
    /**
     * Scrolls the codex grid to a specified row.
     *
     * @param newRow Target row index
     */
    void scroll(int newRow);
    
    /**
     * Hides the currently open detail panel.
     */
    void hideDetailPanel();
    
    /**
     * Updates which item buttons are visible based on scroll position.
     */
    void updateButtonVisibility();

};

#endif /* __CODEX_SCENE_H__ */


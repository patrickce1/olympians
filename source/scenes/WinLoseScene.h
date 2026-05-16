#ifndef __WIN_LOSE_SCENE_H__
#define __WIN_LOSE_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "../EnemyLoader.h"
#include "../NetworkController.h"

/**
 * This class provides the interface to make the win lose scene.
 */
class WinLoseScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /** basic state  */
        IDLE,
        /** back to lobby */
        ABORT,
        /** forward to pre gamescene*/
        PRE_GAMESCENE_START
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The scene node defined by the JSON */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The network controller shared across all scenes */
    std::shared_ptr<NetworkController> _network;

    /** Button that returns the player to the main lobby */
    std::shared_ptr<cugl::scene2::Button> _returnButton;

    /** Overlay image displayed on victory */
    std::shared_ptr<cugl::scene2::SceneNode> _victoryImage;

    /** Overlay image displayed on defeat */
    std::shared_ptr<cugl::scene2::SceneNode> _defeatImage;

    /** Button shown in phase 1 that advances to the stats screen */
    std::shared_ptr<cugl::scene2::Button> _continueButton;

    /** Root node for the team-wide stat panel */
    std::shared_ptr<cugl::scene2::SceneNode> _teamStats;
    
    /** Root node for the team-wide stat panel */
    std::shared_ptr<cugl::scene2::PolygonNode> _teamStatsBG;

    /** Header banner displayed above the stats panels */
    std::shared_ptr<cugl::scene2::SceneNode> _statsHeader;

    /** Root node containing the four individual player stat rows */
    std::shared_ptr<cugl::scene2::SceneNode> _indivStats;

    /** Win/lose label whose texture swaps based on match outcome */
    std::shared_ptr<cugl::scene2::PolygonNode> _successLabel;

    /** Label showing the team's total damage dealt */
    std::shared_ptr<cugl::scene2::Label> _teamTotalDmg;

    /** Label showing the team's total healing done */
    std::shared_ptr<cugl::scene2::Label> _teamTotalHeal;

    /** Utility rating stars; each node has an empty/fill child toggled by star count */
    std::shared_ptr<cugl::scene2::SceneNode> _teamUtilStar[3];

    /** Per-player damage labels, indexed 0–3 */
    std::shared_ptr<cugl::scene2::Label> _summaryTableDmg[4];

    /** Per-player healing labels, indexed 0–3 */
    std::shared_ptr<cugl::scene2::Label> _summaryTableHeal[4];

    /** Per-player utility labels, indexed 0–3 */
    std::shared_ptr<cugl::scene2::SceneNode> _summaryTableUtil[4][3];

    /** Per-player display name labels, indexed 0–3 */
    std::shared_ptr<cugl::scene2::Label> _summaryTableNames[4];
    
    /** Per-player display name labels, indexed 0–3 */
    std::shared_ptr<cugl::scene2::PolygonNode> _summaryTableIcons[4];

    /** End-of-match stats for a single player */
    struct PlayerStats {
        /** House id of the player in the display  */
        std::string houseId;
        /** Display name shown in the stats table, e.g. "ATHENA | help_me123" */
        std::string displayName;
        /** Total damage dealt by this player */
        int damage;
        /** Total healing done by this player */
        int heals;
        /** Star rating from 0 (no stars) to 3 (all stars) */
        int utility;
    };

    /** Timeline that drives all phase animations */
    std::shared_ptr<cugl::ActionTimeline> _timeline;

    /** True while an animation sequence is actively running */
    bool _animating = false;

    /** Which UI phase is currently displayed (1 = result reveal, 2 = stats) */
    int _phase = 1;

    /** True when the continue button was pressed and phase 2 should begin next update */
    bool _pendingPhase2 = false;

    /** The current status of this scene, used to signal transitions to the app */
    Status _status;

    /** True if the local player's team won the match */
    bool _didWin;

    /** True while waiting for the phase 1 fade-out to complete before revealing phase 2 */
    bool _transitioningToPhase2 = false;

    /** Countdown in seconds until the phase 2 content is revealed after the fade-out */
    float _phase2Timer = 0.0f;

    /** Resting positions of each player row, saved before they are pushed off-screen */
    cugl::Vec2 _rowOriginalPos[4];
    
    /** Snapshot of player stats captured at game end, populated by captureStats(). */
    PlayerStats _capturedStats[4];
    
    /** Slot index of the MVP player after reordering, always 0 after captureStats(). */
    int _mvpOriginalSlot = -1;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new win/lose scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    WinLoseScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~WinLoseScene() { dispose(); }
    
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
     * Retrieves and stores references to the WinLoseScene UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * images and the return button. When stats are added more background UI
     * will be added.
     */
    void setupUI();
    
    void setupStatsUI();
    
    /**
     * Attaches input listeners to the return button.
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
     * Any value other than IDLE will transition to a new scene.
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
    
    /**
     * Sets the whether we won or lost.
     */
    void setDidWin(bool value) { _didWin = value; }
    
    /**
     * Populates all phase 2 UI labels and star ratings from a PlayerStats array.
     *
     * For each player row (0–3):
     *   - Sets the name label to players[i].displayName.
     *   - Sets the DMG label to players[i].damage, formatted with commas.
     *   - Sets the HEAL label to players[i].heals, formatted with commas.
     *   - Sets the UTL label to players[i].utility, formatted with commas.
     *
     * For team statistics:
     *   - Sets the Total Damage label to the sum of all players' damage.
     *   - Sets the Total Heals label to the sum of all players' heals.
     *   - Toggles the fill child of each _teamUtilStar node based on the
     *     star count returned by _network->computeTeamUtilityStars().
     *
     * @param players  Array of exactly 4 PlayerStats structs, one per player slot
     *                 in circle order (slot 0 = index 0, etc.). The caller is
     *                 responsible for ensuring all four entries are populated.
     */
    void setStats(const PlayerStats players[4]);
    
    /**
     * Captures the current stats from the NetworkController into a local
     * snapshot. Must be called immediately when the game ends, before any
     * network state is cleared or lobby updates arrive.
     * setActive(true) will then display from this snapshot.
     */
    void captureStats();

    /**
     * Computes which slot in the captured stats array has the highest combined
     * score (damage + heals + weighted utility), moves that entry to index 0,
     * and shifts everyone else down by one. Sets _mvpOriginalSlot to the
     * original slot index of the MVP before reordering.
     * Must be called after all four _capturedStats entries are populated and
     * before setStats() is called.
     */
    void reorderForMVP();
    
private:
    
    /**
     * Toggles visibility and activation state for the two UI phases.
     *
     * Phase 1 — result reveal:
     *   Visible:  win/lose image + continue button.
     *   Hidden:   stats panels + return button.
     *
     * Phase 2 — post-game stats:
     *   Visible:  stats panels + return button.
     *   Hidden:   win/lose image + continue button.
     *
     * @param phase  1 for the result-reveal screen, 2 for the stats screen.
     */
    void showPhase(int phase);
    
    /**
     * Formats an integer as "20,780" style
     *
     * @param value   The number to be formatted
     */
    std::string formatNumber(int value);
    
    /**
     * Formats the house name and player username in the form HOUSE | username
     *
     * @param house   The player's house to be formatted
     * @param username   The player's username to be formatted
     */
    std::string formatPlayerLabel(std::string house, std::string username);
    
    /**
     * Resets all nodes to their initial hidden state for phase 1.
     * The win/lose overlay fades in; everything else starts invisible.
     */
    void resetPhase1Visuals();
    
    /**
     * Animates phase 1: win/lose overlay fades and scales in, then the
     * continue button fades in.
     */
    void runPhase1Intro();

    /**
     * Fades out phase 1 nodes, then starts the timer that triggers the
     * phase 2 reveal once the fade completes.
     */
    void transitionToPhase2();

    /**
     * Resets all phase 2 nodes to their pre-animation state:
     *   - Backgrounds and labels start at alpha 0.
     *   - Each player row is pushed fully off-screen to the left.
     *   - Original row positions are saved so the reset is safe on re-entry.
     */
    void resetPhase2Visuals();

    /**
     * Runs the phase 2 intro sequence using chained completion listeners:
     *
     *   Beat 1 (immediate):  successLabel + teamStats background fade in (0.40s)
     *   Beat 2 (on beat 1):  teamStats content labels/stars fade in     (0.40s)
     *   Beat 3 (on beat 2):  player rows slide in one at a time         (0.35-0.45s)
     *   Beat 4 (after rows): return button fades in                     (0.25s)
     */
    void runPhase2Intro();
    
    /**
     * Slides player row i in from off-screen left, then chains to row i+1.
     * After all 4 rows have slid in the return button fades in.
     *
     * @param i  Index of the row to animate (0-3).
     */
    void slideInRow(int i);
    
    /**
     * Fades in utility star i, then chains to star i+1.
     * After all 3 stars have faded in, starts the row slide sequence.
     *
     * @param i  Index of the star to animate (0-2).
     */
    void fadeInStar(int i);

};

#endif /* __WIN_LOSE_SCENE_H__ */


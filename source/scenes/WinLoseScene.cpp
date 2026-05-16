#include "WinLoseScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852
/** The width of the individual banner */
#define STATS_BANNER_WIDTH  484

#pragma mark -
#pragma mark Provided Methods

/**
 * Initializes the boss selection scene.
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
bool WinLoseScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
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
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("winLoseScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    _timeline = ActionTimeline::alloc();

    setupUI();
    setupListeners();
    
    _status = Status::IDLE;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Retrieves and stores references to the WinLoseScene UI elements.
 *
 * This method looks up UI components from the scene graph including the
 * images and the return button.
 */
void WinLoseScene::setupUI() {
    _continueButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("winLoseScene.continue"));
    
    _victoryImage = _assets->get<scene2::SceneNode>("winLoseScene.winner");
    
    _defeatImage = _assets->get<scene2::SceneNode>("winLoseScene.loser");
    
    _returnButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("winLoseScene.return"));
    
    _teamStats    = _assets->get<scene2::SceneNode>("winLoseScene.teamStats");
    _statsHeader  = _assets->get<scene2::SceneNode>("winLoseScene.statsHeader");
    _indivStats   = _assets->get<scene2::SceneNode>("winLoseScene.indivStats");
    _successLabel = std::dynamic_pointer_cast<scene2::PolygonNode>(
        _assets->get<scene2::SceneNode>("winLoseScene.winLoseLabelS"));
    
    setupStatsUI();
}

void WinLoseScene::setupStatsUI() {
    // Team stat number labels
    _teamTotalDmg = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("winLoseScene.teamStats.stats.totalDmg.value"));
    _teamTotalHeal = std::dynamic_pointer_cast<scene2::Label>(
        _assets->get<scene2::SceneNode>("winLoseScene.teamStats.stats.totalHeal.value"));

    // Utility rating stars
    for (int i = 0; i < 3; i++) {
        std::string path = "winLoseScene.teamStats.stats.utilRating.stars.star" + std::to_string(i);
        _teamUtilStar[i] = _assets->get<scene2::SceneNode>(path);
    }

    // Individual player rows
    std::string rowKeys[4] = {"1", "2", "3", "4"};
    for (int i = 0; i < 4; i++) {
        std::string base = "winLoseScene.indivStats." + rowKeys[i];
        _summaryTableNames[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".name"));
        _summaryTableDmg[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".values.damage"));
        _summaryTableHeal[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".values.heal"));

        for (int j = 0; j < 3; j++) {
            std::string starPath = base + ".values.utility.stars.star" + std::to_string(j);
            _summaryTableUtil[i][j] = _assets->get<scene2::SceneNode>(starPath);
        }
    }
}

/**
 * Attaches input listeners to the buttons.
 */
void WinLoseScene::setupListeners() {
    _continueButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _pendingPhase2 = true;
        }
    });
    
    _returnButton->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void WinLoseScene::dispose() {
    if (_active) {
        _returnButton  = nullptr;
        _continueButton = nullptr;
        _victoryImage  = nullptr;
        _defeatImage   = nullptr;
        _teamStats     = nullptr;
        _statsHeader   = nullptr;
        _indivStats    = nullptr;
        _successLabel  = nullptr;
        _active = false;
    }
    _network = nullptr;
}

/**
 * Sets whether the scene is currently active.
 *
 * Buttons are activated when the scene is made active and deactivated
 * when it is not.
 *
 * @param value whether the scene is currently active
 */
void WinLoseScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _pendingPhase2        = false;
            _transitioningToPhase2 = false;
            _status = IDLE;
            showPhase(1);
        } else {
            _returnButton->deactivate();
            _returnButton->setDown(false);
            _continueButton->deactivate();
            _continueButton->setDown(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void WinLoseScene::update(float timestep) {
    _timeline->update(timestep);

    if (_pendingPhase2) {
        _pendingPhase2 = false;
        showPhase(2);
    }

    // Delayed phase 2 content reveal — waits for phase 1 fade-out to finish
    if (_transitioningToPhase2) {
        _phase2Timer -= timestep;
        if (_phase2Timer <= 0.0f) {
            _transitioningToPhase2 = false;

            // Hide phase 1 nodes
            _victoryImage->setVisible(false);
            _defeatImage->setVisible(false);
            _continueButton->setVisible(false);

            // Show phase 2 nodes (alpha handled by resetPhase2Visuals)
            _teamStats->setVisible(true);
            _statsHeader->setVisible(true);
            _indivStats->setVisible(true);
            _successLabel->setVisible(true);
            _returnButton->setVisible(true);
            _returnButton->activate();

            resetPhase2Visuals();
            runPhase2Intro();
        }
    }

    _network->getNetworkUpdates();
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
    }
}

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
void WinLoseScene::showPhase(int phase) {
    _phase = phase;

    if (phase == 1) {
        resetPhase1Visuals();
        _continueButton->activate();
        _returnButton->deactivate();
        runPhase1Intro();
    } else {
        // Fade out phase 1; _transitioningToPhase2 timer triggers the swap
        transitionToPhase2();
    }
}

/**
 * Sets the player stats displayed on the phase 2 screen.
 *
 * @param players   The stats of every player as defined by the struct
 */
void WinLoseScene::setStats(const PlayerStats players[4]) {
    int totalDamage  = 0;
    int totalHeals   = 0;
    int totalUtility = 0;

    for (int i = 0; i < 4; i++) {
        totalDamage  += players[i].damage;
        totalHeals   += players[i].heals;
        totalUtility += players[i].utility;

        _summaryTableNames[i]->setText(players[i].displayName);
        _summaryTableDmg[i]->setText(formatNumber(players[i].damage));
        _summaryTableHeal[i]->setText(formatNumber(players[i].heals));
        for (int j = 0; j < 3; j++) {
            _summaryTableUtil[i][j]->getChildByName("fill")->setVisible(j < players[i].utility);
        }
    }

    _teamTotalDmg->setText(formatNumber(totalDamage));
    _teamTotalHeal->setText(formatNumber(totalHeals));

    // Utility stars (hardcoded to 3 for now)
    int stars = 3;
    for (int i = 0; i < 3; i++) {
        _teamUtilStar[i]->getChildByName("fill")->setVisible(i < stars);
    }
}

/**
 * Formats an integer as "20,780" style
 *
 * @param value   The number to be formatted
 */
std::string WinLoseScene::formatNumber(int value) {
    std::string s = std::to_string(value);
    int insertPos = (int)s.size() - 3;
    while (insertPos > 0) {
        s.insert(insertPos, ",");
        insertPos -= 3;
    }
    return s;
}

#pragma mark -
#pragma mark Phase 1 Animation

/**
 * Resets all nodes to their initial hidden state for phase 1.
 * The win/lose overlay fades in; everything else starts invisible.
 */
void WinLoseScene::resetPhase1Visuals() {
    auto overlay = _didWin ? _victoryImage : _defeatImage;
    auto other = _didWin ? _defeatImage  : _victoryImage;

    other->setVisible(false);

    overlay->setVisible(true);
    overlay->setColor(Color4(255, 255, 255, 0));

    _continueButton->setVisible(true);
    _continueButton->setColor(Color4(255, 255, 255, 0));

    // Phase 2 nodes fully hidden
    _teamStats->setVisible(false);
    _statsHeader->setVisible(false);
    _indivStats->setVisible(false);
    _successLabel->setVisible(false);
    _returnButton->setVisible(false);
}

/**
 * Animates phase 1: win/lose overlay fades and scales in, then the
 * continue button fades in.
 */
void WinLoseScene::runPhase1Intro() {
    auto overlay = _didWin ? _victoryImage : _defeatImage;

    // Overlay fades + scales in simultaneously
    _timeline->add("overlayFade",
                   cugl::scene2::FadeTo::alloc(1.0f)->attach(overlay), 0.35f);
    _timeline->add("overlayScaleUp",
                   cugl::scene2::ScaleTo::alloc(1.05f)->attach(overlay), 0.30f);

    // Continue button fades in after overlay finishes
    _timeline->addCompletionListener("overlayFade",
        [this](const std::string& key, float time, float actual) {
            _timeline->add("continueFade",
                           cugl::scene2::FadeTo::alloc(1.0f)->attach(_continueButton), 0.40f);
        });
}

/**
 * Fades out phase 1 nodes, then starts the timer that triggers the
 * phase 2 reveal once the fade completes.
 */
void WinLoseScene::transitionToPhase2() {
    auto overlay = _didWin ? _victoryImage : _defeatImage;

    _timeline->add("overlayOut",
                   cugl::scene2::FadeTo::alloc(0.0f)->attach(overlay), 0.25f);
    _timeline->add("continueOut",
                   cugl::scene2::FadeTo::alloc(0.0f)->attach(_continueButton), 0.25f);

    _transitioningToPhase2 = true;
    _phase2Timer = 0.28f;   // slightly longer than the 0.25s fade
}

#pragma mark -
#pragma mark Phase 2 Animation

/**
 * Resets all phase 2 nodes to their pre-animation state:
 *   - Backgrounds and labels start at alpha 0.
 *   - Each player row is pushed fully off-screen to the left.
 *   - Original row positions are saved so the reset is safe on re-entry.
 */
void WinLoseScene::resetPhase2Visuals() {
    auto successPolygon = std::dynamic_pointer_cast<scene2::PolygonNode>(_successLabel);
    if (successPolygon) {
        std::string texKey = _didWin ? "successLabelS" : "defeatLabelS";
        successPolygon->setTexture(_assets->get<cugl::graphics::Texture>(texKey));
        successPolygon->setScale(0.5f);
    }
    
    // Backgrounds / header / label all start invisible
    _teamStats->setColor(Color4(255, 255, 255, 0));
    _statsHeader->setColor(Color4(255, 255, 255, 0));
    _successLabel->setColor(Color4(255, 255, 255, 0));
    _returnButton->setColor(Color4(255, 255, 255, 0));

    // teamStats content labels start invisible
    _teamTotalDmg->setColor(Color4(255, 255, 255, 0));
    _teamTotalHeal->setColor(Color4(255, 255, 255, 0));
    for (int i = 0; i < 3; i++) {
        _teamUtilStar[i]->setColor(Color4(255, 255, 255, 0));
    }

    // indivStats parent is fully visible — rows control their own position
    _indivStats->setColor(Color4(255, 255, 255, 255));

    for (int i = 0; i < 4; i++) {
        auto row = _indivStats->getChildByName(std::to_string(i + 1));
        _rowOriginalPos[i] = row->getPosition();
        row->setPosition(Vec2(-STATS_BANNER_WIDTH, _rowOriginalPos[i].y));
        row->setColor(Color4(255, 255, 255, 255)); // fully opaque; position is the reveal
    }
}

/**
 * Runs the phase 2 intro sequence using chained completion listeners:
 *
 *   Beat 1 (immediate):  successLabel + teamStats background fade in (0.40s)
 *   Beat 2 (on beat 1):  teamStats content labels/stars fade in     (0.40s)
 *   Beat 3 (on beat 2):  player rows slide in one at a time         (0.35-0.45s)
 *   Beat 4 (after rows): return button fades in                     (0.25s)
 */
void WinLoseScene::runPhase2Intro() {
    // ── Beat 1: backgrounds fade in ──────────────────────────────────────────
    _timeline->add("labelFade",
                   cugl::scene2::FadeTo::alloc(1.0f)->attach(_successLabel), 0.40f);
    _timeline->add("headerFade",
                   cugl::scene2::FadeTo::alloc(1.0f)->attach(_statsHeader), 0.40f);
    _timeline->add("teamStatsFade",
                   cugl::scene2::FadeTo::alloc(1.0f)->attach(_teamStats), 0.40f);

    // ── Beat 2: teamStats content fades in once background is visible ────────
    _timeline->addCompletionListener("teamStatsFade",
        [this](const std::string& key, float time, float actual) {
            _timeline->add("dmgFade",
                           cugl::scene2::FadeTo::alloc(1.0f)->attach(_teamTotalDmg), 0.45f);
            _timeline->add("healFade",
                           cugl::scene2::FadeTo::alloc(1.0f)->attach(_teamTotalHeal), 0.45f);
        
            // ── Beat 3: rows slide in sequentially once content is visible ───
            _timeline->addCompletionListener("dmgFade",
                [this](const std::string& key, float time, float actual) {
                    fadeInStar(0);
                });
        });
}

/**
 * Slides player row i in from off-screen left, then chains to row i+1.
 * After all 4 rows have slid in the return button fades in.
 *
 * @param i  Index of the row to animate (0-3).
 */
void WinLoseScene::slideInRow(int i) {
    if (i >= 4) {
        // All rows done — show the return button
        _timeline->add("returnFade",
                       cugl::scene2::FadeTo::alloc(1.0f)->attach(_returnButton), 0.25f);
        return;
    }

    auto row = _indivStats->getChildByName(std::to_string(i + 1));
    std::string moveKey = "rowMove" + std::to_string(i);

    // Slide right by screenWidth to land on the saved original position
    _timeline->add(moveKey,
                   cugl::scene2::MoveBy::alloc(Vec2(STATS_BANNER_WIDTH, 0))->attach(row), 0.50f);

    _timeline->addCompletionListener(moveKey,
        [this, i](const std::string& key, float time, float actual) {
            slideInRow(i + 1);
        });
}

/**
 * Fades in utility star i, then chains to star i+1.
 * After all 3 stars have faded in, starts the row slide sequence.
 *
 * @param i  Index of the star to animate (0-2).
 */
void WinLoseScene::fadeInStar(int i) {
    if (i >= 3) {
        // All stars done — start sliding in player rows
        slideInRow(0);
        return;
    }
 
    std::string starKey = "starFade" + std::to_string(i);
 
    _timeline->add(starKey,
                   cugl::scene2::FadeTo::alloc(1.0f)->attach(_teamUtilStar[i]), 0.40f);
 
    _timeline->addCompletionListener(starKey,
        [this, i](const std::string& key, float time, float actual) {
            fadeInStar(i + 1);
        });
}

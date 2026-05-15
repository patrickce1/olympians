#include "WinLoseScene.h"

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
 * images and the return button. When stats are added more background UI
 * will be added.
 */
void WinLoseScene::setupUI() {
    _continueButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("winLoseScene.continue"));
    
    _victoryImage = _assets->get<scene2::SceneNode>("winLoseScene.winner");
    
    _defeatImage = _assets->get<scene2::SceneNode>("winLoseScene.loser");
    
    _returnButton = std::dynamic_pointer_cast<scene2::Button>(
        _assets->get<scene2::SceneNode>("winLoseScene.return"));
    
    _teamStats     = _assets->get<scene2::SceneNode>("winLoseScene.teamStats");
    _statsHeader   = _assets->get<scene2::SceneNode>("winLoseScene.statsHeader");
    _indivStats    = _assets->get<scene2::SceneNode>("winLoseScene.indivStats");
    _successLabel  = _assets->get<scene2::SceneNode>("winLoseScene.winLoseLabelS");
    
    showPhase(1);
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
        _utilStar[i] = _assets->get<scene2::SceneNode>(path);
    }

    // Individual player rows
    std::string rowKeys[4] = {"1", "2", "3", "4"};
    for (int i = 0; i < 4; i++) {
        std::string base = "winLoseScene.indivStats." + rowKeys[i];
        _playerName[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".name"));
        _playerDmg[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".values.damage"));
        _playerHeal[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".values.heal"));
        _playerUtility[i] = std::dynamic_pointer_cast<scene2::Label>(
            _assets->get<scene2::SceneNode>(base + ".values.utility"));
    }
}

/**
 * Attaches input listeners to the return button.
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
        _returnButton = nullptr;
        _continueButton  = nullptr;
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
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.  Buttons
 * should be activated when it is made active and deactivated when
 * it is not.
 *
 * @param value whether the scene is currently active
 */
void WinLoseScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _pendingPhase2 = false;
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
    if (_pendingPhase2) {
        _pendingPhase2 = false;
        showPhase(2);
    }
    
    _network->getNetworkUpdates();
    
    // Forward to pre game scene if host started while we were here
    if (_network->getHostsCurrentScene() == 0) {
        _status = Status::PRE_GAMESCENE_START;
        return;
    }
}

/**
 * Toggles visibility and activation state for the two UI phases.
 *
 * Phase 1 — result reveal:
 *   Visible:  win/lose sign + return (continue) button.
 *   Hidden:   stats panels + medium (lobby) button.
 *
 * Phase 2 — post-game stats:
 *   Visible:  stats panels + medium (lobby) button.
 *   Hidden:   win/lose sign + return (continue) button.
 *
 * @param phase  1 for the result-reveal screen, 2 for the stats screen.
 */
void WinLoseScene::showPhase(int phase) {
    bool onPhase1 = (phase == 1);
 
    if (onPhase1) {
        _victoryImage->setVisible(_didWin);
        _defeatImage->setVisible(!_didWin);
    } else {
        _victoryImage->setVisible(false);
        _defeatImage->setVisible(false);
    }
 
    // active only in phase 1
    _continueButton->setVisible(onPhase1);
    if (onPhase1) {
        _continueButton->activate();
    } else {
        _continueButton->deactivate();
        _continueButton->setDown(false);
    }
 
    // active only in phase 2
    _teamStats->setVisible(!onPhase1);
    _statsHeader->setVisible(!onPhase1);
    _indivStats->setVisible(!onPhase1);
    _successLabel->setVisible(!onPhase1);
 
    _returnButton->setVisible(!onPhase1);
    if (!onPhase1) {
        _returnButton->activate();
    } else {
        _returnButton->deactivate();
        _returnButton->setDown(false);
    }
 
    _phase = phase;
}

void WinLoseScene::setStats(const PlayerStats players[4]) {
    int totalDamage = 0;
    int totalHeals  = 0;
    int totalUtility = 0;

    for (int i = 0; i < 4; i++) {
        totalDamage  += players[i].damage;
        totalHeals   += players[i].heals;
        totalUtility += players[i].utility;

        _playerName[i]->setText(players[i].displayName);
        _playerDmg[i]->setText(formatNumber(players[i].damage));
        _playerHeal[i]->setText(formatNumber(players[i].heals));
        _playerUtility[i]->setText(formatNumber(players[i].utility));
    }

    _teamTotalDmg->setText(formatNumber(totalDamage));
    _teamTotalHeal->setText(formatNumber(totalHeals));

    // Utility stars
    int stars = 3;
    for (int i = 0; i < 3; i++) {
        _utilStar[i]->getChildByName("fill")->setVisible(i < stars);
    }
}

// Helper to format integers as "20,780" style
std::string WinLoseScene::formatNumber(int value) {
    std::string s = std::to_string(value);
    int insertPos = (int)s.size() - 3;
    while (insertPos > 0) {
        s.insert(insertPos, ",");
        insertPos -= 3;
    }
    return s;
}


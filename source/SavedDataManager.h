#ifndef __SAVE_DATA_MANAGER_H__
#define __SAVE_DATA_MANAGER_H__

#include <cugl/cugl.h>
#include <string>

/**
 * Singleton that manages persistent local player data stored in
 * savedData.json inside the application's writable directory.
 *
 * Persisted fields:
 *   - playerName    : the player's display name
 *   - sfxVolume     : SFX/audio volume multiplier in [0, 1]
 *   - musicVolume   : music volume multiplier in [0, 1]
 *   - effectsEnabled: whether screen effects are on
 *   - hapticsEnabled: whether haptic feedback is on
 *
 * Call load() once at app startup before any scene is activated.
 * Call save() after mutating any field to persist changes to disk.
 *
 * Usage:
 *   SavedDataManager::get().load();
 *   SavedDataManager::get().getPlayerName();
 *   SavedDataManager::get().setPlayerName("Atlas");
 *   SavedDataManager::get().setSFXVolume(0.8f);
 *   SavedDataManager::get().save();
 */
class SavedDataManager {
public:

#pragma mark - Single Access

    /**
     * Returns the single shared instance of SavedDataManager.
     *
     * The instance is constructed on first call and lives for the duration
     * of the application.
     *
     * @return the global SavedDataManager instance.
     */
    static SavedDataManager& get() {
        static SavedDataManager instance;
        return instance;
    }

    // Non-copyable / non-movable
    SavedDataManager(const SavedDataManager&)            = delete;
    SavedDataManager& operator=(const SavedDataManager&) = delete;

#pragma mark - Lifecycle

    /**
     * Loads save data from disk.
     *
     * On first launch, copies the default savedData.json from the asset
     * bundle into the writable save directory, then reads from that copy.
     * On subsequent launches it reads the writable copy directly.
     *
     * Recognised JSON keys and their corresponding fields:
     *   - "playerName"     → _playerName
     *   - "sfxVolume"      → _sfxVolume
     *   - "musicVolume"    → _musicVolume
     *   - "effectsEnabled" → _effectsEnabled
     *   - "hapticsEnabled" → _hapticsEnabled
     *
     * Missing keys are silently skipped; the corresponding field retains
     * its default value. Safe to call more than once — each call re-reads
     * the file from disk.
     *
     * @return true if the file was found and parsed without error;
     *         false on missing file or parse failure (defaults are used
     *         in both cases).
     */
    bool load();

    /**
     * Writes current save data to the writable copy of savedData.json.
     *
     * Serialises all member fields as a JSON object and writes it to
     * savedData.json in the platform writable directory, overwriting any
     * prior contents. Never touches the read-only asset bundle.
     *
     * Written keys:
     *   - "playerName"
     *   - "sfxVolume"
     *   - "musicVolume"
     *   - "effectsEnabled"
     *   - "hapticsEnabled"
     *
     * Should be called after any setter whose change should survive
     * across sessions.
     *
     * @return true if the file was written successfully; false on any
     *         filesystem error.
     */
    bool save();

#pragma mark - Accessors

    // ── Player name ────────────────────────────────────────────────────────

    /**
     * Returns the saved player name.
     *
     * Returns an empty string if no name has been set or load() has not
     * yet been called.
     *
     * @return the saved player name, or "" if none exists.
     */
    const std::string& getPlayerName() const { return _playerName; }

    /**
     * Sets the player name in memory.
     *
     * Does not write to disk. Call save() afterwards to persist the
     * change across sessions.
     *
     * @param name  The player name to store.
     */
    void setPlayerName(const std::string& name) { _playerName = name; }

    /**
     * Returns true if a non-empty player name has been set.
     *
     * Used by MenuScene to decide whether to show the first-launch
     * onboarding flow or skip directly to HostSetupScene.
     *
     * @return true if a player name is saved; false otherwise.
     */
    bool hasPlayerName() const { return !_playerName.empty(); }

    // ── SFX volume ─────────────────────────────────────────────────────────

    /**
     * Returns the saved SFX/audio volume multiplier.
     *
     * Value is in [0, 1]. Defaults to 1.0 if load() has not yet been
     * called or the key was absent from the save file.
     *
     * @return the SFX volume multiplier.
     */
    float getSFXVolume() const { return _sfxVolume; }

    /**
     * Sets the SFX/audio volume multiplier in memory.
     *
     * Does not write to disk. Call save() afterwards to persist the
     * change across sessions. Value should be in [0, 1].
     *
     * @param value  The new SFX volume multiplier.
     */
    void setSFXVolume(float value) { _sfxVolume = value; }

    // ── Music volume ───────────────────────────────────────────────────────

    /**
     * Returns the saved music volume multiplier.
     *
     * Value is in [0, 1]. Defaults to 1.0 if load() has not yet been
     * called or the key was absent from the save file.
     *
     * @return the music volume multiplier.
     */
    float getMusicVolume() const { return _musicVolume; }

    /**
     * Sets the music volume multiplier in memory.
     *
     * Does not write to disk. Call save() afterwards to persist the
     * change across sessions. Value should be in [0, 1].
     *
     * @param value  The new music volume multiplier.
     */
    void setMusicVolume(float value) { _musicVolume = value; }

    // ── Screen effects ─────────────────────────────────────────────────────

    /**
     * Returns true if screen effects are enabled.
     *
     * Defaults to true if load() has not yet been called or the key was
     * absent from the save file.
     *
     * @return true if effects are on; false if they are disabled.
     */
    bool getEffectsEnabled() const { return _effectsEnabled; }

    /**
     * Sets whether screen effects are enabled in memory.
     *
     * Does not write to disk. Call save() afterwards to persist the
     * change across sessions.
     *
     * @param value  true to enable effects; false to disable.
     */
    void setEffectsEnabled(bool value) { _effectsEnabled = value; }

    // ── Haptics ────────────────────────────────────────────────────────────

    /**
     * Returns true if haptic feedback is enabled.
     *
     * Defaults to true if load() has not yet been called or the key was
     * absent from the save file.
     *
     * @return true if haptics are on; false if they are disabled.
     */
    bool getHapticsEnabled() const { return _hapticsEnabled; }

    /**
     * Sets whether haptic feedback is enabled in memory.
     *
     * Does not write to disk. Call save() afterwards to persist the
     * change across sessions.
     *
     * @param value  true to enable haptics; false to disable.
     */
    void setHapticsEnabled(bool value) { _hapticsEnabled = value; }
    
    // ── Tutorial ───────────────────────────────────────────────────────────

    /**
     * Returns true if the player has completed the tutorial at least once.
     *
     * When false, only the Circe boss is selectable in HouseSelectScene.
     * Defaults to false if load() has not yet been called or the key was
     * absent from the save file (i.e. new installs start with the tutorial
     * required).
     *
     * @return true if the tutorial has been completed; false otherwise.
     */
    bool getTutorialCompleted() const { return _tutorialCompleted; }

    /**
     * Sets whether the tutorial has been completed in memory.
     *
     * Does not write to disk. Call save() afterwards to persist the
     * change across sessions.
     *
     * @param value  true to mark the tutorial as completed; false to reset it.
     */
    void setTutorialCompleted(bool value) { _tutorialCompleted = value; }

private:
    SavedDataManager() = default;

    /**
     * Returns the absolute path to savedData.json inside the writable
     * application directory. This is the live copy that gets read and
     * written at runtime.
     *
     * @return the full writable file path.
     */
    std::string getSavePath() const;

    /**
     * Returns the path to the savedData.json template shipped inside the
     * app's read-only asset bundle. Used only for the first-run copy.
     *
     * @return the full asset path to the default save file.
     */
    std::string getAssetPath() const;

    /** The saved player display name. Empty string if not yet set. */
    std::string _playerName;

    /** SFX/audio volume multiplier in [0, 1]. Default 1.0. */
    float _sfxVolume = 1.0f;

    /** Music volume multiplier in [0, 1]. Default 1.0. */
    float _musicVolume = 1.0f;

    /** Whether screen effects are enabled. Default true. */
    bool _effectsEnabled = true;

    /** Whether haptic feedback is enabled. Default true. */
    bool _hapticsEnabled = true;
    
    /**
     * Whether the player has completed the tutorial. Default false so that
     * new installs always start with the tutorial required and only Circe
     * unlocked until it is finished.
     */
    bool _tutorialCompleted = false;
};

#endif /* __SAVE_DATA_MANAGER_H__ */

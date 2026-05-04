#ifndef __SAVE_DATA_MANAGER_H__
#define __SAVE_DATA_MANAGER_H__

#include <cugl/cugl.h>
#include <string>

/**
 * Singleton that manages persistent local player data stored in
 * save/savedata.json inside the application's writable directory.
 *
 * Call load() once at app startup before any scene is activated.
 * Call save() after mutating any field to persist changes to disk.
 *
 * Usage:
 *   SavedDataManager::get().load();
 *   SavedDataManager::get().getPlayerName();
 *   SavedDataManager::get().setPlayerName("Atlas");
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
     * Attempts to open and parse save/savedata.json from the application's
     * writable directory. If the file is absent (first launch) or malformed,
     * all fields retain their default values and false is returned. On
     * success each recognised JSON key is applied to the corresponding
     * member field. Safe to call more than once — each call re-reads the
     * file from disk.
     *
     * @return true if the file was found and parsed without error;
     *         false on missing file or parse failure (defaults are used
     *         in both cases).
     */
    bool load();

    /**
     * Writes current save data to disk.
     *
     * Creates the save/ subdirectory under the platform save directory if
     * it does not already exist, then serialises all member fields as a
     * JSON object and writes it to save/savedata.json, overwriting any
     * prior contents. Should be called after any setter call that should
     * be persisted across sessions.
     *
     * @return true if the directory was accessible and the file was written
     *         successfully; false on any filesystem error.
     */
    bool save();

#pragma mark - Accessors

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
     * This does not write to disk. Call save() afterwards to persist the
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

    /** The saved player name. Empty string if not yet set. */
    std::string _playerName;
};

#endif /* __SAVE_DATA_MANAGER_H__ */

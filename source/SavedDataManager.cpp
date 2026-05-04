#include "SavedDataManager.h"

using namespace cugl;

#define SAVE_FILE       "savedData.json"
#define SAVE_ASSET_PATH "json/savedData.json"   // path inside your assets folder

// ---------------------------------------------------------------------------
#pragma mark - Helpers
// ---------------------------------------------------------------------------

/**
 * Returns the absolute path to savedata.json inside the writable
 * application directory. This is the live copy that gets read and
 * written at runtime.
 *
 * @return the full writable file path.
 */
std::string SavedDataManager::getSavePath() const {
    return Application::get()->getSaveDirectory() + SAVE_FILE;
}

/**
 * Returns the path to the savedata.json template shipped inside the
 * app's read-only asset bundle. Used only for the first-run copy.
 *
 * @return the full asset path to the default save file.
 */
std::string SavedDataManager::getAssetPath() const {
    return Application::get()->getAssetDirectory() + SAVE_ASSET_PATH;
}

// ---------------------------------------------------------------------------
#pragma mark - Lifecycle
// ---------------------------------------------------------------------------

/**
 * Loads save data from disk.
 *
 * On first launch, copies the default savedata.json from the asset
 * bundle into the writable save directory, then reads from that copy.
 * On subsequent launches it reads the writable copy directly. If
 * anything fails, all fields retain their default values.
 *
 * @return true if data was loaded successfully; false otherwise.
 */
bool SavedDataManager::load() {
    std::string writePath  = getSavePath();
    std::string assetPath  = getAssetPath();

    // First launch — copy the asset template into the writable directory
    if (!cugl::filetool::file_exists(writePath)) {
        CULog("SaveDataManager: no save file found, copying default from assets");
        
        // Read the default file from the asset bundle
        auto reader = JsonReader::alloc(assetPath);
        if (reader == nullptr) {
            CULog("SaveDataManager: missing default asset at %s", assetPath.c_str());
            return false;
        }
        auto root = reader->readJson();
        reader->close();

        if (root == nullptr) {
            CULog("SaveDataManager: failed to parse default asset");
            return false;
        }

        // Write it into the writable directory
        auto writer = JsonWriter::alloc(writePath);
        if (writer == nullptr) {
            CULog("SaveDataManager: failed to write initial save to %s", writePath.c_str());
            return false;
        }
        writer->writeJson(root);
        writer->close();
    }

    // Read from the writable copy
    auto reader = JsonReader::alloc(writePath);
    if (reader == nullptr) {
        CULog("SaveDataManager: failed to open save file at %s", writePath.c_str());
        return false;
    }

    auto root = reader->readJson();
    reader->close();

    if (root == nullptr) {
        CULog("SaveDataManager: failed to parse save file");
        return false;
    }

    if (root->has("playerName")) {
        _playerName = root->getString("playerName", "");
    }

    CULog("SaveDataManager: loaded — playerName='%s'", _playerName.c_str());
    return true;
}

/**
 * Writes current save data to the writable copy of savedata.json.
 *
 * Never touches the asset bundle. Always writes to the platform
 * writable directory returned by getSavePath().
 *
 * @return true if the file was written successfully; false otherwise.
 */
bool SavedDataManager::save() {
    std::string path = getSavePath();

    auto root = JsonValue::allocObject();
    root->appendChild("playerName", JsonValue::alloc(_playerName));

    auto writer = JsonWriter::alloc(path);
    if (writer == nullptr) {
        CULog("SaveDataManager: failed to open %s for writing", path.c_str());
        return false;
    }
    writer->writeJson(root);
    writer->close();

    CULog("SaveDataManager: saved — playerName='%s'", _playerName.c_str());
    return true;
}

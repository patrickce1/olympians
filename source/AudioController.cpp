#include "AudioController.h"

/**
 * Creates a new audio controller (uninitialized).
 */
AudioController::AudioController() : _assets(nullptr) {}

/**
 * Initializes the audio controller with the given asset manager.
 *
 * @param assets The asset manager containing loaded sounds
 * @return true if initialization succeeds; false otherwise.
 */
bool AudioController::init(const std::shared_ptr<AssetManager>& assets) {
    if (assets == nullptr) {
        if (_debug) CULog("AudioController: assets cannot be null");
        return false;
    }
    _assets = assets;
    loadDefaultVolumes();
    return true;
}

/**
 * Disposes of all resources allocated by this controller.
 */
void AudioController::dispose() {
    stopAllSounds();
    _assets = nullptr;
}

/**
 * Starts the audio engine.
 *
 * @param slots The maximum number of simultaneous sound slots (default 16)
 * @return true if the audio engine started successfully
 */
bool AudioController::startAudioEngine(Uint32 slots) {
    bool success = AudioEngine::start(slots);
    if (success) {
        if (_debug) CULog("AudioController: AudioEngine started with %u slots", slots);
    } else {
        if (_debug) CULog("AudioController: Failed to start AudioEngine");
    }
    return success;
}

/**
 * Stops the audio engine, releasing all audio resources.
 */
void AudioController::stopAudioEngine() {
    AudioEngine::stop();
    if (_debug) CULog("AudioController: AudioEngine stopped");
}

/**
 * Plays a sound from the asset manager.
 *
 * @param key The unique identifier for this playback instance
 * @param soundKey The key to retrieve the sound from the asset manager
 * @param loop Whether the sound should loop continuously
 * @param volume The playback volume (0.0 to 1.0)
 * @param force Whether to force playback even if all slots are full
 * @return true if the sound was successfully added to the audio engine
 */
bool AudioController::playSound(const std::string& key, const std::string& soundKey,
                                bool loop, bool force) {
    if (_assets == nullptr) {
        if (_debug) CULog("AudioController::playSound: assets not initialized");
        return false;
    }

    auto engine = AudioEngine::get();
    if (engine == nullptr) {
        if (_debug) CULog("AudioController::playSound: AudioEngine not initialized");
        return false;
    }

    auto sound = _assets->get<Sound>(soundKey);
    if (sound == nullptr) {
        if (_debug) CULog("AudioController::playSound: sound '%s' not found in assets", soundKey.c_str());
        return false;
    }

    bool success = engine->play(key, sound, loop, _sfxVolumeMultiplier, force);
    if (success) {
        if (_debug) CULog("AudioController: Playing sound '%s' with key '%s' (volume=%.2f)", soundKey.c_str(), key.c_str(), _sfxVolumeMultiplier);
    } else {
        if (_debug) CULog("AudioController: Failed to play sound '%s' (no available slots)", soundKey.c_str());
    }
    return success;
}

/**
 * Stops a currently playing sound.
 *
 * @param key The unique identifier for the sound to stop
 * @param fade The fade-out duration in seconds (0 for immediate stop)
 */
void AudioController::stopSound(const std::string& key, float fade) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->clear(key, fade);
        if (_debug) CULog("AudioController: Stopped sound with key '%s'", key.c_str());
    }
}

/**
 * Pauses a currently playing sound.
 *
 * @param key The unique identifier for the sound to pause
 * @param fade The fade-out duration in seconds (0 for immediate pause)
 */
void AudioController::pauseSound(const std::string& key, float fade) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->pause(key, fade);
        if (_debug) CULog("AudioController: Paused sound with key '%s'", key.c_str());
    }
}

/**
 * Resumes a paused sound.
 *
 * @param key The unique identifier for the sound to resume
 */
void AudioController::resumeSound(const std::string& key) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->resume(key);
        if (_debug) CULog("AudioController: Resumed sound with key '%s'", key.c_str());
    }
}

/**
 * Sets the volume of a playing sound.
 *
 * @param key The unique identifier for the sound
 * @param volume The new volume level (0.0 to 1.0)
 */
void AudioController::setVolume(const std::string& key, float volume) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->setVolume(key, volume);
        if (_debug) CULog("AudioController: Set volume for '%s' to %.2f", key.c_str(), volume);
    }
}

/**
 * Gets the current volume of a playing sound.
 *
 * @param key The unique identifier for the sound
 * @return the current volume level, or 0 if not found
 */
float AudioController::getVolume(const std::string& key) const {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        return engine->getVolume(key);
    }
    return 0.0f;
}

/**
 * Stops all currently playing sounds.
 *
 * @param fade The fade-out duration in seconds (0 for immediate stop)
 */
void AudioController::stopAllSounds(float fade) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->clear(fade);
        if (_debug) CULog("AudioController: Stopped all sounds");
    }
}

/**
 * Pauses all currently playing sounds.
 *
 * @param fade The fade-out duration in seconds (0 for immediate pause)
 */
void AudioController::pauseAllSounds(float fade) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->pause(fade);
        if (_debug) CULog("AudioController: Paused all sounds");
    }
}

/**
 * Resumes all paused sounds.
 */
void AudioController::resumeAllSounds() {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->resume();
        if (_debug) CULog("AudioController: Resumed all sounds");
    }
}

/**
 * Plays a music track from the asset manager through the music queue.
 *
 * @param soundKey The key to retrieve the music from the asset manager
 * @param loop Whether the music should loop continuously
 */
void AudioController::playMusic(const std::string& soundKey, bool loop) {
    // Don't restart music if the same track is already playing
    if (_currentMusicKey == soundKey) {
        if (_debug) CULog("AudioController: Music '%s' already playing, skipping restart", soundKey.c_str());
        return;
    }

    if (_assets == nullptr) {
        if (_debug) CULog("AudioController::playMusic: assets not initialized");
        return;
    }

    auto engine = AudioEngine::get();
    if (engine == nullptr) {
        if (_debug) CULog("AudioController::playMusic: AudioEngine not initialized");
        return;
    }

    auto sound = _assets->get<Sound>(soundKey);
    if (sound == nullptr) {
        if (_debug) CULog("AudioController::playMusic: music '%s' not found in assets", soundKey.c_str());
        return;
    }

    auto musicQueue = engine->getMusicQueue();
    if (musicQueue == nullptr) {
        if (_debug) CULog("AudioController::playMusic: failed to get music queue");
        return;
    }
    
    // Queue the sound and set the volume
    musicQueue->play(sound, loop, _musicVolumeMultiplier);
    _currentMusicKey = soundKey;
    if (_debug) CULog("AudioController: Playing music '%s' (loop=%d, volume=%.2f)", soundKey.c_str(), loop, _musicVolumeMultiplier);
}

/**
 * Stops the currently playing music track.
 *
 * @param fade The fade-out duration in seconds (0 for immediate stop)
 */
void AudioController::stopMusic(float fade) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        auto musicQueue = engine->getMusicQueue();
        if (musicQueue != nullptr) {
            musicQueue->clear(fade);
            if (_debug) CULog("AudioController: Stopped music");
        }
    }
}

/**
 * Pauses the currently playing music.
 *
 * @param fade The fade-out duration in seconds (0 for immediate pause)
 */
void AudioController::pauseMusic(float fade) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        auto musicQueue = engine->getMusicQueue();
        if (musicQueue != nullptr) {
            musicQueue->pause(fade);
            if (_debug) CULog("AudioController: Paused music");
        }
    }
}

/**
 * Resumes the paused music.
 */
void AudioController::resumeMusic() {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        auto musicQueue = engine->getMusicQueue();
        if (musicQueue != nullptr) {
            musicQueue->resume();
            if (_debug) CULog("AudioController: Resumed music");
        }
    }
}

/**
 * Sets the volume of the music track.
 *
 * @param volume The music volume (0.0 to 1.0)
 */
void AudioController::setMusicVolume(float volume) {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        auto musicQueue = engine->getMusicQueue();
        if (musicQueue != nullptr) {
            musicQueue->setVolume(volume);
            if (_debug) CULog("AudioController: Set music volume to %.2f", volume);
        }
    }
}

/**
 * Gets the current volume of the music track.
 *
 * @return the current music volume level
 */
float AudioController::getMusicVolume() const {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        auto musicQueue = engine->getMusicQueue();
        if (musicQueue != nullptr) {
            return musicQueue->getVolume();
        }
    }
    return 0.0f;
}

/**
 * Plays a sound effect with an automatically generated unique key.
 *
 * @param soundKey The key to retrieve the sound from the asset manager
 * @param loop Whether the sound should loop continuously
 * @param volume The playback volume (0.0 to 1.0)
 * @return true if the sound was successfully added to the audio engine
 */
bool AudioController::playSoundUnique(const std::string& soundKey, bool loop) {
    std::string uniqueKey = soundKey + "_" + std::to_string(_soundCounter++);
    return playSound(uniqueKey, soundKey, loop, _sfxVolumeMultiplier);
}

/**
 * Sets the music volume multiplier for all music playback.
 * This multiplies with the default volume set in JSON.
 *
 * @param multiplier The music volume multiplier (0.0 to 1.0)
 */
void AudioController::setMusicVolumeMultiplier(float multiplier) {
    float clamped = std::max(0.0f, std::min(1.0f, multiplier));
    if (clamped == _musicVolumeMultiplier) return;
    
    _musicVolumeMultiplier = clamped;
    if (_debug) CULog("AudioController: Set music volume multiplier to %.2f", _musicVolumeMultiplier);
    
    auto engine = AudioEngine::get();
    if (engine) {
        auto musicQueue = engine->getMusicQueue();
        if (musicQueue) {
            musicQueue->setVolume(_musicVolumeMultiplier);
        }
    }
}

/**
 * Gets the current music volume multiplier.
 *
 * @return the music volume multiplier
 */
float AudioController::getMusicVolumeMultiplier() const {
    return _musicVolumeMultiplier;
}

/**
 * Sets the SFX volume multiplier for all sound effects.
 * This multiplies with the default volume set in JSON.
 *
 * @param multiplier The SFX volume multiplier (0.0 to 1.0)
 */
void AudioController::setSFXVolumeMultiplier(float multiplier) {
    _sfxVolumeMultiplier = std::max(0.0f, std::min(1.0f, multiplier));
    if (_debug) CULog("AudioController: Set SFX volume multiplier to %.2f", _sfxVolumeMultiplier);
}

/**
 * Gets the current SFX volume multiplier.
 *
 * @return the SFX volume multiplier
 */
float AudioController::getSFXVolumeMultiplier() const {
    return _sfxVolumeMultiplier;
}

/**
 * Loads sound/music default volumes from assets.json
 */
void AudioController::loadDefaultVolumes() {
    _defaultVolumes.clear();
    
    // Try to load assets.json using JsonReader
    auto reader = JsonReader::allocWithAsset("json/assets.json");
    if (reader == nullptr) {
        if (_debug) CULog("AudioController: Failed to load assets.json");
        return;
    }
    
    auto assetsJson = reader->readJson();
    if (assetsJson == nullptr) {
        if (_debug) CULog("AudioController: Failed to parse assets.json");
        return;
    }
    
    // Read sounds section
    auto soundsJson = assetsJson->get("sounds");
    if (soundsJson == nullptr || !soundsJson->isObject()) {
        if (_debug) CULog("AudioController: No sounds section in assets.json");
        return;
    }
    
    // Iterate through all sounds dynamically from JSON object children
    const auto& children = soundsJson->children();
    for (const auto& soundEntry : children) {
        std::string soundName = soundEntry->key();
        if (soundEntry->isObject() && soundEntry->has("volume")) {
            float volume = soundEntry->get("volume")->asFloat();
            _defaultVolumes[soundName] = volume;
            if (_debug) CULog("AudioController: Loaded default volume for '%s': %.2f", soundName.c_str(), volume);
        }
    }
}

/**
 * Gets the default volume for a sound key from the loaded volumes map.
 * Returns 1.0f if not found.
 *
 * @param soundKey The sound asset key
 * @return the default volume (0.0 to 1.0)
 */
float AudioController::getDefaultVolume(const std::string& soundKey) const {
    auto it = _defaultVolumes.find(soundKey);
    if (it != _defaultVolumes.end()) {
        return it->second;
    }
    return 1.0f;  // Default to 1.0f if not found
}

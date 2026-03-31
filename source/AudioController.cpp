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
        CULog("AudioController: assets cannot be null");
        return false;
    }
    _assets = assets;
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
        CULog("AudioController: AudioEngine started with %u slots", slots);
    } else {
        CULog("AudioController: Failed to start AudioEngine");
    }
    return success;
}

/**
 * Stops the audio engine, releasing all audio resources.
 */
void AudioController::stopAudioEngine() {
    AudioEngine::stop();
    CULog("AudioController: AudioEngine stopped");
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
                                bool loop, float volume, bool force) {
    if (_assets == nullptr) {
        CULog("AudioController::playSound: assets not initialized");
        return false;
    }

    auto engine = AudioEngine::get();
    if (engine == nullptr) {
        CULog("AudioController::playSound: AudioEngine not initialized");
        return false;
    }

    auto sound = _assets->get<Sound>(soundKey);
    if (sound == nullptr) {
        CULog("AudioController::playSound: sound '%s' not found in assets", soundKey.c_str());
        return false;
    }

    bool success = engine->play(key, sound, loop, volume, force);
    if (success) {
        CULog("AudioController: Playing sound '%s' with key '%s'", soundKey.c_str(), key.c_str());
    } else {
        CULog("AudioController: Failed to play sound '%s' (no available slots)", soundKey.c_str());
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
        CULog("AudioController: Stopped sound with key '%s'", key.c_str());
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
        CULog("AudioController: Paused sound with key '%s'", key.c_str());
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
        CULog("AudioController: Resumed sound with key '%s'", key.c_str());
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
        CULog("AudioController: Set volume for '%s' to %.2f", key.c_str(), volume);
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
        CULog("AudioController: Stopped all sounds");
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
        CULog("AudioController: Paused all sounds");
    }
}

/**
 * Resumes all paused sounds.
 */
void AudioController::resumeAllSounds() {
    auto engine = AudioEngine::get();
    if (engine != nullptr) {
        engine->resume();
        CULog("AudioController: Resumed all sounds");
    }
}

/**
 * Plays a music track from the asset manager through the music queue.
 *
 * @param soundKey The key to retrieve the music from the asset manager
 * @param loop Whether the music should loop continuously
 */
void AudioController::playMusic(const std::string& soundKey, bool loop) {
    if (_assets == nullptr) {
        CULog("AudioController::playMusic: assets not initialized");
        return;
    }

    auto engine = AudioEngine::get();
    if (engine == nullptr) {
        CULog("AudioController::playMusic: AudioEngine not initialized");
        return;
    }

    auto sound = _assets->get<Sound>(soundKey);
    if (sound == nullptr) {
        CULog("AudioController::playMusic: music '%s' not found in assets", soundKey.c_str());
        return;
    }

    auto musicQueue = engine->getMusicQueue();
    if (musicQueue == nullptr) {
        CULog("AudioController::playMusic: failed to get music queue");
        return;
    }

    musicQueue->play(sound, loop);
    CULog("AudioController: Playing music '%s' (loop=%d)", soundKey.c_str(), loop);
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
            CULog("AudioController: Stopped music");
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
            CULog("AudioController: Paused music");
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
            CULog("AudioController: Resumed music");
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
            CULog("AudioController: Set music volume to %.2f", volume);
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

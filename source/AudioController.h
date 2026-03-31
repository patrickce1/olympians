#ifndef __AUDIO_CONTROLLER_H__
#define __AUDIO_CONTROLLER_H__

#include <cugl/cugl.h>
#include <memory>
#include <unordered_map>

using namespace cugl;
using namespace cugl::audio;

/**
 * Controller for managing audio playback and the audio engine.
 *
 * This class abstracts all audio playing logic and provides a simple
 * interface for playing sounds, managing volume, and controlling the
 * audio lifecycle.
 */
class AudioController {
protected:
    /** Reference to the asset manager for loading sounds */
    std::shared_ptr<AssetManager> _assets;

public:
    /**
     * Creates a new audio controller (uninitialized).
     */
    AudioController();

    /**
     * Disposes of this audio controller, releasing all resources.
     */
    ~AudioController() { dispose(); }

    /**
     * Initializes the audio controller with the given asset manager.
     *
     * @param assets The asset manager containing loaded sounds
     * @return true if initialization succeeds; false otherwise.
     */
    bool init(const std::shared_ptr<AssetManager>& assets);

    /**
     * Disposes of all resources allocated by this controller.
     */
    void dispose();

    /**
     * Starts the audio engine.
     *
     * @param slots The maximum number of simultaneous sound slots (default 16)
     * @return true if the audio engine started successfully
     */
    bool startAudioEngine(Uint32 slots = 16);

    /**
     * Stops the audio engine, releasing all audio resources.
     */
    void stopAudioEngine();

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
    bool playSound(const std::string& key, const std::string& soundKey,
                   bool loop = false, float volume = 1.0f, bool force = false);

    /**
     * Stops a currently playing sound.
     *
     * @param key The unique identifier for the sound to stop
     * @param fade The fade-out duration in seconds (0 for immediate stop)
     */
    void stopSound(const std::string& key, float fade = 0.0f);

    /**
     * Pauses a currently playing sound.
     *
     * @param key The unique identifier for the sound to pause
     * @param fade The fade-out duration in seconds (0 for immediate pause)
     */
    void pauseSound(const std::string& key, float fade = 0.0f);

    /**
     * Resumes a paused sound.
     *
     * @param key The unique identifier for the sound to resume
     */
    void resumeSound(const std::string& key);

    /**
     * Sets the volume of a playing sound.
     *
     * @param key The unique identifier for the sound
     * @param volume The new volume level (0.0 to 1.0)
     */
    void setVolume(const std::string& key, float volume);

    /**
     * Gets the current volume of a playing sound.
     *
     * @param key The unique identifier for the sound
     * @return the current volume level, or 0 if not found
     */
    float getVolume(const std::string& key) const;

    /**
     * Stops all currently playing sounds.
     *
     * @param fade The fade-out duration in seconds (0 for immediate stop)
     */
    void stopAllSounds(float fade = 0.0f);

    /**
     * Pauses all currently playing sounds.
     *
     * @param fade The fade-out duration in seconds (0 for immediate pause)
     */
    void pauseAllSounds(float fade = 0.0f);

    /**
     * Resumes all paused sounds.
     */
    void resumeAllSounds();
};

#endif /* __AUDIO_CONTROLLER_H__ */

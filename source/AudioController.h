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
    /** Debug boolean. Set to false to prevent debug statements */
    bool _debug = false;

    /** Reference to the asset manager for loading sounds */
    std::shared_ptr<AssetManager> _assets;
    
    /** Counter for generating unique sound keys */
    int _soundCounter = 0;
    
    /** Key of the currently playing music track */
    std::string _currentMusicKey = "";
    
    /** Master volume multiplier for music (0.0 to 1.0) */
    float _musicVolumeMultiplier = 1.0f;
    
    /** Master volume multiplier for sound effects (0.0 to 1.0) */
    float _sfxVolumeMultiplier = 1.0f;
    
    /** Map of sound/music keys to their default volumes from assets.json */
    std::unordered_map<std::string, float> _defaultVolumes;

    /**
     * Loads sound/music default volumes from assets.json
     */
    void loadDefaultVolumes();
    
    /**
     * Gets the default volume for a sound key from the loaded volumes map.
     * Returns 1.0f if not found.
     *
     * @param soundKey The sound asset key
     * @return the default volume (0.0 to 1.0)
     */
    float getDefaultVolume(const std::string& soundKey) const;

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
     * Plays a sound effect from the asset manager.
     *
     * Sound effects use a limited slot-based system (separate from music).
     * Multiple effects can play simultaneously. For background music, use playMusic() instead.
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

    /**
     * Plays a music track from the asset manager through the music queue.
     *
     * Music is played on a separate queue from sound effects, allowing
     * seamless background music playback without interference from SFX.
     *
     * @param soundKey The key to retrieve the music from the asset manager
     * @param loop Whether the music should loop continuously
     * @param volume The music volume (0.0 to 1.0), defaults to 1.0
     */
    void playMusic(const std::string& soundKey, bool loop = true, float volume = 1.0f);

    /**
     * Stops the currently playing music track.
     *
     * @param fade The fade-out duration in seconds (0 for immediate stop)
     */
    void stopMusic(float fade = 0.5f);

    /**
     * Pauses the currently playing music.
     *
     * @param fade The fade-out duration in seconds (0 for immediate pause)
     */
    void pauseMusic(float fade = 0.5f);

    /**
     * Resumes the paused music.
     */
    void resumeMusic();

    /**
     * Sets the volume of the music track.
     *
     * @param volume The music volume (0.0 to 1.0)
     */
    void setMusicVolume(float volume);

    /**
     * Gets the current volume of the music track.
     *
     * @return the current music volume level
     */
    float getMusicVolume() const;

    /**
     * Sets the music volume multiplier for all music playback.
     * This multiplies with the default volume set in JSON.
     *
     * @param multiplier The music volume multiplier (0.0 to 1.0)
     */
    void setMusicVolumeMultiplier(float multiplier);

    /**
     * Gets the current music volume multiplier.
     *
     * @return the music volume multiplier
     */
    float getMusicVolumeMultiplier() const;

    /**
     * Sets the SFX volume multiplier for all sound effects.
     * This multiplies with the default volume set in JSON.
     *
     * @param multiplier The SFX volume multiplier (0.0 to 1.0)
     */
    void setSFXVolumeMultiplier(float multiplier);

    /**
     * Gets the current SFX volume multiplier.
     *
     * @return the SFX volume multiplier
     */
    float getSFXVolumeMultiplier() const;

    /**
     * Plays a sound effect with an automatically generated unique key.
     *
     * Useful for sound effects that may be triggered multiple times in quick
     * succession. Each call generates a unique key, allowing multiple instances
     * of the same sound to play simultaneously.
     *
     * @param soundKey The key to retrieve the sound from the asset manager
     * @param loop Whether the sound should loop continuously
     * @param volume The playback volume (0.0 to 1.0)
     * @return true if the sound was successfully added to the audio engine
     */
    bool playSoundUnique(const std::string& soundKey, bool loop = false, float volume = 1.0f);
};
#endif /* __AUDIO_CONTROLLER_H__ */

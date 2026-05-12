#ifndef __ENEMY_LOADER_H__
#define __ENEMY_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>
#include <vector>

class EnemyLoader {
public:
    /* Enum to represent state. All bosses follow the pattern of Passive, 3 Attacks, and 1 Defensive move*/
    enum State {
        IDLE,
        PASSIVE_SPECIAL,  // special states that come as a result of our passive. Ex. Cerberus stun. Bosses can optionally include this move
        ATTACK_1,
        ATTACK_2,
        ATTACK_3,
        DEFENSE_MOVE,
    };

    enum class EventType { 
        DAMAGE, 
        HEAL, 
        SIDE_MODIFIER, 
        PLAYER_SCRAMBLE, 
        VINE,
        UNKNOWN 
    };

    // Enum that tracks which boss this is
    enum Boss {
        CIRCE = 0,
        CERBERUS = 1,
        CYCLOPS = 2,
        GAIA = 3
    };

    struct EventDef {
        EventType type = EventType::UNKNOWN;
        int target = 0;                            // relative index offset. What player to attack or what side to modify. Heal ignores this and self targets
        float amount = 0.0f;                       // damage amount, heal amount, or multiplier change
        float duration = 0.0f;
    };

    struct StateDef {
        State state;
        std::string name;
        float buildUpTime = 0.0f;
        float cooldownTime = 0.0f;
        State nextState = IDLE;                
        std::vector<EventDef> entryEvents;  // fired immediately when entering this state
        std::vector<EventDef> events;       // fired when the state completes
        std::string animationKey;           // Key to lookup animation in enemyAnimations.json
        int loopStartFrame = -1;            // First frame of loop range (-1 = no loop, play linearly)
        int loopEndFrame = -1;              // Last frame of loop range
        int frameCount = 0;                 // 0 for looping states, actual count for linear-play states
        float frameDuration = 0.0f;         // Duration per frame in seconds
        int damageFrame = -1;               // Frame index when events fire (-1 = fire at loop end or last frame)
        int outroFrameCount = 0;            // Frames after loopEndFrame that play before state exits
    };

    struct AIConfig {
        float retargetLikelihood = 0.0f;
        float defenseLikelihood = 0.0f;
    };

    AIConfig ai;
    struct EnemyDef {
        std::string id;
        Boss name; //Not used yet, but will be used for boss animations in a future pr
        float maxHealth = 0.0f;
        std::string spritesheetPath;
        AIConfig ai;
        std::unordered_map<State, StateDef> states;
        std::shared_ptr<cugl::JsonValue> customData;
    };

private:
    std::unordered_map<std::string, EnemyDef> _enemies;
    /** A ordered vector of all enemies/bosses for selection */
    std::vector<EnemyDef> _enemiesVector;
    
    /** Animation metadata for calculating state durations */
    struct AnimationMetadata {
        int frameCount = 0;
        float frameDuration = 0.0f;
        int loopStartFrame = -1;  // First frame of loop range (-1 = no loop, play linearly)
        int loopEndFrame = -1;
        int damageFrame = -1;
    };
    std::unordered_map<std::string, AnimationMetadata> _animationRegistry;

private:
    /** Parses an event type string from JSON into an EventType enum. */
    static EventType parseEventType(const std::string& s) {
        if (s == "DAMAGE")           return EventType::DAMAGE;
        if (s == "HEAL")             return EventType::HEAL;
        if (s == "SIDE_MODIFIER")    return EventType::SIDE_MODIFIER;
        if (s == "PLAYER_SCRAMBLE")  return EventType::PLAYER_SCRAMBLE;
        if (s == "VINE")             return EventType::VINE;
        return EventType::UNKNOWN;
    }

    /** Parses a state name string from JSON into a State enum. */
    static State parseStateType(const std::string& s) {
        if (s == "attack_1")        return State::ATTACK_1;
        if (s == "attack_2")        return State::ATTACK_2;
        if (s == "attack_3")        return State::ATTACK_3;
        if (s == "defensive_move")  return State::DEFENSE_MOVE;
        if (s == "passive_special") return State::PASSIVE_SPECIAL;
        return State::IDLE;
    }

    /** Parses a boss name string from JSON into a Boss enum. */
    static Boss parseBoss(const std::string& s) {
        if (s == "cyclops")  return Boss::CYCLOPS;
        if (s == "cerberus") return Boss::CERBERUS;
        if (s == "circe") return Boss::CIRCE;
        if (s == "gaia") return Boss::GAIA;
        CUAssertLog(false, "Unknown boss type: %s", s.c_str());
        return Boss::CYCLOPS;
    }

public:
    /** Loads animation metadata from enemyAnimations.json into registry.
     * Animation metadata (frameCount, buildupFrameCount, frameDuration) is used to calculate
     * state durations and animation frame sequences.
     * 
     * Should be called before or during loadFromFile so state definitions can access the registry.
     * 
     * @param assets AssetManager containing the enemyAnimations.json asset
     * @return true if loaded successfully, false on error
     */
    bool loadAnimationRegistry(const std::shared_ptr<cugl::AssetManager>& assets) {
        _animationRegistry.clear();
        
        auto json = assets->get<cugl::JsonValue>("enemyAnimations");
        if (!json) {
            CULog("WARNING: Could not find 'enemyAnimations' asset");
            return false;
        }
        
        auto registryArray = json->get("animationRegistry");
        if (!registryArray || !registryArray->isArray()) {
            CULog("WARNING: enemyAnimations.json missing 'animationRegistry' array");
            return false;
        }
        
        for (int i = 0; i < registryArray->size(); i++) {
            auto entry = registryArray->get(i);
            if (!entry) continue;
            
            AnimationMetadata meta;
            meta.frameCount = entry->getInt("frameCount", 0);
            meta.frameDuration = entry->getFloat("frameDuration", 0.1f);
            meta.loopStartFrame = entry->getInt("loopStartFrame", -1);
            meta.loopEndFrame = entry->getInt("loopEndFrame", -1);
            meta.damageFrame = entry->getInt("damageFrame", -1);
            
            std::string id = entry->getString("id", "");
            if (!id.empty()) {
                _animationRegistry[id] = meta;
            }
        }
        
        return true;
    }
    
    /** Loads and parses all enemy definitions from a JSON file.
     * Populates enemy definitions including states, animations, AI parameters, and events.
     * If animation registry is already loaded, state definitions will include animation metadata.
     * 
     * @param path Path to enemies.json file
     * @return true if loaded successfully, false on error
     */
    bool loadFromFile(const std::string& path) {
        auto reader = cugl::JsonReader::alloc(path);
        if (!reader) return false;

        auto json = reader->readJson();
        if (!json) return false;

        auto enemyArray = json->get("enemies");
        if (!enemyArray || !enemyArray->isArray()) return false;

        for (int i = 0; i < enemyArray->size(); i++) {
            auto entry = enemyArray->get(i);
            if (!entry) continue;

            EnemyDef def;
            def.id = entry->getString("id");
            def.name = parseBoss(entry->getString("name"));
            def.maxHealth = entry->getFloat("maxHealth");
            def.spritesheetPath = entry->getString("spritesheetPath");
            def.customData = entry->get("customData");

            auto statesObj = entry->get("states");
            CUAssertLog(statesObj && statesObj->isObject(),
                        "Enemy '%s' missing object 'states'", def.id.c_str());

            for (int k = 0; k < statesObj->size(); k++) {
                auto stateJson = statesObj->get(k);
                if (!stateJson) continue;

                StateDef stateDef;
                stateDef.state = parseStateType(stateJson->_key); 
                stateDef.name = stateJson->getString("name", "");
                stateDef.buildUpTime  = stateJson->getFloat("buildUpTime", 0.0f);
                stateDef.cooldownTime = stateJson->getFloat("cooldownTime", 0.0f);
                stateDef.nextState    = parseStateType(stateJson->getString("nextState", "idle"));
                stateDef.animationKey = stateJson->getString("animationKey", "");
                
                // Populate animation metadata from registry if available
                if (!stateDef.animationKey.empty() && _animationRegistry.count(stateDef.animationKey) > 0) {
                    const auto& animMeta = _animationRegistry.at(stateDef.animationKey);
                    stateDef.frameDuration  = animMeta.frameDuration;
                    stateDef.damageFrame    = animMeta.damageFrame;
                    stateDef.loopStartFrame = animMeta.loopStartFrame;
                    stateDef.loopEndFrame   = animMeta.loopEndFrame;
                    if (animMeta.loopStartFrame >= 0) {
                        // Looping states: frameCount=0 so readyToFire() uses buildUpTime
                        stateDef.frameCount = 0;
                        int loopEnd = (animMeta.loopEndFrame >= 0) ? animMeta.loopEndFrame : animMeta.frameCount - 1;
                        stateDef.outroFrameCount = std::max(0, animMeta.frameCount - loopEnd - 1);
                    } else {
                        stateDef.frameCount = animMeta.frameCount;
                    }
                }

                auto aiObj = entry->get("ai");
                if (aiObj && aiObj->isObject()) {
                    def.ai.retargetLikelihood =
                        aiObj->getFloat("retargetLikelihood", 0.0f);
                    def.ai.defenseLikelihood = aiObj->getFloat("defenseLikelihood", 0.05f);
                }
                
                // Shared parser for both entryEvents and events arrays
                auto parseEventArray = [](const std::shared_ptr<cugl::JsonValue>& arr, std::vector<EventDef>& out) {
                    if (!arr || !arr->isArray()) return;
                    for (int j = 0; j < arr->size(); j++) {
                        auto eventJson = arr->get(j);
                        if (!eventJson) continue;
                        EventDef eventDef;
                        eventDef.type   = parseEventType(eventJson->getString("type", ""));
                        // "target" is a relative player-index offset; only meaningful for DAMAGE and SIDE_MODIFIER
                        if (eventDef.type == EventType::DAMAGE || eventDef.type == EventType::SIDE_MODIFIER || eventDef.type == EventType::VINE) {
                            eventDef.target = eventJson->getInt("target", 0);
                        }
                        //player scramble has no amount or duration, it just happens
                        if (eventDef.type != EventType::PLAYER_SCRAMBLE) {
                            eventDef.amount = eventJson->getFloat("amount", 0.0f);
                            eventDef.duration = eventJson->getFloat("duration", 0.0f);
                        }
                        
                        out.push_back(eventDef);
                    }
                };
                parseEventArray(stateJson->get("entryEvents"), stateDef.entryEvents);
                parseEventArray(stateJson->get("events"),      stateDef.events);

                def.states[stateDef.state] = stateDef;
            }

            CUAssertLog(def.states.count(State::IDLE) > 0,
                "Enemy '%s' must define an 'idle' state", def.id.c_str());

            CULog("Loaded Enemy: id=%s name=%d maxHealth=%.2f states=%zu sprite=%s",
                def.id.c_str(),
                def.name,
                def.maxHealth,
                def.states.size(),
                def.spritesheetPath.c_str());

            _enemies[def.id] = def;
            _enemiesVector.push_back(def);
        }
        return true;
    }

    bool has(const std::string& id) const { return _enemies.count(id) > 0; }
    
    /**
     * Retrieves an enemy definition by ID.
     * 
     * @param id The unique identifier of the enemy to retrieve
     * @return Reference to the enemy definition
     * @throws std::out_of_range if the enemy ID is not found
     */
    const EnemyDef& get(const std::string& id) const { return _enemies.at(id); }
    
    /**
     * Returns all loaded enemy definitions in a lookup map.
     * 
     * @return Reference to the map of enemy ID -> EnemyDef
     */
    const std::unordered_map<std::string, EnemyDef>& getAll() const { return _enemies; }
    
    /**
     * Returns all loaded enemy definitions in a stable order.
     * Use this for UI iteration where order matters (e.g., boss selection screens).
     * 
     * @return Reference to the ordered vector of EnemyDef
     */
    const std::vector<EnemyDef>& getAllOrdered() const { return _enemiesVector; }
    
    /**
     * Checks if the animation registry has been successfully loaded.
     * Used for smart initialization to verify animation metadata is available.
     * 
     * @return true if animation registry is populated, false otherwise
     */
    bool isAnimationRegistryLoaded() const { return !_animationRegistry.empty(); }
};

#endif /* !__ENEMY_LOADER_H__ */

#ifndef __ENEMY_LOADER_H__
#define __ENEMY_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>
#include <vector>

class EnemyLoader {
public:
    /** Relative direction of an event target, interpreted against enemy facing */
    enum class RelDir { FRONT, BACK, LEFT, RIGHT, UNKNOWN };
    
    /** Types of gameplay events a state can emit (can add more later) */
    enum class EventType { DAMAGE, UNKNOWN };

    /** Definition of a single state event loaded from JSON */
    struct EventDef {
        EventType type = EventType::UNKNOWN;
        RelDir target = RelDir::UNKNOWN;
        float amount = 0.0f;
        float duration = 0.0f;
    };

    /** Definition of an enemy state loaded from JSON */
    struct StateDef {
        std::string name;
        int animationRow = 0;

        // Execution flow:
        // enter -> wait buildUpTime -> fire events -> transition to nextState -> apply cooldown lockout
        float buildUpTime = 0.0f; // seconds until events fire
        float cooldownTime = 0.0f; // seconds of lockout AFTER events fire
        std::string nextState; // state to enter immediately after firing events (usually "idle")
        std::vector<EventDef> events;
    };

    /** Full enemy definition loaded from JSON */
    struct EnemyDef {
        std::string id;
        std::string name;
        float maxHealth = 0.0f;
        std::string spritesheetPath;

        // multiplier for how much incoming damage should be reduced. can be implemented by combat system later
        struct Shields {
            float front = 0.0f;
            float back  = 0.0f;
            float left  = 0.0f;
            float right = 0.0f;
        } defaultShields;

        std::unordered_map<std::string, StateDef> states;
    };

private:
    /** Mapping from enemy id -> enemy definition */
    std::unordered_map<std::string, EnemyDef> _enemies;

private:
    /** Converts a string into a relative direction enum */
    static RelDir parseRelDir(const std::string& s) {
        if (s == "front") return RelDir::FRONT;
        if (s == "back")  return RelDir::BACK;
        if (s == "left")  return RelDir::LEFT;
        if (s == "right") return RelDir::RIGHT;
        return RelDir::UNKNOWN;
    }
    
    /** Converts a string into an event type enum */
    static EventType parseEventType(const std::string& s) {
        if (s == "DAMAGE") return EventType::DAMAGE;
        return EventType::UNKNOWN;
    }

public:
    /**
     * Loads all enemies from the given JSON file path.
     * Expected structure:
     * {
     *   "enemies": [
     *     { "id":..., "name":..., "maxHealth":..., "spritesheetPath":...,
     *       "defaultShields": {...},
     *       "states": { "idle": {...}, "attack1": {...}, ... }
     *     }
     *   ]
     * }
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
            def.name = entry->getString("name");
            def.maxHealth = entry->getFloat("maxHealth");
            def.spritesheetPath= entry->getString("spritesheetPath");

            // optional shields
            auto shields = entry->get("defaultShields");
            if (shields) {
                def.defaultShields.front = shields->getFloat("front", 0.0f);
                def.defaultShields.back = shields->getFloat("back",  0.0f);
                def.defaultShields.left = shields->getFloat("left",  0.0f);
                def.defaultShields.right = shields->getFloat("right", 0.0f);
            }

            // every enemy REQUIRED to at least have "idle" state.
            auto statesObj = entry->get("states");
            CUAssertLog(statesObj && statesObj->isObject(), "Enemy '%s' missing object 'states'", def.id.c_str());

            for (int i = 0; i < statesObj->size(); i++) {
                auto st = statesObj->get(i);
                if (!st) continue;

                StateDef sdef;
                sdef.name = st->_key;
                sdef.animationRow = st->getInt("animationRow", 0);
                sdef.buildUpTime = st->getFloat("buildUpTime", 0.0f);
                sdef.cooldownTime = st->getFloat("cooldownTime", 0.0f);
                
                // Default to idle if nextState not specified
                sdef.nextState = st->getString("nextState", "idle");

                // optional events (e.g. players taking damage)
                auto evArr = st->get("events");
                if (evArr && evArr->isArray()) {
                    for (int j = 0; j < evArr->size(); j++) {
                        auto ev = evArr->get(j);
                        if (!ev) continue;

                        EventDef edef;
                        edef.type = parseEventType(ev->getString("type", ""));

                        // currently only DAMAGE supported
                        if (edef.type == EventType::DAMAGE) {
                            edef.target = parseRelDir(ev->getString("target", ""));
                            edef.amount = ev->getFloat("amount", 0.0f);
                        }

                        sdef.events.push_back(edef);
                    }
                }

                def.states[sdef.name] = sdef;
            }

            // Validate "idle" state exists
            CUAssertLog(def.states.count("idle") > 0,
                        "Enemy '%s' must define an 'idle' state", def.id.c_str());
            
            // Log loaded enemy
            CULog("Loaded Enemy: id=%s name=%s maxHealth=%.2f states=%zu sprite=%s",
                  def.id.c_str(),
                  def.name.c_str(),
                  def.maxHealth,
                  def.states.size(),
                  def.spritesheetPath.c_str());
            
            _enemies[def.id] = def;
        }
        
        return true;
    }
    
    /** Returns true if a definition exists for the given id */
    bool has(const std::string& id) const { return _enemies.count(id) > 0; }
    
    /** Returns the enemy definition for the given id (must exist) */
    const EnemyDef& get(const std::string& id) const { return _enemies.at(id); }
    
    /** Returns all loaded enemy definitions */
    const std::unordered_map<std::string, EnemyDef>& getAll() const { return _enemies; }
};

#endif /* !__ENEMY_LOADER_H__ */

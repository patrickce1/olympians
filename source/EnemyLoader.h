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

    enum class EventType { DAMAGE, HEAL, SIDE_MODIFIER, UNKNOWN };

    // Enum that tracks which boss this is
    enum Boss {
        CYCLOPS = 0,
        CERBERUS = 1,
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
        std::vector<EventDef> events;
        
        // Animation key to lookup metadata in the animation registry
        std::string animationKey;           // Key to lookup animation in enemyAnimations.json
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

private:
    /** Parses an event type string from JSON into an EventType enum. */
    static EventType parseEventType(const std::string& s) {
        if (s == "DAMAGE")           return EventType::DAMAGE;
        if (s == "HEAL")             return EventType::HEAL;
        if (s == "SIDE_MODIFIER")  return EventType::SIDE_MODIFIER;
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
        CUAssertLog(false, "Unknown boss type: %s", s.c_str());
        return Boss::CYCLOPS;
    }

public:
    /** Loads and parses all enemy definitions from a JSON file at the given path. Returns true on success. */
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
                auto st = statesObj->get(k);
                if (!st) continue;

                StateDef sdef;
                sdef.state = parseStateType(st->_key); 
                sdef.name = st->getString("name", "");
                sdef.buildUpTime  = st->getFloat("buildUpTime", 0.0f);
                sdef.cooldownTime = st->getFloat("cooldownTime", 0.0f);
                sdef.nextState    = parseStateType(st->getString("nextState", "idle"));
                
                // Read animation key (metadata loads from animation registry, not JSON)
                sdef.animationKey = st->getString("animationKey", "");

                auto aiObj = entry->get("ai");
                if (aiObj && aiObj->isObject()) {
                    def.ai.retargetLikelihood =
                        aiObj->getFloat("retargetLikelihood", 0.0f);
                    def.ai.defenseLikelihood = aiObj->getFloat("defenseLikelihood", 0.05f);
                }
                
                auto evArr = st->get("events");
                if (evArr && evArr->isArray()) {
                    for (int j = 0; j < evArr->size(); j++) {
                        auto ev = evArr->get(j);
                        if (!ev) continue;

                        EventDef edef;
                        edef.type = parseEventType(ev->getString("type", ""));

                        //A "target" only applies to damage and side modifiers, not boss healing self
                        if (edef.type == EventType::DAMAGE || edef.type == EventType::SIDE_MODIFIER) {
                            edef.target = ev->getInt("target", 0);
                        }
                        edef.amount = ev->getFloat("amount", 0.0f);
                        edef.duration = ev->getFloat("duration", 0.0f);
                        sdef.events.push_back(edef);
                    }
                }

                def.states[sdef.state] = sdef;
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
    const EnemyDef& get(const std::string& id) const { return _enemies.at(id); }
    const std::unordered_map<std::string, EnemyDef>& getAll() const { return _enemies; }
    /** Returns all the mapping of enemy id -> EnemyDef */
    const std::vector<EnemyDef>& getAllOrdered() const { return _enemiesVector; }
};

#endif /* !__ENEMY_LOADER_H__ */

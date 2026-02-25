#ifndef __ENEMY_LOADER_H__
#define __ENEMY_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>
#include <vector>

class EnemyLoader {
public:
    enum class RelDir { FRONT, BACK, LEFT, RIGHT, UNKNOWN };
    enum class EventType { DAMAGE, UNKNOWN }; // can add more types later

    struct EventDef {
        EventType type = EventType::UNKNOWN;

        RelDir target = RelDir::UNKNOWN; // front/back/left/right relative to facing
        float amount = 0.0f;

        float duration = 0.0f;
    };

    struct StateDef {
        std::string name;

        int animationRow = 0;

        // State execution:
        // enter -> wait buildUpTime -> fire events -> transition to nextState -> apply cooldown lockout
        float buildUpTime = 0.0f; // seconds until events fire
        float cooldownTime = 0.0f; // seconds of lockout AFTER events fire
        std::string nextState; // state to enter immediately after firing events (usually "idle")

        std::vector<EventDef> events;
    };

    struct EnemyDef {
        std::string id;
        std::string name;
        float maxHealth = 0.0f;

        std::string spritesheetPath;

        // can be used by combat system later, multiplier for how much incoming damage should be reduced
        struct Shields {
            float front = 0.0f;
            float back  = 0.0f;
            float left  = 0.0f;
            float right = 0.0f;
        } defaultShields;

        std::unordered_map<std::string, StateDef> states;
    };

private:
    std::unordered_map<std::string, EnemyDef> _enemies;

private:
    static RelDir parseRelDir(const std::string& s) {
        if (s == "front") return RelDir::FRONT;
        if (s == "back")  return RelDir::BACK;
        if (s == "left")  return RelDir::LEFT;
        if (s == "right") return RelDir::RIGHT;
        return RelDir::UNKNOWN;
    }

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

            // defaultShields optional
            auto shields = entry->get("defaultShields");
            if (shields) {
                def.defaultShields.front = shields->getFloat("front", 0.0f);
                def.defaultShields.back = shields->getFloat("back",  0.0f);
                def.defaultShields.left = shields->getFloat("left",  0.0f);
                def.defaultShields.right = shields->getFloat("right", 0.0f);
            }

            // states required. every enemy must at least have "idle" state.
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
                sdef.nextState = st->getString("nextState", "idle");

                // optional events for the state (e.g. players taking damage)
                auto evArr = st->get("events");
                if (evArr && evArr->isArray()) {
                    for (int j = 0; j < evArr->size(); j++) {
                        auto ev = evArr->get(j);
                        if (!ev) continue;

                        EventDef edef;
                        edef.type = parseEventType(ev->getString("type", ""));

                        // can add more event types later. maybe status effects?
                        if (edef.type == EventType::DAMAGE) {
                            edef.target = parseRelDir(ev->getString("target", ""));
                            edef.amount = ev->getFloat("amount", 0.0f);
                        }

                        sdef.events.push_back(edef);
                    }
                }

                def.states[sdef.name] = sdef;
            }

            // every enemy must have idle
            CUAssertLog(def.states.count("idle") > 0,
                        "Enemy '%s' must define an 'idle' state", def.id.c_str());

            _enemies[def.id] = def;
        }

        return true;
    }

    bool has(const std::string& id) const { return _enemies.count(id) > 0; }
    const EnemyDef& get(const std::string& id) const { return _enemies.at(id); }
    const std::unordered_map<std::string, EnemyDef>& getAll() const { return _enemies; }
};

#endif /* !__ENEMY_LOADER_H__ */

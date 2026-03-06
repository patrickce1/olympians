#ifndef __ENEMY_LOADER_H__
#define __ENEMY_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>
#include <vector>

class EnemyLoader {
public:
    enum class EventType { DAMAGE, UNKNOWN };

    struct EventDef {
        EventType type = EventType::UNKNOWN;
        int target = 0;                            // relative index offset
        float amount = 0.0f;
        float duration = 0.0f;
    };

    struct StateDef {
        std::string name;
        int animationRow = 0;
        std::string tag;
        float buildUpTime = 0.0f;
        float cooldownTime = 0.0f;
        std::string nextState;
        std::vector<EventDef> events;
    };

    struct AIConfig {
        float retargetLikelihood = 0.0f;
    };

    AIConfig ai;
    struct EnemyDef {
        std::string id;
        std::string name;
        float maxHealth = 0.0f;
        std::string spritesheetPath;
        AIConfig ai;
        std::unordered_map<std::string, StateDef> states;
    };

private:
    std::unordered_map<std::string, EnemyDef> _enemies;

private:
    static EventType parseEventType(const std::string& s) {
        if (s == "DAMAGE") return EventType::DAMAGE;
        return EventType::UNKNOWN;
    }

public:
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
            def.spritesheetPath = entry->getString("spritesheetPath");

            auto statesObj = entry->get("states");
            CUAssertLog(statesObj && statesObj->isObject(),
                        "Enemy '%s' missing object 'states'", def.id.c_str());

            for (int k = 0; k < statesObj->size(); k++) {
                auto st = statesObj->get(k);
                if (!st) continue;

                StateDef sdef;
                sdef.name = st->_key;
                sdef.animationRow = st->getInt("animationRow", 0);
                sdef.tag = st->getString("tag", "");
                sdef.buildUpTime  = st->getFloat("buildUpTime", 0.0f);
                sdef.cooldownTime = st->getFloat("cooldownTime", 0.0f);
                sdef.nextState    = st->getString("nextState", "idle");

                auto aiObj = entry->get("ai");
                if (aiObj && aiObj->isObject()) {
                    def.ai.retargetLikelihood =
                        aiObj->getFloat("retargetLikelihood", 0.0f);
                }
                
                auto evArr = st->get("events");
                if (evArr && evArr->isArray()) {
                    for (int j = 0; j < evArr->size(); j++) {
                        auto ev = evArr->get(j);
                        if (!ev) continue;

                        EventDef edef;
                        edef.type = parseEventType(ev->getString("type", ""));

                        if (edef.type == EventType::DAMAGE) {
                            edef.target = ev->getInt("target", 0);
                            edef.amount = ev->getFloat("amount", 0.0f);
                            edef.duration = ev->getFloat("duration", 0.0f);
                        }
                        sdef.events.push_back(edef);
                    }
                }

                def.states[sdef.name] = sdef;
            }

            CUAssertLog(def.states.count("idle") > 0,
                        "Enemy '%s' must define an 'idle' state", def.id.c_str());

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

    bool has(const std::string& id) const { return _enemies.count(id) > 0; }
    const EnemyDef& get(const std::string& id) const { return _enemies.at(id); }
    const std::unordered_map<std::string, EnemyDef>& getAll() const { return _enemies; }
};

#endif /* !__ENEMY_LOADER_H__ */

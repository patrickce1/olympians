// HouseLoader.h
#ifndef __HOUSE_LOADER_H__
#define __HOUSE_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>
#include <algorithm>

class HouseLoader {
    
public:

    /** This is a struct with all the properties of our houses defined in the JSON*/
    struct HouseDef {
        std::string id;
        float maxHealth;
        std::string spritesheetPath;
        std::vector<std::string> specialAbilities;
        float attack = 0.0f;
        float support = 0.0f;
        float utility = 0.0f;
        float affinityBonus = 1.5f;
    };
    
private:
    
    /** A mapping from houses to houseDef objects indexed by ID's */
    std::unordered_map<std::string, HouseDef> _houses;
    /** A ordered vector of all houses for later selection */
    std::vector<HouseDef> _housesVector;
    
public:

    /**
     * Loads all houses from the given JSON file path.
        * Call this once during startup.
     * @return true if loading succeeded
     */
    bool loadFromFile(const std::string& path) {
        _houses.clear();
        _housesVector.clear();
        
        // CUGL reads JSON files via JsonReader
        auto reader = cugl::JsonReader::alloc(path);
        if (!reader) return false;
        
        // Reads in the Json values
        auto json = reader->readJson();
        if (!json) return false;
        
        // Gets the houses array from JSON
        auto houseArray = json->get("houses");
        if (!houseArray) return false;
        
        // Creates mapping of house objects to HouseDef
        for (int houseIndex = 0; houseIndex < houseArray->size(); houseIndex++) {
            auto houseEntry = houseArray->get(houseIndex);
            HouseDef houseDef;
            houseDef.id                = houseEntry->getString("id", "");
            houseDef.maxHealth         = houseEntry->getFloat("maxHealth");
            houseDef.spritesheetPath   = houseEntry->getString("spritesheetPath");

            auto readSlider = [&](const char* key, float fallback) {
                if (!houseEntry->has(key) || !houseEntry->get(key)->isNumber()) {
                    return fallback;
                }
                float sliderValue = houseEntry->getFloat(key);
                return std::max(0.0f, std::min(1.0f, sliderValue));
            };

            houseDef.attack = readSlider("attack", 0.0f);
            houseDef.support = readSlider("support", 0.0f);
            houseDef.utility = readSlider("utility", 0.0f);

            if (houseEntry->has("affinityBonus") && houseEntry->get("affinityBonus")->isNumber()) {
                houseDef.affinityBonus = houseEntry->getFloat("affinityBonus");
                if (houseDef.affinityBonus <= 0.0f) {
                    houseDef.affinityBonus = 1.5f;
                }
            } else {
                houseDef.affinityBonus = 1.5f;
            }
            
            //Parsing the special abilities array
            auto specialAbilities = houseEntry->get("specialAbilities");
            if (specialAbilities){
                for (int j=0; j<specialAbilities->size(); j++) {
                    houseDef.specialAbilities.push_back(specialAbilities->get(j)->asString());
                }
            }
            
            _houses[houseDef.id] = houseDef;
            _housesVector.push_back(houseDef);
        }
        return true;
    }
    
    /**
     * Returns whether the HouseDef has a given house id.
     * Returns nullptr equivalent (use has() first) if not found.
     */
    bool has(const std::string& id) const {
        return _houses.count(id) > 0;
    }
    
    /**
     *Returns the HouseDef for the given house id
     */
    const HouseDef& get(const std::string& id) const {
        return _houses.at(id);
    }
    
    /**
     *Returns the all the HouseDefs in tthe order defined by the JSON
     */
    const std::vector<HouseDef>& getAllOrdered() const {
        return _housesVector;
    }
    
    /**
     *Returns all the mapping of house id -> HouseDef
     */
    const std::unordered_map<std::string, HouseDef>& getAll() const {
        return _houses;
    }
};

#endif /* !__HOUSE_LOADER_H__ */


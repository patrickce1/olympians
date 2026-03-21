// HouseLoader.h
#ifndef __HOUSE_LOADER_H__
#define __HOUSE_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>
#include "House.h"

class HouseLoader {
    
public:
    
    /** This is an enum for ability classes*/
    enum class AbilityClass {
        HEALER,
        DAMAGE_DEALER,
        ALL_ROUNDER
    };
    
    /** This is a struct with all the properties of our houses defined in the JSON*/
    struct HouseDef {
        std::string id;
        int house;
        float maxHealth;
        AbilityClass abilityClass;
        std::string spritesheetPath;
        std::vector<std::string> specialAbilities;
    };
    
private:
    
    /** A mapping from houses to houseDef objects indexed by ID's */
    std::unordered_map<std::string, HouseDef> _houses;
    
public:
    
    /**Returns the respective ability class given a string**/
    AbilityClass parseAbilityClass(const std::string& s) {
        if (s == "Healer")        return AbilityClass::HEALER;
        if (s == "Damage Dealer") return AbilityClass::DAMAGE_DEALER;
        return AbilityClass::ALL_ROUNDER;
    }
    
    /**Returns the respective house given a string**/
    int houseFromString(const std::string& s) {
        if (s == "Zeus") return House::ZEUS;
        if (s == "Poseidon") return House::POSEIDON;
        if (s == "Hades") return House::HADES;
        if (s == "Demeter") return House::DEMETER;
        if (s == "Athena") return House::ATHENA;
        if (s == "Aphrodite") return House::APHRODITE;
        if (s == "Ares") return House::ARES;
        if (s == "Hephestus") return House::HEPHESTUS;
        if (s == "Hermes") return House::HERMES;
        return House::NONE; // Athena is default
    }

    /**
     * Loads all houses from the given JSON file path.
     * Call this once during sta.
     * @return true if loading succeeded
     */
    bool loadFromFile(const std::string& path) {
        
        // CUGL reads JSON files via JsonReader
        auto reader = cugl::JsonReader::alloc(path);
        if (!reader) return false;
        
        // Reads in the Json values
        auto json = reader->readJson();
        if (!json) return false;
        
        // Gets the house object from JSON
        auto charArray = json->get("houses");
        if (!charArray) return false;
        
        // Creates mapping of house objects to HouseDef
        for (int i = 0; i < charArray->size(); i++) {
            auto entry = charArray->get(i);
            HouseDef def;
            def.id                = entry->getString("id");
            def.house             = houseFromString(entry->getString("house"));
            def.maxHealth         = entry->getFloat("maxHealth");
            def.abilityClass      = parseAbilityClass(entry->getString("abilityClass"));
            def.spritesheetPath   = entry->getString("spritesheetPath");
            
            //Parsing the special abilities array
            auto specialAbilities = entry->get("specialAbilities");
            if (specialAbilities){
                for (int j=0; j<specialAbilities->size(); j++) {
                    def.specialAbilities.push_back(specialAbilities->get(j)->asString());
                }
            }
            
            _houses[def.id] = def;
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
     *Returns all the mapping of house id -> HouseDef
     */
    const std::unordered_map<std::string, HouseDef>& getAll() const {
        return _houses;
    }
};

#endif /* !__HOUSE_LOADER_H__ */


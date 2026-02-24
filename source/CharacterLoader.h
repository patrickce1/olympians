// CharacterLoader.h
#ifndef __CHARACTER_LOADER_H__
#define __CHARACTER_LOADER_H__

#include <cugl/cugl.h>
#include <unordered_map>
#include <string>

class CharacterLoader {
    
private:
    
    /** A mapping from characters to characterDef objects indexed by ID's */
    std::unordered_map<std::string, CharacterDef> _characters;
    
public:
    
    /** This is an enum for ability classes*/
    enum class AbilityClass {
        HEALER,
        DAMAGE_DEALER,
        ALL_ROUNDER
    };
    
    /** This is a struct with all the properties of our characters defined in the JSON*/
    struct CharacterDef {
        std::string id;
        std::string house;
        float maxHealth;
        AbilityClass abilityClass;
        std::string spritesheetPath;
        std::vector<std::string> specialAbilities;
    };
    
    /**Returns the respective ability class given a string**/
    AbilityClass parseAbilityClass(const std::string& s) {
        if (s == "Healer")        return AbilityClass::HEALER;
        if (s == "Damage Dealer") return AbilityClass::DAMAGE_DEALER;
        return AbilityClass::ALL_ROUNDER;
    }

    /**
     * Loads all characters from the given JSON file path.
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
        
        // Gets the character object from JSON
        auto charArray = json->get("characters");
        if (!charArray) return false;
        
        // Creates mapping of character objects to CharacterDef
        for (int i = 0; i < charArray->size(); i++) {
            auto entry = charArray->get(i);
            CharacterDef def;
            
            def.id                = entry->getString("id");
            def.house             = entry->getString("house");
            def.maxHealth         = entry->getFloat("maxHealth");
            def.abilityClass      = parseAbilityClass(entry->getFloat("abilityClass"));
            def.spriteSheetPath   = entry->getString("spritesheetPath");
            
            //Parsing the special abilities array
            auto specialAbilities = entry->get("specialAbilities");
            if (specialAbilities){
                for (int j=0; j<specialAbilities->size(); j++) {
                    def.specialAbilities.push_back(specialAbilities->get(j)->asString());
                }
            }
            
            _characters[def.id] = def;
        }
        return true;
    }
    
    /**
     * Returns whether the CharacterDef has a given character id.
     * Returns nullptr equivalent (use has() first) if not found.
     */
    bool has(const std::string& id) const {
        return _characters.count(id) > 0;
    }
    
    /**
     *Returns the CharacterDef for the given character id
     */
    const CharacterDef& get(const std::string& id) const {
        return _characters.at(id);
    }
    
    /**
     *Returns all the mapping of character id -> CharacterDef
     */
    const std::unordered_map<std::string, CharacterDef>& getAll() const {
        return _characters;
    }
};

#endif /* !__CHARACTER_LOADER_H__ */

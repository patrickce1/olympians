//
//  ItemInstance.h
//  olympians
//
//  Created by Aiden Joo on 2/23/26.
//

#ifndef __ITEM_INSTANCE_H__
#define __ITEM_INSTANCE_H__

#include <cugl/cugl.h>
#include <cstdint>
#include <string>


/**
 * A specific spawned item in the match (what is owned and gets passed between players).
 *
 * Identity MUST be stable across transfers -> instanceId exists for networking & UI mapping.
 */
class ItemInstance {
public:
    using ItemId = std::uint64_t;
    
    /**
    * Host-authoritative generator
    *
    * - gameId: random per game/party
    * - counter: increment per spawn (per match)
    */
    class IdGenerator {
    private:
        std::uint32_t _gameId = 0;
        std::uint32_t _counter = 0;
        
    public:
        void startGame(std::uint32_t gameId) {
            _gameId = gameId;
            _counter = 0;
        }
        
        ItemId next() {
            ItemId id = (static_cast<ItemId>(_gameId) << 32) | static_cast<ItemId>(_counter);
            _counter++;
            return id;
        }
        
        static std::uint32_t randomGameId() {
            cugl::Random rng;
            if (rng.init()) {
                return (std::uint32_t)rng.getUint32();
            }
            // fallback
            return 0xA17D1234u;
        }
    };
    
private:
    ItemId _id = 0;
    std::string _defId;
    
public:
    ItemInstance() = default;
    ~ItemInstance() = default;
    
    /**
    * Create an item instance with a known id (host assigns).
    */
    bool init(const std::string& defId, ItemId id);
    
    static std::shared_ptr<ItemInstance> alloc(const std::string& defId, ItemId id) {
        auto result = std::make_shared<ItemInstance>();
        return (result->init(defId, id) ? result : nullptr);
    }
    
    // Getters
    ItemId getId() const { return _id; }
    const std::string& getDefId() const { return _defId; }
    
    // Used for sharing items
    std::shared_ptr<cugl::JsonValue> toJson() const;
    bool fromJson(const std::shared_ptr<cugl::JsonValue>& json);
};

#endif // __ITEM_INSTANCE_H__

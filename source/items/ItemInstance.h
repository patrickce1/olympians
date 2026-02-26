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
        /** The unique ID for this game session, packed into the upper 32 bits of each ItemId */
        std::uint32_t _gameId = 0;
        /** Monotonically increasing counter, packed into the lower 32 bits of each ItemId */
        std::uint32_t _counter = 0;
        
    public:
        
        /**
         * Initializes the generator for a new game session.
         * Stores the given gameId and resets the counter to 0.
         * Should be called once at the start of each match.
         *
         * @param gameId    The unique identifier for this game session
         */
        void startGame(std::uint32_t gameId) {
            _gameId = gameId;
            _counter = 0;
        }
        
        /**
         * Generates the next unique ItemId by packing gameId into the upper 32 bits
         * and the counter into the lower 32 bits. This ensures IDs are unique both
         * within a match and across different game sessions.
         *
         * @return a unique ItemId for this game session
         */
        ItemId next() {
            ItemId id = (static_cast<ItemId>(_gameId) << 32) | static_cast<ItemId>(_counter);
            _counter++;
            return id;
        }
        
        /**
         * Generates a random 32-bit game ID to seed the IdGenerator.
         * Uses CUGL's Random RNG if available, otherwise falls back to a hardcoded constant.
         * Should be called by the host once per game session.
         *
         * @return a random uint32 to use as a gameId, or 0xA17D1234 as a fallback
         */
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
     * Initializes an ItemInstance with a definition ID and a host-assigned unique ID.
     * Returns false if defId is empty or id is 0 (reserved as invalid).
     *
     * @param defId     The definition ID linking this instance to its ItemDef
     * @param id        The unique instance ID assigned by the host's IdGenerator
     * @return true if initialization succeeded, false otherwise
     */
    bool init(const std::string& defId, ItemId id);
    
    /**
     * Allocates and initializes a new ItemInstance.
     * Returns nullptr if initialization fails.
     *
     * @param defId     The definition ID linking this instance to its ItemDef
     * @param id        The unique instance ID assigned by the host's IdGenerator
     * @return a shared pointer to the new ItemInstance, or nullptr on failure
     */
    static std::shared_ptr<ItemInstance> alloc(const std::string& defId, ItemId id) {
        auto result = std::make_shared<ItemInstance>();
        return (result->init(defId, id) ? result : nullptr);
    }
    
    // Getters
    ItemId getId() const { return _id; }
    const std::string& getDefId() const { return _defId; }
    
    /**
     * Serializes this ItemInstance to a JSON object.
     * Stores the instance ID as a double (for JSON compatibility) and the defId as a string.
     *
     * @return a JsonValue object containing the id and defId fields
     */
    std::shared_ptr<cugl::JsonValue> toJson() const;
    
    /**
     * Deserializes this ItemInstance from a JSON object.
     * Expects fields "id" (number) and "defId" (string).
     * Returns false if either field is missing, malformed, or invalid (id == 0 or defId empty).
     *
     * @param json      The JSON object to read from
     * @return true if deserialization succeeded, false otherwise
     */
    bool fromJson(const std::shared_ptr<cugl::JsonValue>& json);
};

#endif // __ITEM_INSTANCE_H__

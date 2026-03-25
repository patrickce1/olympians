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
    /**
     * Enum tracking the origin of a sliding item's motion
     */
    enum class SlideOriginType : std::uint8_t {
        SLIDE_FROM_SPAWN = 0,   // Item sliding up from spawn point
        SLIDE_FROM_DROP = 1,    // Item sliding after being dropped by player
        SLIDE_FROM_PASS = 2     // Item sliding during pass between players
    };
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
    
    // Sliding state tracking
    bool _isSliding = false;                                    // Whether item is currently sliding/animating
    bool _canInteractWithZones = false;                         // Whether item can trigger drop zones
    bool _isBeingPassed = false;                                // Whether item is in transit during a pass
    cugl::Vec2 _slideVelocity{0.0f, 0.0f};                      // Current velocity during slide animation
    float _slideAnimationTimer = 0.0f;                          // Countdown timer for slide/settlement
    float _slideSettleTime = 2.0f;                              // Configurable settlement duration (seconds)
    SlideOriginType _slideOrigin = SlideOriginType::SLIDE_FROM_SPAWN; // Where the slide came from
    bool _zoneHitDuringSlide = false;                           // Tracks if item hit zone during dropped slide
    
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
    
    // Sliding state getters
    /**
     * Returns whether this item is currently sliding or animating.
     *
     * @return true if item is sliding, false otherwise
     */
    bool isSliding() const { return _isSliding; }
    
    /**
     * Returns whether this item can trigger drop zones.
     * Dropped items can interact immediately; spawned/passed items only after settling.
     *
     * @return true if item can interact with zones, false otherwise
     */
    bool canInteractWithZones() const { return _canInteractWithZones; }
    
    /**
     * Returns whether this item is currently being passed to another player.
     * Items marked as being passed cannot interact with zones until settled.
     *
     * @return true if item is in transit during a pass, false otherwise
     */
    bool isBeingPassed() const { return _isBeingPassed; }
    
    /**
     * Returns the current velocity of this item during sliding animation.
     *
     * @return a Vec2 representing velocity (units/sec)
     */
    const cugl::Vec2& getSlideVelocity() const { return _slideVelocity; }
    
    /**
     * Returns the countdown timer for this item's slide/settlement animation.
     *
     * @return time remaining in slide animation (seconds)
     */
    float getSlideAnimationTimer() const { return _slideAnimationTimer; }
    
    /**
     * Returns the origin type of this item's slide motion.
     *
     * @return the SlideOriginType indicating where the slide came from
     */
    SlideOriginType getSlideOrigin() const { return _slideOrigin; }
    
    // Sliding state setters
    /**
     * Sets whether this item is currently sliding or animating.
     *
     * @param isSliding true to mark item as sliding, false to mark as settled
     */
    void setSliding(bool isSliding) { _isSliding = isSliding; }
    
    /**
     * Sets whether this item can trigger drop zones.
     *
     * @param canInteract true to enable zone interaction, false to disable
     */
    void setCanInteractWithZones(bool canInteract) { _canInteractWithZones = canInteract; }
    
    /**
     * Sets whether this item is currently being passed to another player.
     *
     * @param isBeingPassed true to mark item as in transit, false once settled
     */
    void setIsBeingPassed(bool isBeingPassed) { _isBeingPassed = isBeingPassed; }
    
    /**
     * Sets the velocity for this item's slide animation.
     *
     * @param velocity a Vec2 representing velocity (units/sec)
     */
    void setSlideVelocity(const cugl::Vec2& velocity) { _slideVelocity = velocity; }
    
    /**
     * Sets the countdown timer for this item's slide/settlement animation.
     *
     * @param timer time remaining in slide animation (seconds)
     */
    void setSlideAnimationTimer(float timer) { _slideAnimationTimer = timer; }
    
    /**
     * Sets the expected settlement duration for this item's slide.
     *
     * @param settleTime how long until item is fully settled (seconds)
     */
    void setSlideSettleTime(float settleTime) { _slideSettleTime = settleTime; }
    
    /**
     * Sets the origin type of this item's slide motion.
     *
     * @param origin the SlideOriginType indicating where the slide came from
     */
    void setSlideOrigin(SlideOriginType origin) { _slideOrigin = origin; }
    
    /**
     * Public accessor to zone-hit tracking flag (used by GameScene zone detection).
     * Set to true when item hits an appropriate zone during dropped-item sliding.
     */
    bool _zoneHitDuringSlide = false;
    
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

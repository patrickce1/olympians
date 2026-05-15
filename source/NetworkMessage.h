#include <algorithm>
#include <array>
#include <cugl/cugl.h>

#ifndef __NETWORK_MESSAGES_H__
#define __NETWORK_MESSAGES_H__
/*
   Certain messages over the network consist of compound data, with multiple pieces of data needed to fully get across
   what changes need to be done by the user.

   These message types are used by classes like GameState to have a convinient way to recieve network updates 
   and change themselves accordingly
*/

/*Below are the message types sent through the lobby*/

/*A message sent by the client to indicate they want to join the lobby and what their custom username is*/
struct JoinMessage {
    std::string playerName;
};

/*Below are the message types that are sent when the game is active*/

/*Message sent by the client to the host to indicate how much damage they did to the boss and from what direction*/
struct AttackMessage {
    float damage;
    int damageDirection;
    std::string itemDefID;
};

/** Message send by the client to the host to indicate how much they healed the Boss for */
struct BossHealMessage {
    float healAmount;
};

/* Message sent by the client to the host to indicate healing.
* The heal float value is how much health the target was healed by
* The playerID is the order of the player in the circle to whom the heal is being applied to
* All healing data is sent to the host
*/
struct HealMessage {
    float heal;
    int playerID;
};

/** Support effect categories sent from clients to the host. */
enum class SupportEffectType : int32_t {
    Heal = 0,
    Shield = 1,
    Barrier = 2,
    Regen = 3,
    Resurrect = 4,
    Educate = 5,
    Forge = 6,
    Charm = 7,
    Frenzy = 8,
    Lifesteal = 9
};

/** Attack effect categories sent from clients to the host. */
enum class EnemyEffectType : int32_t {
    /** Freezes enemy timer progression for a duration without changing state. */
    Stun = 0,
    /** Forces the enemy idle for a duration. */
    Love = 1,
    /** Scales how quickly enemy state time advances for a duration. */
    Slow = 2,
    /** Increases incoming damage to the enemy for a duration. */
    Vulnerable = 3
};

/** Message sent by the client to indicate a support effect applied to a player.
 * The playerID is the order of the player in the circle to whom the effect is applied.
 * The effectType identifies whether this is a heal, shield, or barrier effect.
 * The magnitude carries the resolved value for the effect (heal amount, shield mitigation,
 * or barrier multiplier), while duration is used by timed effects and is 0 for instant heals.
 */
struct SupportEffectMessage {
    int playerID;
    SupportEffectType effectType;
    float magnitude;
    float duration;
    float secondaryMagnitude = 0.0f;
    bool applyToAllPlayers = false;
};

/** Message sent to request or apply a party-wide forge inventory transformation. */
struct ForgeEffectMessage {
    /** Chance in [0, 1] that each rare item upgrades to divine. */
    float divineChance = 0.0f;
    /** Host-authoritative deterministic seed for local forge rolls. */
    int seed = 0;
    /** True when this message came from the host and should be applied locally. */
    bool authoritative = false;
};

struct EnemyEffectMessage {
    /** The category of enemy effect to apply. */
    EnemyEffectType effectType;
    /** The resolved effect magnitude; for stun this is the delayed damage amount. */
    float magnitude;
    /** The number of seconds the enemy effect should last. */
    float duration;
    /** Seconds after receipt before the enemy effect should take effect. */
    float delay = 0.0f;
    /** The attacking player's slot, used for side-relative enemy effects. */
    int playerIndex = 0;
    /** Whether the effect should be applied to all four boss sides instead of one side. */
    bool applyToAllSides = false;
};

/**
 * Message broadcast by the host when a Cerberus corrosive drain tick fires.
 * Received by all devices; only the target player's device will have widgets
 * to animate. Item selection is done locally on each device (instance IDs are
 * not shared across the network, so they cannot be sent).
 */
struct CorrosiveDrainMessage {
    int targetPlayerSlot = -1;
    float fadeDuration   = 0.9f;
    float fadeVariance   = 0.3f;
    int maxAffected      = 0;
};

/** Message sent by client to indicate passing an item.
 * The itemID is the ID of a item definition type, which are used in the itemDatabase
 * The playerID is the location of the player in the circle, with 0 being the host
 * If the item is passed to an AI, it is sent to the host to update
 * If the item is passed to a real player, the passing message is sent to that player for them to handle themselves
 */
struct PassMessage {
    std::string itemID;
    int playerID;      // Receiver's player ID
    int passDirection; // Direction: 1=left, 2=right
};

/** Runtime support-effect snapshot for a single player in a `GameStateMessage`.
 * The values are serialized in slot order as shield mitigation, shield duration,
 * barrier multiplier, and barrier duration.
 * Shield mitigation stores the remaining flat damage absorption for an active shield.
 * Barrier multiplier stores the active damage multiplier for a barrier effect, where
 * `1.0f` is the neutral value when no barrier is active.
 */
struct PlayerRuntimeEffectState {
    float shieldMitigation;
    float shieldDuration;
    float barrierMultiplier;
    float barrierDuration;
    float regenAmountRemaining;
    float regenDuration;
    float educateDuration;
    float charmDuration;
    float lifestealMultiplier;
    float lifestealDuration;
    bool hasLeftVine;
    bool hasRightVine;
};

/** Message sent by the host to other players about the current state of the game
 * GameState has a function to update itself according to the information in this message type
 */
struct GameStateMessage {
    /** The max number of active players in a game. */
    static constexpr int kMaxPlayers = 4;

    // boss health
    float bossHealth;
    
    // who the boss is facing
    int bossTarget;
    
    // which phase the boss is in
    // check EnemyLoader.h to see what each number corresponds to
    int bossState;
    
    // how long the boss has been in this phase for
    float stateTime;
    
    // We might need to send side multiplier data over network
    // based on how we decide to indicate it
    // but that is for UI people to add to ts
    /** Remaining authoritative stun time for the boss, in seconds. */
    float bossStunDuration = 0.0f;
    
    /** Remaining authoritative love time for the boss, in seconds. */
    float bossLoveDuration = 0.0f;

    /** Remaining authoritative slow time for the boss, in seconds. */
    float bossSlowDuration = 0.0f;

    /** Active authoritative state-time multiplier while slow is active. */
    float bossSlowMultiplier = 1.0f;

    /** Remaining authoritative frenzy time for item spawning, in seconds. */
    float frenzyDuration = 0.0f;

    /** Active authoritative item spawn interval while frenzy is active. */
    float frenzyItemInterval = 0.0f;
    
    /** Remaining authoritative vulnerable time for each relative boss side, in seconds. */
    std::array<float, kMaxPlayers> bossVulnerableDurations = {0.0f, 0.0f, 0.0f, 0.0f};
    
    /** Active authoritative vulnerable multiplier for each relative boss side. */
    std::array<float, kMaxPlayers> bossVulnerableMultipliers = {1.0f, 1.0f, 1.0f, 1.0f};

    /** Authoritative number of prior mallet uses recorded for each player this round. */
    std::array<int32_t, kMaxPlayers> playerMalletUseCounts = {0, 0, 0, 0};

    /** Cerberus head knocked state (indices 0=main, 1=right, 2=left). Zero for non-Cerberus bosses. */
    std::array<bool,  3> cerberusHeadsKnocked      = {false, false, false};
    std::array<float, 3> cerberusHeadsKnockedTimer  = {0.0f,  0.0f,  0.0f};

    /** Player slot locked as the target for the current single-head Cerberus attack, or -1. */
    int cerberusLockedVictim = -1;

    // player health
    union {
        struct {
            float player1HP;
            float player2HP;
            float player3HP;
            float player4HP;
        };
        float playerHP[kMaxPlayers];
    };

    // player buffs/debuff metadata
    union {
        struct {
            float player1ShieldMitigation;
            float player1ShieldDuration;
            float player1BarrierMultiplier;
            float player1BarrierDuration;
            float player1RegenAmountRemaining;
            float player1RegenDuration;
            float player1EducateDuration;
            float player1CharmDuration;
            float player1LifestealMultiplier;
            float player1LifestealDuration;
            bool player1HasVineLeft;
            bool player1HasVineRight;
            float player2ShieldMitigation;
            float player2ShieldDuration;
            float player2BarrierMultiplier;
            float player2BarrierDuration;
            float player2RegenAmountRemaining;
            float player2RegenDuration;
            float player2EducateDuration;
            float player2CharmDuration;
            float player2LifestealMultiplier;
            float player2LifestealDuration;
            bool player2HasVineLeft;
            bool player2HasVineRight;
            float player3ShieldMitigation;
            float player3ShieldDuration;
            float player3BarrierMultiplier;
            float player3BarrierDuration;
            float player3RegenAmountRemaining;
            float player3RegenDuration;
            float player3EducateDuration;
            float player3CharmDuration;
            float player3LifestealMultiplier;
            float player3LifestealDuration;
            bool player3HasVineLeft;
            bool player3HasVineRight;
            float player4ShieldMitigation;
            float player4ShieldDuration;
            float player4BarrierMultiplier;
            float player4BarrierDuration;
            float player4RegenAmountRemaining;
            float player4RegenDuration;
            float player4EducateDuration;
            float player4CharmDuration;
            float player4LifestealMultiplier;
            float player4LifestealDuration;
            bool player4HasVineLeft;
            bool player4HasVineRight;
        };
        PlayerRuntimeEffectState playerRuntimeEffects[kMaxPlayers];
    };

    /** Message struct for GameState */
    GameStateMessage() : bossHealth(0.0f), bossTarget(0), bossState(0), stateTime(0.0f) {
        std::fill_n(playerHP, kMaxPlayers, 0.0f);
        for (int ii = 0; ii < kMaxPlayers; ++ii) {
            playerRuntimeEffects[ii] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        }
    }

    //future info like boss direction will be added as the game expands
};

/*
* Represents a player's selected house sent to the host for updates.
*/
struct SetHouseMessage {
    std::string houseID;
};

/**
 * Message sent by the host to swap two players' game slots.
 * slotA and slotB are 0-based indices into the player array.
 * Broadcast to all clients; clients update their local lobby state
 * via the LOBBY_UPDATE that the host sends immediately after.
 */
struct SwapSlotsMessage {
    int slotA;
    int slotB;
};

/*
* Represents a player as seen over the network.
* Carries information used for identifying the player.
* Stored in the NetworkController and used to map network IDs to in-game players.
*/
struct NetworkedPlayer {
    std::string networkID;
    std::string username;
    std::string houseID;
    //will be expanded to carry things such as player class
};

#endif /* __NETWORK_MESSAGES_H__ */

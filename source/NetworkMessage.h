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

/*Message sent by the client to the host to indicate how much damage they did to the boss*/
struct AttackMessage {
    float damage;
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
    Barrier = 2
};

/** Attack effect categories sent from clients to the host. */
enum class EnemyEffectType : int32_t {
    Stun = 0
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
};

/** Message sent by the client to indicate an enemy-affecting item effect. */
struct EnemyEffectMessage {
    /** The category of enemy effect to apply. */
    EnemyEffectType effectType;
    /** The resolved item magnitude associated with the attack. */
    float magnitude;
    /** The number of seconds the enemy effect should last. */
    float duration;
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

/* Message sent by the host to other players about the current state of the game
* GameState has a function to update itself according to the information in this message type
*/
struct GameStateMessage {
    //boss health
    float bossHealth;
    /** Remaining authoritative stun time for the boss, in seconds. */
    float bossStunDuration;

    //player health
    float player1HP;
    float player2HP;
    float player3HP;
    float player4HP;

    //future info like boss direction will be added as the game expands
};

/*
* Represents a player's selected house sent to the host for updates.
*/
struct SetHouseMessage {
    std::string houseID;
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

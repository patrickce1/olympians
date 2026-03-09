#include <cugl/cugl.h>

#ifndef __NETWORK_MESSAGES_H__
#define __NETWORK_MESSAGES_H__
/*
   BELOW IS THE SPECIFICATION FOR HOW DATA IS ORGANIZED BASED ON MESSAGE TYPE
   BOSS_DAMAGE
       This type of message includes data about what damage was dealt to the boss.
       The data payload for this is just an integer representing how much damage was done to the boss.
   PLAYER_HEAL
       This type of message includes data about what player was healed and for how much health
   PLAYER_PASS
       This type of message includes data about what player was sent
   GAME_UPDATE
       This is the biggest type of message sent over the network. The host sends out this message to tell players of the new world state.
       How this payload
   */
   /*
   These are the types of messages we send/recieve when in the lobby
   */
struct JoinMessage {
    std::string playerName;
};

/*
Types of messages we send/recieve when fighting the boss
*/
struct AttackMessage {
    float damage;
};

struct HealMessage {
    float heal;
    int playerID;
};

struct PassMessage {
    std::string itemID;
    int playerID;
};

struct GameStateMessage {
    //boss health
    float bossHealth;

    //player health
    float player1HP;
    float player2HP;
    float player3HP;
    float player4HP;

    //future info will be added as the game expands
};

#endif /* __NETWORK_MESSAGES_H__ */
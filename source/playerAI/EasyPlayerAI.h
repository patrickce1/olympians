#ifndef __EASY_PLAYER_AI_H__
#define __EASY_PLAYER_AI_H__

#include "PlayerAI.h"

/**
 * Easy difficulty AI controller.
 *
 * EasyPlayerAI extends PlayerAI with simple weighted-random decision making.
 * On each think cycle, it randomly chooses between attacking and supporting
 * based on aggressionWeight and supportWeight. If neither is viable, it passes.
 *
 * Item selection is random within the appropriate type — attack items are
 * selected randomly for attacking, support items randomly for healing.
 *
 * Support and pass actions target only the left or right neighbor of the
 * controlled player, as set by Player::setLeftPlayer / setRightPlayer.
 *
 * EasyPlayerAI reads its config from the "easyPlayerAI" sub-object of the
 * shared playerAI.json file.
 */
class EasyPlayerAI : public PlayerAI {
public:
    /**
     * Constructs an EasyPlayerAI, forwarding all arguments to the Player base constructor.
     * Required because Player has no default constructor.
     *
     * @param characterId   The character ID as it appears in characters.json
     * @param playerNumber  The assigned player slot number
     * @param playerName    Display name for this player
     * @param loader        The CharacterLoader used to populate stats
     */
    EasyPlayerAI(const std::string& characterId,
                 int playerNumber,
                 const std::string& playerName,
                 const CharacterLoader& loader)
        : PlayerAI(characterId, playerNumber, playerName, loader) {}

    ~EasyPlayerAI() = default;

    /**
     * Initializes the EasyPlayerAI by reading the "easyPlayerAI" block
     * from the shared playerAI.json file.
     *
     * @param db        Read-only reference to the item database for item lookups.
     * @param path      Path to the shared playerAI.json config file.
     * @return true if initialization succeeded, false otherwise.
     */
    bool init(const ItemDatabase& db, const std::string& path) override;

protected:

    /**
     * Evaluates game context using weighted random selection between attack and support.
     * Uses the player's left and right neighbors to determine support viability.
     * Falls back to PASS if neither is viable, or IDLE if inventory is empty.
     *
     * @param enemy     The current enemy, used to confirm an attack target exists.
     * @return the State the AI should transition to.
     */
    EasyPlayerAI::State evaluate(const Enemy& enemy) override;

    /**
     * Returns whether the AI has at least one attack item in its inventory.
     */
    bool canAttack() const;

    /**
     * Returns whether the AI has a support item AND at least one neighbor
     * (left or right) is alive and below _healThreshold.
     */
    bool canSupport() const;

    /**
     * Selects a random attack item from inventory and uses it on the enemy.
     * Does nothing if no attack item is found.
     *
     * @param enemy     The enemy to attack.
     * @param items     The ItemController used to resolve the item action.
     */
    void actAttack(Enemy& enemy, ItemController& items) override;

    /**
     * Selects a random support item and uses it on the most injured neighbor
     * (left or right) below _healThreshold.
     * Does nothing if no support item or valid neighbor is found.
     *
     * @param items     The ItemController used to resolve the item action.
     */
    void actSupport(ItemController& items) override;

    /**
     * Passes a random item to a random alive neighbor (left or right).
     * Does nothing if inventory is empty or neither neighbor is alive.
     */
    void actPass() override;
};

#endif /* __EASY_PLAYER_AI_H__ */

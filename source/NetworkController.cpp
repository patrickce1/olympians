#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "NetworkController.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace cugl::netcode;
using namespace std;

namespace {
constexpr int kMaxPlayers = GameStateMessage::kMaxPlayers;

/**
 * Reads player health and runtime support-effect values from a game-state payload.
 *
 * The payload is expected to contain `kMaxPlayers` health values first,
 * followed by `kMaxPlayers` groups of shield mitigation, shield duration,
 * barrier multiplier, and barrier duration values.
 *
 * @param deserializer  The deserializer positioned at the first player-health
 *                      field within a `GAME_UPDATE` payload.
 * @param stateMsg      The game-state message receiving the decoded player
 *                      runtime state.
 */
void readPlayerRuntimeState(NetcodeDeserializer& deserializer, GameStateMessage& stateMsg) {
    for (int ii = 0; ii < kMaxPlayers; ++ii) {
        stateMsg.playerHP[ii] = deserializer.readFloat();
    }

    for (int ii = 0; ii < kMaxPlayers; ++ii) {
        PlayerRuntimeEffectState& effectState = stateMsg.playerRuntimeEffects[ii];
        effectState.shieldMitigation = deserializer.readFloat();
        effectState.shieldDuration = deserializer.readFloat();
        effectState.barrierMultiplier = deserializer.readFloat();
        effectState.barrierDuration = deserializer.readFloat();
        effectState.regenAmountRemaining = deserializer.readFloat();
        effectState.regenDuration = deserializer.readFloat();
        effectState.educateDuration = deserializer.readFloat();
        effectState.charmDuration = deserializer.readFloat();
    }

    for (int ii = 0; ii < kMaxPlayers; ++ii) {
        stateMsg.playerMalletUseCounts[ii] = deserializer.readSint32();
    }
}

/**
 * Writes player health and runtime support-effect values into a game-state payload.
 *
 * The serializer always emits exactly `kMaxPlayers` player slots in slot order.
 * Missing slots are written with default values so the snapshot stays fixed-width.
 *
 * @param serializer  The serializer to append player runtime state to.
 * @param players     The authoritative players whose health, shield, and barrier
 *                    values should be written into the outgoing snapshot.
 */
void writePlayerRuntimeState(NetcodeSerializer& serializer, const vector<shared_ptr<Player>>& players) {
    for (int ii = 0; ii < kMaxPlayers; ++ii) {
        const float health = ii < players.size() ? players[ii]->getCurrentHealth() : 0.0f;
        serializer.writeFloat(health);
    }

    for (int ii = 0; ii < kMaxPlayers; ++ii) {
        if (ii < players.size()) {
            const auto& player = players[ii];
            serializer.writeFloat(player->getShieldHealth());
            serializer.writeFloat(player->getShieldDuration());
            serializer.writeFloat(player->getBarrierMultiplier());
            serializer.writeFloat(player->getBarrierDuration());
            serializer.writeFloat(player->getRegenAmountRemaining());
            serializer.writeFloat(player->getRegenDuration());
            serializer.writeFloat(player->getEducateDuration());
            serializer.writeFloat(player->getCharmDuration());
        } else {
            serializer.writeFloat(0.0f);
            serializer.writeFloat(0.0f);
            serializer.writeFloat(1.0f);
            serializer.writeFloat(0.0f);
            serializer.writeFloat(0.0f);
            serializer.writeFloat(0.0f);
            serializer.writeFloat(0.0f);
            serializer.writeFloat(0.0f);
        }
    }

    for (int ii = 0; ii < kMaxPlayers; ++ii) {
        const int malletUseCount = ii < players.size() ? players[ii]->getMalletUseCount() : 0;
        serializer.writeSint32(malletUseCount);
    }
}

/**
 * Reads authoritative enemy runtime-effect values from a game-state payload.
 *
 * The payload contains the remaining stun duration, love duration,
 * then one vulnerable duration and multiplier pair for each relative boss side.
 *
 * @param deserializer  The deserializer positioned at the first enemy-effect
 *                      field within a `GAME_UPDATE` payload.
 * @param stateMsg      The game-state message receiving the decoded enemy
 *                      runtime state.
 */
void readEnemyRuntimeState(NetcodeDeserializer& deserializer, GameStateMessage& stateMsg) {
    stateMsg.bossStunDuration = deserializer.readFloat();
    stateMsg.bossLoveDuration = deserializer.readFloat();
    stateMsg.bossSlowDuration = deserializer.readFloat();
    stateMsg.bossSlowMultiplier = deserializer.readFloat();
    for (int side = 0; side < Enemy::NUM_PLAYERS; side++) {
        stateMsg.bossVulnerableDurations[side] = deserializer.readFloat();
        stateMsg.bossVulnerableMultipliers[side] = deserializer.readFloat();
    }
}

/**
 * Writes authoritative enemy runtime-effect values into a game-state payload.
 *
 * The payload contains the remaining stun duration, love duration,
 * then one vulnerable duration and multiplier pair for each relative boss side.
 *
 * @param serializer  The serializer to append enemy runtime state to.
 * @param enemy       The authoritative enemy whose runtime effect values should
 *                    be written into the outgoing snapshot.
 */
void writeEnemyRuntimeState(NetcodeSerializer& serializer, const shared_ptr<Enemy>& enemy) {
    serializer.writeFloat(enemy->getStunDuration());
    serializer.writeFloat(enemy->getLoveDuration());
    serializer.writeFloat(enemy->getSlowDuration());
    serializer.writeFloat(enemy->getSlowMultiplier());
    for (int side = 0; side < Enemy::NUM_PLAYERS; side++) {
        serializer.writeFloat(enemy->getVulnerableDurationForSide(side));
        serializer.writeFloat(enemy->getVulnerableMultiplierForSide(side));
    }
}

/**
 * Reads one enemy-effect message payload from the current deserializer position.
 *
 * The payload contains the enemy effect type followed by the resolved magnitude,
 * the timed duration for that effect, and the attacking player's index.
 *
 * @param deserializer  The deserializer positioned at the enemy-effect payload.
 * @return the decoded enemy-effect message.
 */
EnemyEffectMessage readEnemyEffectMessage(NetcodeDeserializer& deserializer) {
    EnemyEffectMessage effectMsg;
    effectMsg.effectType = static_cast<EnemyEffectType>(deserializer.readSint32());
    effectMsg.magnitude = deserializer.readFloat();
    effectMsg.duration = deserializer.readFloat();
    effectMsg.playerIndex = deserializer.readSint32();
    effectMsg.applyToAllSides = deserializer.readBool();
    return effectMsg;
}

/**
 * Writes one enemy-effect message payload to the current serializer position.
 *
 * The payload contains the enemy effect type followed by the resolved magnitude,
 * the timed duration for that effect, and the attacking player's index.
 *
 * @param serializer  The serializer receiving the enemy-effect payload.
 * @param effectMsg   The enemy-effect message to serialize.
 */
void writeEnemyEffectMessage(NetcodeSerializer& serializer, const EnemyEffectMessage& effectMsg) {
    serializer.writeSint32(static_cast<int>(effectMsg.effectType));
    serializer.writeFloat(effectMsg.magnitude);
    serializer.writeFloat(effectMsg.duration);
    serializer.writeSint32(effectMsg.playerIndex);
    serializer.writeBool(effectMsg.applyToAllSides);
}
} // namespace

/*HELPERS*/

/**
 * Converts a decimal string to a hexadecimal string
 *
 * This function assumes that the string is a decimal number less
 * than 65535.
 *
 * @param dec the decimal string to convert
 *
 * @return the hexadecimal equivalent to dec
 */
static std::string dec2hex(const std::string dec) {
	Uint32 value = strtool::stou32(dec);
	if (value >= 65535) {
		value = 0;
	}
	return strtool::to_hexstring(value, 4);
}

/**
 * Converts a hexadecimal string to a decimal string
 *
 * This function assumes that the string is 4 hexadecimal characters
 * or less, and therefore it converts to a decimal string of five
 * characters or less (as is the case with the lobby server). We
 * pad the decimal string with leading 0s to bring it to 5 characters
 * exactly.
 *
 * @param hex the hexadecimal string to convert
 *
 * @return the decimal equivalent to hex
 */
static std::string hex2dec(const std::string hex) {
	Uint32 value = strtool::stou32(hex, 0, 16);
	std::string result = strtool::to_string(value);
	if (result.size() < 5) {
		size_t diff = 5 - result.size();
		std::string alt(5, '0');
		for (size_t ii = 0; ii < result.size(); ii++) {
			alt[diff + ii] = result[ii];
		}
		result = alt;
	}
	return result;
}

/**
* Initializes the NetworkController with the server configuration from assets.
* Must be called once after assets have finished loading, before any
* network calls are made.
*
* @param assets    The loaded asset manager.
* @return          true if initialization succeeded, false if assets is null
*                  or the server config could not be loaded.
*/
bool NetworkController::init(const std::shared_ptr<cugl::AssetManager>& assets) {
	if (assets == nullptr) {
		return false;
	}
	auto json = assets->get<JsonValue>("server");
	_config.set(json);
	_serializer = NetcodeSerializer();
	_deserializer = NetcodeDeserializer();
	_playerName = "";
	_gameWon = false;
	_gameLost = false;
	return true;
}

/**
 * Connects to an existing room as a client.
 * Converts the 5-digit decimal room code to the hex format expected
 * by the netcode layer before opening the connection.
 *
 * @param room  The 5-digit decimal room code displayed in the lobby.
 */
void NetworkController::joinRoom(const std::string room) {
	_network = NetcodeConnection::alloc(_config, dec2hex(room));
	_network->open();
    registerDisconnectCallback();
}

/**
 * Creates and opens a new room as the host.
 * The room code can be retrieved via getRoom() once the connection
 * state reaches CONNECTED.
 */
void NetworkController::hostRoom() {
	_network = NetcodeConnection::alloc(_config);
    _network->open();
    registerDisconnectCallback();
}

/**
 * Returns the current room code as a 5-digit decimal string.
 * Returns "nullstr" if no network connection exists.
 *
 * @return  The room code, or "nullstr" if not connected.
 */
std::string NetworkController::getRoom() {
	if (!_network) { return "nullstr"; }
	return hex2dec(_network->getRoom());
}

/**
 * Closes the network connection and releases the connection object.
 * Should be called when leaving a lobby or game session.
 */
void NetworkController::disconnect() {
    _network->close();
    _network = nullptr;
    _uuidToSlot.clear();
    _slotToPlayer.clear();
    _gameWon = false;
    _gameLost = false;
    _sessionTerminated = false;
    _disconnectedSlots.clear();
    _enemy = "";
    _aIHouses.clear();
    _hostsCurrentScene = -1;
}

/**
 * Releases the network connection without explicitly closing it.
 * Prefer disconnect() for intentional disconnects. This is used
 * during shutdown to release the pointer cleanly.
 */
void NetworkController::dispose() {
	_network = nullptr;
}

/**
 * Returns whether this instance is the host of the current session.
 * Returns false if no network connection exists.
 *
 * @return  true if this client is the host, false otherwise.
 */
bool NetworkController::isHost() {
	if (!_network) { return false; }
	return _network->isHost();
}

/**
 * Returns the current connection status as a simplified Status enum.
 * Maps the netcode layer's connection states to WAITING, CONNECTED, or FAILED.
 * Closes the connection automatically on any failure state.
 *
 * @return  The current connection status.
 */
NetworkController::Status NetworkController::checkConnection() {
	if (!_network) { return FAILED; }

	switch (_network->getState()) {
		case NetcodeConnection::State::NEGOTIATING:
			return Status::WAITING;
			break;
		case NetcodeConnection::State::CONNECTED:
			return Status::CONNECTED;
			break;
		case NetcodeConnection::State::DENIED:
		case NetcodeConnection::State::INVALID:
		case NetcodeConnection::State::MISMATCHED:
		case NetcodeConnection::State::FAILED:
		case NetcodeConnection::State::DISCONNECTED:
			_network->close();
			return Status::FAILED;
			break;
	}
	return FAILED;
}

/**
 * Deserializes and routes an incoming network message to the appropriate queue.
 *
 * Reads the message type code first, then deserializes the remaining fields
 * according to that type. Results are pushed into the corresponding message
 * queues (attacks, heals, passes, etc.) to be consumed during the next
 * game update. Called internally by getNetworkUpdates().
 *
 * @param senderID  The network UUID of the sender.
 * @param message   The raw byte payload to deserialize.
 */
void NetworkController::handleMessage(const std::string& senderID, const std::vector<std::byte>& message) {
	_deserializer.receive(message);
	int msgCode = _deserializer.readSint32();
	switch (msgCode) {
		case MessageType::BOSS_DAMAGE: {
			float damage = _deserializer.readFloat();
			int playerIndex = _deserializer.readSint32();
			AttackMessage attackMsg;
			attackMsg.damage = damage;
			attackMsg.damageDirection = playerIndex;
			attackMsg.itemDefID = _deserializer.readString();
				attacks.push_back(attackMsg);
			break;
		}
        case MessageType::PLAYER_HEAL: {
            float heal = _deserializer.readFloat();
            int healRecieverID = _deserializer.readSint32();
			HealMessage healMsg;
			healMsg.heal = heal;
			healMsg.playerID = healRecieverID;
            heals.push_back(healMsg);
            break;
        }
        case MessageType::PLAYER_SUPPORT_EFFECT: {
            SupportEffectMessage effectMsg;
            effectMsg.playerID = _deserializer.readSint32();
            effectMsg.effectType = static_cast<SupportEffectType>(_deserializer.readSint32());
            effectMsg.magnitude = _deserializer.readFloat();
            effectMsg.duration = _deserializer.readFloat();
            effectMsg.secondaryMagnitude = _deserializer.readFloat();
            effectMsg.applyToAllPlayers = _deserializer.readBool();
            supportEffects.push_back(effectMsg);
            break;
        }
        case MessageType::ENEMY_EFFECT: {
            enemyEffects.push_back(readEnemyEffectMessage(_deserializer));
            break;
        }
        case MessageType::FORGE_EFFECT: {
            ForgeEffectMessage forgeMsg;
            forgeMsg.divineChance = _deserializer.readFloat();
            forgeMsg.seed = _deserializer.readSint32();
            forgeMsg.authoritative = _deserializer.readBool();
            forgeEffects.push_back(forgeMsg);
            break;
        }
        case MessageType::PLAYER_PASS: {
            std::string itemID = _deserializer.readString();
            int passRecieverID = _deserializer.readSint32();
			int passDirection = _deserializer.readSint32();
			PassMessage passMsg;
			passMsg.itemID = itemID;
			passMsg.playerID = passRecieverID;
			passMsg.passDirection = passDirection;
			passes.push_back(passMsg);
			break;
		}
        case MessageType::PLAYER_JOIN: {
            std::string playerName = _deserializer.readString();
            CULog("HOST received join from %s with name %s", senderID.c_str(), playerName.c_str());
            
            // Reject if the host has already started — don't assign a slot so that
            // when the client disconnects it doesn't trigger broadcastPlayerDisconnected
            // and kick everyone out of PreGameEntry or GameScene.
            if (_hostsCurrentScene == 0 || _hostsCurrentScene == 1) {
                break;
            }

            if (_uuidToSlot.find(senderID) == _uuidToSlot.end()) {
                // Find the lowest numbered slot not occupied by a real player.
                int slot = -1;
                for (int i = 0; i < 4; i++) {
                    if (_slotToPlayer.find(i) == _slotToPlayer.end()) {
                        slot = i;
                        break;
                    }
                }
                if (slot == -1) break; // lobby full

                // Remove any AI house assignment for this slot since it's
                // now occupied by a real player.
                _aIHouses.erase(slot);

                _uuidToSlot[senderID] = slot;
                NetworkedPlayer newPlayer;
                newPlayer.networkID = senderID;
                newPlayer.username = playerName;
                _slotToPlayer[slot] = newPlayer;
                broadcastLobbyState();
            }
            break;
        }
        case MessageType::LOBBY_UPDATE: {
            std::vector<std::string> playerData = _deserializer.readStringVector();
            _uuidToSlot.clear();
            _slotToPlayer.clear();
            _aIHouses.clear();

            int i = 0;
            while (i < (int)playerData.size() - 1) {
                std::string type = playerData[i];
                int slot = std::stoi(playerData[i + 1]);

                if (type == "player") {
                    NetworkedPlayer newPlayer;
                    newPlayer.networkID = playerData[i + 2];
                    newPlayer.username  = playerData[i + 3];
                    newPlayer.houseID   = playerData[i + 4];
                    _uuidToSlot[newPlayer.networkID] = slot;
                    _slotToPlayer[slot] = newPlayer;
                    i += 5;
                } else {
                    // AI slot
                    _aIHouses[slot] = playerData[i + 2];
                    i += 3;
                }
            }

            _enemy = playerData.back();
            break;
        }
		case MessageType::GAME_UPDATE : {
			GameStateMessage stateMsg;
            stateMsg.bossHealth = _deserializer.readFloat();
            stateMsg.bossTarget = _deserializer.readSint32();
            stateMsg.bossState = _deserializer.readSint32();
            stateMsg.stateTime = _deserializer.readFloat();
            readEnemyRuntimeState(_deserializer, stateMsg);
            stateMsg.frenzyItemInterval = _deserializer.readFloat();
            stateMsg.frenzyDuration = _deserializer.readFloat();
            readPlayerRuntimeState(_deserializer, stateMsg);
            
			_latestGameState = stateMsg;
			break;
		}
		case MessageType::GAME_WON: {
			_gameWon = true;
			break;
		}
		case MessageType::GAME_LOST: {
			_gameLost = true;
			break;
		}
        case MessageType::SELECT_HOUSE: {
            std::string houseID = _deserializer.readString();
            auto pair = _uuidToSlot.find(senderID);
            if (pair != _uuidToSlot.end()) {
                _slotToPlayer[pair->second].houseID = houseID;
                broadcastLobbyState();
            }
            break;
        }
        case MessageType::PLAYER_DISCONNECT: {
            int slot = _deserializer.readSint32();
            CULog("NetworkController: received PLAYER_DISCONNECT for slot %d", slot);
            _disconnectedSlots.push_back(slot);

            // Remove from maps so updateNetworkOrder() on clients correctly
            // detects the disconnect via the missing slot rather than stale data.
            auto pairToRemove = _slotToPlayer.find(slot);
            if (pairToRemove != _slotToPlayer.end()) {
                _uuidToSlot.erase(pairToRemove->second.networkID);
                _slotToPlayer.erase(pairToRemove);
            }
            break;
        }
        case MessageType::SESSION_TERMINATED: {
            _sessionTerminated = true;
            break;
        }
        case MessageType::BOSS_SELECT: {
            _enemy = _deserializer.readString();
            break;
        }
        case MessageType::AI_HOUSE_SELECT: {
            int slot = _deserializer.readSint32();
            std::string houseID = _deserializer.readString();
            _aIHouses[slot] = houseID;
            break;
        }
        case MessageType::BOSS_HEAL: {
            BossHealMessage msg;
            msg.healAmount = _deserializer.readFloat();
            bossHeals.push_back(msg);
            break;
        }
        case MessageType::GAIA_SPAWN: {
            gaiaSpawns++;
            break;
        }
        case MessageType::HOSTS_CURRENT_SCENE: {
            _hostsCurrentScene = _deserializer.readSint32();
            break;
        }
	}
}

/**
 * Polls the network connection for incoming messages and processes them.
 * Should be called once per frame at the start of the update cycle,
 * before reading from any message queues.
 */
void NetworkController::getNetworkUpdates() {
	if (_network) {
		_network->receive([this](const std::string source,
			const std::vector<std::byte>& data) {
				handleMessage(source, data);
			});
		checkConnection();
	}
}

/**
 * Clears all incoming message queues.
 * Should be called at the end of each update cycle after all queues
 * have been consumed, to prevent messages from being processed twice.
 */
void NetworkController::clearQueues() {
	attacks.clear();
	heals.clear();
	supportEffects.clear();
	enemyEffects.clear();
    forgeEffects.clear();
	passes.clear();
    bossHeals.clear();
    gaiaSpawns = 0;
	_gameWon = false;
	_gameLost = false;
    _hostsCurrentScene = -1;
    _sessionTerminated = false;
    _disconnectedSlots.clear();
}

/**
 * Sends an attack message to the host with the given damage value.
 * Called by non-host clients when the local player attacks the boss.
 *
 * @param damageAmount The locally resolved damage amount to report for this attack.
 * @param playerIndex The attacking player's slot index.
 * @param itemDefID The definition ID of the attack item so the host can recompute authoritative damage.
 */
void NetworkController::broadcastDamage(float damageAmount, int playerIndex, const std::string& itemDefID) {
	_serializer.writeSint32(MessageType::BOSS_DAMAGE);
	_serializer.writeFloat(damageAmount);
	_serializer.writeSint32(playerIndex);
	_serializer.writeString(itemDefID);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}


/**
 * Sends a boss heal message to the host.
 * Called by clients when a Gaia rock item is used, which heals
 * the boss instead of dealing damage.
 *
 * @param healAmount  The amount of health to restore to the boss.
 */
void NetworkController::broadcastBossHeal(float healAmount) {
    _serializer.writeSint32(MessageType::BOSS_HEAL);
    _serializer.writeFloat(healAmount);
    _network->sendToHost(_serializer.serialize());
    _serializer.reset();
}

/**
 * Sends a heal message to the host targeting a specific player.
 * Called by non-host clients when the local player uses a support item.
 *
 * @param heal      The amount of health to restore.
 * @param playerID  The 0-based index of the player to heal.
 */
void NetworkController::broadcastHeal(float heal, int playerID) {
	_serializer.writeSint32(MessageType::PLAYER_HEAL);
	_serializer.writeFloat(heal);
	_serializer.writeSint32(playerID);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

/**
 * Sends a Gaia rock spawn message directly to the target player.
 * Called by the host when Gaia's rock spawn targets a real (non-AI) player,
 * telling that client to add a Gaia rock to their local inventory.
 *
 * @param playerID  The 0-based slot index of the player to receive the rock.
 */
void NetworkController::broadcastGaiaSpawn(int playerID) {
    _serializer.writeSint32(MessageType::GAIA_SPAWN);

    if (checkRealPlayer(playerID)) {
        _network->sendTo(_slotToPlayer.at(playerID).networkID, _serializer.serialize());
    }

    _serializer.reset();
}

/**
 * Sends a support effect application to the host for authoritative processing.
 *
 * @param effectType The kind of support effect that was applied.
 * @param magnitude  The primary resolved magnitude of the effect.
 * @param duration   The timed duration of the effect, or 0 for instant effects.
 * @param playerID   The 0-based index of the player receiving the effect, or -1 for all-player effects.
 * @param secondaryMagnitude Optional secondary magnitude used by multi-stage effects such as resurrect.
 * @param applyToAllPlayers Whether the effect should be applied to every allied player slot instead of one target.
 */
void NetworkController::broadcastSupportEffect(SupportEffectType effectType, float magnitude, float duration, int playerID, float secondaryMagnitude, bool applyToAllPlayers) {
	_serializer.writeSint32(MessageType::PLAYER_SUPPORT_EFFECT);
	_serializer.writeSint32(playerID);
	_serializer.writeSint32(static_cast<int>(effectType));
	_serializer.writeFloat(magnitude);
	_serializer.writeFloat(duration);
	_serializer.writeFloat(secondaryMagnitude);
	_serializer.writeBool(applyToAllPlayers);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

/**
 * Sends an enemy-affecting item effect to the host so the host can apply it once and replicate the result.
 *
 * @param effectType The type of enemy effect being applied.
 * @param magnitude  The resolved magnitude associated with the attack item.
 * @param duration   The timed duration of the enemy effect.
 * @param playerIndex The attacking player's slot.
 * @param applyToAllSides Whether the enemy effect should be applied to all four boss sides.
 */
void NetworkController::broadcastEnemyEffect(EnemyEffectType effectType, float magnitude, float duration, int playerIndex, bool applyToAllSides) {
    EnemyEffectMessage effectMsg;
    effectMsg.effectType = effectType;
    effectMsg.magnitude = magnitude;
    effectMsg.duration = duration;
    effectMsg.playerIndex = playerIndex;
    effectMsg.applyToAllSides = applyToAllSides;

	_serializer.writeSint32(MessageType::ENEMY_EFFECT);
    writeEnemyEffectMessage(_serializer, effectMsg);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

/**
 * Sends a forge request to the host for authoritative seeding.
 *
 * @param chance  Chance in [0, 1] that each rare item upgrades to divine.
 */
void NetworkController::requestForgeEffect(float chance) {
    _serializer.writeSint32(MessageType::FORGE_EFFECT);
    _serializer.writeFloat(chance);
    _serializer.writeSint32(0);
    _serializer.writeBool(false);
    _network->sendToHost(_serializer.serialize());
    _serializer.reset();
}

/**
 * HOST ONLY. Broadcasts an authoritative forge seed to every connected client.
 *
 * @param chance  Chance in [0, 1] that each rare item upgrades to divine.
 * @param seed    Host-generated deterministic seed all clients should use for forge rolls.
 */
void NetworkController::broadcastForgeEffect(float chance, int seed) {
    _serializer.writeSint32(MessageType::FORGE_EFFECT);
    _serializer.writeFloat(chance);
    _serializer.writeSint32(seed);
    _serializer.writeBool(true);
    _network->broadcast(_serializer.serialize());
    _serializer.reset();
}

/**
 * Returns whether a given player index corresponds to a real (human) player.
 * A player is considered real if their index falls within the online players list.
 *
 * @param playerID  The 0-based player index to check.
 * @return          true if the player is a real networked player, false if AI.
 */
bool NetworkController::checkRealPlayer(int playerID) {
    return _slotToPlayer.find(playerID) != _slotToPlayer.end();
}

/**
 * Sends an item pass message to the target player.
 * If the target is a real player, sends directly to their network UUID.
 * If the target is an AI slot, sends to the host to handle locally.
 *
 * @param itemDefID The definition ID of the item being passed.
 * @param playerID  The 0-based index of the player to pass the item to.
 */
void NetworkController::broadcastPass(const std::string& itemDefID, int playerID, int passDirection) {
	_serializer.writeSint32(MessageType::PLAYER_PASS);
	_serializer.writeString(itemDefID);
	_serializer.writeSint32(playerID);
	_serializer.writeSint32(passDirection);
	
	CULog("Sending broadcasting message to player %d", playerID);
    if (checkRealPlayer(playerID)) {
        std::string playerNetworkID = _slotToPlayer.at(playerID).networkID;
        _network->sendTo(playerNetworkID, _serializer.serialize());
    } else {
        _network->sendToHost(_serializer.serialize());
    }
	
	_serializer.reset();
}

/**
 * Sends a join message to the host with this player's username.
 * Should be called once when the client first connects to a lobby.
 * The host will add this player to the online players list and
 * broadcast the updated lobby state to all clients.
 */
void NetworkController::broadcastJoinedLobby() {
	_serializer.writeSint32(MessageType::PLAYER_JOIN);
	_serializer.writeString(_playerName);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

/**
 * Broadcasts the current authoritative game state to all clients.
 * Should be called by the host once per frame after processing all
 * incoming attack and heal messages for that frame.
 *
 * @param state     The current authoritative game state.
 * @param frenzyItemInterval Active frenzy item interval, or 0 when inactive.
 * @param frenzyDuration Remaining frenzy duration in seconds, or 0 when inactive.
 */
void NetworkController::broadcastGameState(const GameState& state, float frenzyItemInterval, float frenzyDuration) {
	_serializer.writeSint32(MessageType::GAME_UPDATE);
	_serializer.writeFloat(state.getEnemy()->getCurrentHealth());
	_serializer.writeSint32(state.getEnemy()->getTargetIndex());
	_serializer.writeSint32(state.getEnemy()->getCurrentState());
	_serializer.writeFloat(state.getEnemy()->getStateTime());
    writeEnemyRuntimeState(_serializer, state.getEnemy());
    _serializer.writeFloat(frenzyItemInterval);
    _serializer.writeFloat(frenzyDuration);
	std::vector<shared_ptr<Player>> players = state.getPlayers();
    writePlayerRuntimeState(_serializer, players);
	_network->broadcast(_serializer.serialize());
	_serializer.reset();
}

/**
* Broacasts to clients if the game was won
*/
void NetworkController::broadcastWonGame() {
	_serializer.writeSint32(MessageType::GAME_WON);
	_network->broadcast(_serializer.serialize());
	_serializer.reset();
}

/**
* Broadcasts to clients if the game was lost
*/
void NetworkController::broadcastLostGame() {
	_serializer.writeSint32(MessageType::GAME_LOST);
	_network->broadcast(_serializer.serialize());
	_serializer.reset();
}

/**
 * Broadcasts the current lobby player & AI list to all connected clients.
 * Called by the host whenever a new player joins so all clients stay in sync.
 * Serializes the online players list as a flat string vector in the format:
 * [networkID_0, username_0, house_0, networkID_1, username_1, house_1, ...]
 */
void NetworkController::broadcastLobbyState() {
    std::vector<std::string> serializablePlayers;

    for (int i = 0; i < 4; i++) {
        auto pair = _slotToPlayer.find(i);
        if (pair != _slotToPlayer.end()) {
            // Real player at this slot
            serializablePlayers.push_back("player");
            serializablePlayers.push_back(std::to_string(i));
            serializablePlayers.push_back(pair->second.networkID);
            serializablePlayers.push_back(pair->second.username);
            serializablePlayers.push_back(pair->second.houseID);
        } else {
            // AI at this slot — look up house from _aIHouses
            auto aIPair = _aIHouses.find(i);
            std::string aiHouse = (aIPair != _aIHouses.end()) ? aIPair->second : "";
            serializablePlayers.push_back("ai");
            serializablePlayers.push_back(std::to_string(i));
            serializablePlayers.push_back(aiHouse);
        }
    }

    serializablePlayers.push_back(_enemy);

    _serializer.writeSint32(MessageType::LOBBY_UPDATE);
    _serializer.writeStringVector(serializablePlayers);
    _network->broadcast(_serializer.serialize());
    _serializer.reset();
}

/**
 * Broadcasts the current selected house by the player to all connected clients.
 * Called by the host whenever a player locks down a house choice so all clients
 * can stay in sync and in can be displayed in the lobby.
 *
 *@param house - the selected house 
 */
void NetworkController::broadcastSelectedHouse(const std::string& house) {
    _serializer.writeSint32(MessageType::SELECT_HOUSE);
    _serializer.writeString(house);
    _network->sendToHost(_serializer.serialize());
    _serializer.reset();
}

/**
 * Sets the local player's display name and registers them in the online
 * players list as the first entry. Should be called once after the player
 * enters their name, before connecting to or hosting a lobby.
 *
 * @param name  The display name to assign to the local player.
 */
void NetworkController::setPlayerName(const std::string& name) {
    _playerName = name;
    if (_uuidToSlot.empty()) {
        int slot = 0;
        std::string uuid = _network->getUUID();
        _uuidToSlot[uuid] = slot;
        NetworkedPlayer newPlayer;
        newPlayer.username = name;
        newPlayer.networkID = uuid;
        _slotToPlayer[slot] = newPlayer;
    }
}

/**
 * Returns the 0-based index of the local player in the online players list.
 * This index corresponds to the player's slot in the game's player array.
 * Returns -1 if the local player is not found in the list.
 *
 * @return  The local player's index, or -1 if not found.
 */
int NetworkController::getLocalPlayerNumber() {
    if (!_network) return -1;
    std::string localID = _network->getUUID();
    auto pair = _uuidToSlot.find(localID);
    return pair != _uuidToSlot.end() ? pair->second : -1;
}

/**
 * Returns the 0-based index of the player in the online players list given their networkID.
 * This index corresponds to the player's slot in the game's player array.
 * Returns -1 if the player is not found in the list.
 *
 * @return  The player's index, or -1 if not found.
 */
int NetworkController::getPlayerNumberByID(const std::string& networkID) {
    auto pair = _uuidToSlot.find(networkID);
    return pair != _uuidToSlot.end() ? pair->second : -1;
}

/**
 * Registers a disconnect callback on the NetcodeConnection so that when
 * any peer closes, their slot is immediately pushed into _disconnectedSlots.
 * Should be called once after the network connection is established.
 */
void NetworkController::registerDisconnectCallback() {
    if (!_network) return;

    _network->onDisconnect([this](const std::string& peerID) {
        auto pair = _uuidToSlot.find(peerID);
        if (pair != _uuidToSlot.end()) {
            int slot = pair->second;
            _disconnectedSlots.push_back(slot);
            _uuidToSlot.erase(pair);
            _slotToPlayer.erase(slot);
            if (isHost()) {
                broadcastPlayerDisconnected(slot);
                broadcastLobbyState();
            }
        }
    });
}

/**
 * Broadcasts a PLAYER_DISCONNECT message to all clients.
 *
 * @param slotIndex  The 0-based player slot that disconnected.
 */
void NetworkController::broadcastPlayerDisconnected(int slotIndex) {
    _serializer.writeSint32(MessageType::PLAYER_DISCONNECT);
    _serializer.writeSint32(slotIndex);
    _network->broadcast(_serializer.serialize());
    _serializer.reset();
}

/**
 * Sets the house selection for the local player (host only).
 *
 * Because sendToHost() does not loop back to the sender, the host cannot
 * receive its own SELECT_HOUSE message via the normal network path. This
 * method writes the house ID directly into the host's slot in the online
 * players list and broadcasts the updated lobby state to all clients so
 * they stay in sync.
 *
 * Should be called on the host immediately after broadcastSelectedHouse()
 * when the host locks in their house selection.
 *
 * @param houseID  The ID of the house the host selected (e.g. "athena").
 *                 Must match a valid entry in the HouseLoader.
 */
void NetworkController::setLocalHouse(const std::string& houseID) {
    std::string uuid = _network->getUUID();
    auto pair = _uuidToSlot.find(uuid);
    if (pair != _uuidToSlot.end()) {
        _slotToPlayer[pair->second].houseID = houseID;
        broadcastLobbyState();
    }
}
/**
 * Returns true if every real player has selected a house AND every AI slot
 * has a house assigned by the host. The start button only activates when
 * this returns true, enforcing that no slot enters the game without a house.
 */
bool NetworkController::allPlayersSelectedHouse() const {
    if (_slotToPlayer.empty()) return false;
    for (const auto& pair : _slotToPlayer) {
        if (pair.second.houseID.empty()) return false;
    }
    int totalSlots = 4;
    for (int i = 0; i < totalSlots; i++) {
        if (_slotToPlayer.find(i) == _slotToPlayer.end()) {
            auto aiHouse = _aIHouses.find(i);
            if (aiHouse == _aIHouses.end() || aiHouse->second.empty()) return false;
        }
    }
    return true;
}

/*HOST ONLY. Notifies all clients that the host has exited the lobby and the session is over.*/
void NetworkController::broadcastSessionTerminated() {
    _serializer.reset();
    _serializer.writeSint32(SESSION_TERMINATED);
    auto msg = _serializer.serialize();
    _network->broadcast(msg);
}

/**
 * Broadcasts the host's selected boss enemy to all connected clients.
 * Should be called by the host immediately after the player confirms
 * their boss selection in the boss select screen.
 *
 * Clients will update their local _enemy field upon receiving this
 * message, which is then read by getEnemy() to update the lobby UI.
 *
 * @param enemyID  The unique identifier of the selected enemy (e.g. "cyclops", "cerberus").
 *                 Must match a valid entry in the enemy JSON definition file.
 */
void NetworkController::broadcastBossSelection(const std::string& enemyID) {
    _serializer.writeSint32(MessageType::BOSS_SELECT);
    _serializer.writeString(enemyID);
    _network->broadcast(_serializer.serialize());
    _serializer.reset();
}

/**
 * Returns true if the given houseID is already claimed by any player
 * other than the local player.
 *
 * @param houseID  The house ID to check.
 * @return         true if another player has claimed it, false otherwise.
 */
bool NetworkController::isHouseTaken(const std::string& houseID) const {
    if (houseID.empty()) return false;
    std::string localID = _network ? _network->getUUID() : "";

    // Check real players (excluding self)
    for (const auto& pair : _slotToPlayer) {
        if (pair.second.networkID == localID) continue;
        if (pair.second.houseID == houseID) return true; // isHouseTaken
    }

    // Check AI slot assignments
    for (const auto& pair : _aIHouses) {
        if (pair.second == houseID) return true;
    }

    return false;
}

/**
 * Returns the set of houseIDs currently claimed by players other than
 * the local player. Used by HouseSelectScene to grey out unavailable cards.
 *
 * @return  A vector of taken house ID strings.
 */
std::vector<std::string> NetworkController::getTakenHouses() const {
    std::string localID = _network ? _network->getUUID() : "";
    std::vector<std::string> taken;

    // Real players (excluding self)
    for (const auto& pair : _slotToPlayer) {
        if (pair.second.networkID == localID) continue;
        if (!pair.second.houseID.empty()) taken.push_back(pair.second.houseID); // getTakenHouses
    }

    // AI slots
    for (const auto& pair : _aIHouses) {
        if (!pair.second.empty()) {
            taken.push_back(pair.second);
        }
    }

    return taken;
}

/**
 * Broadcasts the host's house selection for an AI slot to all clients.
 * Clients will update that slot's houseID in their local _uiudToSlot
 * list upon receiving this message.
 *
 * @param slotIndex  The 0-based AI slot index being configured.
 * @param houseID    The selected house ID, or "" to clear the selection.
 */
void NetworkController::broadcastAIHouseSelection(int slotIndex, const std::string& houseID) {
    _serializer.writeSint32(MessageType::AI_HOUSE_SELECT);
    _serializer.writeSint32(slotIndex);
    _serializer.writeString(houseID);
    _network->broadcast(_serializer.serialize());
    _serializer.reset();

    // Store locally — host doesn't receive its own broadcast
    _aIHouses[slotIndex] = houseID;
}

/** Returns the house ID assigned to the given AI slot, or "" if unset */
std::string NetworkController::getAIHouse(int slotIndex) const {
    auto houseAtAIIndex = _aIHouses.find(slotIndex);
    return houseAtAIIndex != _aIHouses.end() ? houseAtAIIndex->second : "";
}

/**
 * Clears the host's AI house assignment for the given slot.
 * Called when a real player joins a slot that was previously
 * configured as AI, so the assignment does not bleed back
 * after the player leaves.
 *
 * @param slotIndex  The 0-based slot index to clear.
 */
void NetworkController::clearAIHouse(int slotIndex) {
    if (_aIHouses.erase(slotIndex) > 0) {
        // Broadcast so all clients remove this slot from their taken set
        broadcastLobbyState();
    }
}

/**
 * Returns true if the host dropped unexpectedly. Polls the connection state directly each frame,
 * since CUGL's onDisconnect callback is unreliable when receive() is called
 * every frame. CLIENT ONLY — always false on the host.
 */
bool NetworkController::wasHostDisconnected() const {
    if (_network && !_network->isHost()) {
        auto state = _network->getState();
        return state == NetcodeConnection::State::DISCONNECTED
            || state == NetcodeConnection::State::FAILED;
    }
    return false;
}

/**
 * Broadcasts the host's current scene state to all clients.
 * Called every frame by the host so clients can mirror scene transitions
 * even if they missed the original transition signal.
 *
 * @param sceneState  0 = PreGameEntryScene, 1 = GameScene
 */
void NetworkController::broadcastHostsCurrentScene(int sceneState) {
    _serializer.writeSint32(MessageType::HOSTS_CURRENT_SCENE);
    _serializer.writeSint32(sceneState);
    _network->broadcast(_serializer.serialize());
    _serializer.reset();
}

/**
 * HOST ONLY. Swaps the game slots of two players (real or AI) and
 * broadcasts the updated lobby state to all clients.
 *
 * Handles all four cases:
 *   real  <-> real  : swap _slotToPlayer entries + update _uuidToSlot for both
 *   real  <-> AI    : move _slotToPlayer entry, move _aIHouses entry, update _uuidToSlot
 *   AI    <-> real  : symmetric to above
 *   AI    <-> AI    : swap _aIHouses entries only
 *
 * After updating both maps, calls broadcastLobbyState() so every client
 * receives a fresh LOBBY_UPDATE reflecting the new arrangement.
 *
 * @param slotA  First 0-based slot index.
 * @param slotB  Second 0-based slot index.
 */
void NetworkController::swapSlots(int slotA, int slotB) {
    if (slotA == slotB) return;

    bool aIsReal = (_slotToPlayer.find(slotA) != _slotToPlayer.end());
    bool bIsReal = (_slotToPlayer.find(slotB) != _slotToPlayer.end());

    if (aIsReal && bIsReal) {
        // Both real: swap player records and update UUID->slot mapping
        NetworkedPlayer playerA = _slotToPlayer[slotA];
        NetworkedPlayer playerB = _slotToPlayer[slotB];
        _slotToPlayer[slotA] = playerB;
        _slotToPlayer[slotB] = playerA;
        if (!playerA.networkID.empty()) _uuidToSlot[playerA.networkID] = slotB;
        if (!playerB.networkID.empty()) _uuidToSlot[playerB.networkID] = slotA;

    } else if (aIsReal && !bIsReal) {
        // A is real, B is AI
        NetworkedPlayer playerA = _slotToPlayer[slotA];
        std::string aiHouseB = getAIHouse(slotB);

        _slotToPlayer.erase(slotA);
        _slotToPlayer[slotB] = playerA;
        if (!playerA.networkID.empty()) _uuidToSlot[playerA.networkID] = slotB;

        _aIHouses.erase(slotB);
        if (!aiHouseB.empty()) _aIHouses[slotA] = aiHouseB;

    } else if (!aIsReal && bIsReal) {
        // A is AI, B is real — symmetric
        NetworkedPlayer playerB = _slotToPlayer[slotB];
        std::string aiHouseA = getAIHouse(slotA);

        _slotToPlayer.erase(slotB);
        _slotToPlayer[slotA] = playerB;
        if (!playerB.networkID.empty()) _uuidToSlot[playerB.networkID] = slotA;

        _aIHouses.erase(slotA);
        if (!aiHouseA.empty()) _aIHouses[slotB] = aiHouseA;

    } else {
        // Both AI: swap house assignments only
        std::string aiHouseA = getAIHouse(slotA);
        std::string aiHouseB = getAIHouse(slotB);
        _aIHouses.erase(slotA);
        _aIHouses.erase(slotB);
        if (!aiHouseA.empty()) _aIHouses[slotB] = aiHouseA;
        if (!aiHouseB.empty()) _aIHouses[slotA] = aiHouseB;
    }

    broadcastLobbyState();
}

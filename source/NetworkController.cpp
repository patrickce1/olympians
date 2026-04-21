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
        } else {
            serializer.writeFloat(0.0f);
            serializer.writeFloat(0.0f);
            serializer.writeFloat(1.0f);
            serializer.writeFloat(0.0f);
        }
    }
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
    registerPromotionCallback();
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
    registerPromotionCallback();
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
    _gameStarted = false;
    _gameWon = false;
    _gameLost = false;
    _sessionTerminated = false;
    _disconnectedSlots.clear();
    _enemy = "";
    _aIHouses.clear();
    // Reset migration flags so a fresh connection via hostRoom() or joinRoom()
    // starts with no leftover promotion or migrating state from a prior session.
    _pendingDisconnectID = "";
    _migrationQueue.clear();
    _migrating = false;
    _promotedToHost = false;
    _postMigrationCooldown = 0;
    _uuidToSlot.clear();
    _playersInfo.clear();
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
        // The lobby server is currently selecting a new host after an unclean
        // disconnect. CUGL blocks all outgoing messages in this state, so we
        // surface it as Status::MIGRATING so GameScene can pause simulation and
        // avoid calling any broadcastX() methods until migration resolves.
        case NetcodeConnection::State::MIGRATING:
            return Status::MIGRATING;
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
            supportEffects.push_back(effectMsg);
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
		case MessageType::GAME_START: {
			_gameStarted = true;
			break;
		}
        case MessageType::PLAYER_JOIN: {
            std::string playerName = _deserializer.readString();
            if (_uuidToSlot.count(senderID) == 0) {
                // Find the host's slot to use as the starting anchor.
                // New players fill slots clockwise (right) from the host.
                int hostSlot = _uuidToSlot.count(_network->getUUID())
                    ? _uuidToSlot.at(_network->getUUID())
                    : 0;

                std::set<int> usedSlots;
                for (const auto& pair : _uuidToSlot) usedSlots.insert(pair.second);

                // Walk clockwise from (hostSlot + 1), wrapping at kMaxPlayers
                int slot = (hostSlot + 1) % kMaxPlayers;
                while (usedSlots.count(slot)) slot = (slot + 1) % kMaxPlayers;

                _uuidToSlot[senderID] = slot;
                NetworkedPlayer np;
                np.networkID = senderID;
                np.username = playerName;
                _playersInfo[senderID] = np;
                broadcastLobbyState();
            }
            break;
        }
        case MessageType::LOBBY_UPDATE: {
            std::vector<std::string> playerData = _deserializer.readStringVector();
            _uuidToSlot.clear();
            _playersInfo.clear();
            _aIHouses.clear();
            CULog("CLIENT received lobby update with %d entries", (int)playerData.size());

            int i = 0;

            // AI houses at the front
            int aiCount = std::stoi(playerData[i++]);
            for (int j = 0; j < aiCount; j++) {
                int slot = std::stoi(playerData[i]);
                _aIHouses[slot] = playerData[i + 1];
                i += 2;
            }

            // Parse real players with their slot assignments:
            while (i < (int)playerData.size() - 1) {
                std::string uuid  = playerData[i];
                std::string name  = playerData[i + 1];
                std::string house = playerData[i + 2];
                int slot          = std::stoi(playerData[i + 3]);
                _uuidToSlot[uuid] = slot;
                NetworkedPlayer np;
                np.networkID = uuid;
                np.username  = name;
                np.houseID   = house;
                _playersInfo[uuid] = np;
                i += 4;
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
            auto pair = _playersInfo.find(senderID);
            if (pair != _playersInfo.end()) {
                pair->second.houseID = houseID;
                broadcastLobbyState();
            }
            break;
        }
        case MessageType::PLAYER_DISCONNECT: {
            int slot = _deserializer.readSint32();
            CULog("NetworkController: received PLAYER_DISCONNECT for slot %d", slot);
            _disconnectedSlots.push_back(slot);
            for (auto pair = _uuidToSlot.begin(); pair != _uuidToSlot.end(); ++pair) {
                if (pair->second == slot) {
                    _playersInfo.erase(pair->first);
                    _uuidToSlot.erase(pair);
                    break;
                }
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
	}
}

/**
 * Polls the network connection for incoming messages and processes them.
 * Should be called once per frame at the start of the update cycle,
 * before reading from any message queues.
 *
 * Also manages the post-migration cooldown timer. After host promotion is
 * confirmed, we delay all outgoing sends for _postMigrationCooldown frames
 * to give the RTC worker thread time to finish tearing down the old host's
 * dead SCTP peer channel. Attempting to send before that cleanup completes
 * throws errno=32 on the RTC worker thread, which cannot be caught on the
 * main thread and crashes the app. Once the cooldown hits zero, any messages
 * that were queued during the window are flushed through sendOrQueue in order.
 */
void NetworkController::getNetworkUpdates() {
    if (_network) {
        _network->receive([this](const std::string source,
            const std::vector<std::byte>& data) {
                handleMessage(source, data);
            });
        checkConnection();

        // Tick the post-migration cooldown. Sends are queued into
        // _migrationQueue while this is > 0 (see sendOrQueue). Once it
        // reaches zero the dead peer channel has been cleaned up by the
        // RTC worker thread and it is safe to broadcast again.
        if (_postMigrationCooldown > 0) {
            _postMigrationCooldown--;
            CULog("[MIGRATION] Post-migration cooldown: %d frames remaining",
                  _postMigrationCooldown);

            if (_postMigrationCooldown == 0) {
                // Cooldown expired — flush any messages that were queued
                // during the window in the order they were enqueued.
                // Swap into a local vector first so _migrationQueue is empty
                // before the flush, preventing re-entry if sendOrQueue
                // triggers another queued send during the loop.
                CULog("[MIGRATION] Cooldown expired, flushing %d queued messages",
                      (int)_migrationQueue.size());
                std::vector<std::pair<std::string, std::vector<std::byte>>> toFlush;
                toFlush.swap(_migrationQueue);
                for (auto& [dest, data] : toFlush) {
                    sendOrQueue(dest, data);
                }
            }
        }
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
	passes.clear();
	_gameWon = false;
	_gameLost = false;
	_gameStarted = false;
    _sessionTerminated = false;
    _disconnectedSlots.clear();
}

/**
 * Sends an attack message to the host with the given damage value.
 * Called by non-host clients when the local player attacks the boss.
 *
 * @param damage    The amount of damage dealt to the boss.
 * @param playerIndex Which player is dealing damage to the boss
 */
void NetworkController::broadcastDamage(float damageAmount, int playerIndex) {
	_serializer.writeSint32(MessageType::BOSS_DAMAGE);
	_serializer.writeFloat(damageAmount);
	sendOrQueue("host", _serializer.serialize());
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
	sendOrQueue("host", _serializer.serialize());
	_serializer.reset();
}

/**
 * Sends a support effect application to the host for authoritative processing.
 *
 * @param effectType The kind of support effect that was applied.
 * @param magnitude  The resolved magnitude of the effect.
 * @param duration   The timed duration of the effect, or 0 for instant effects.
 * @param playerID   The 0-based index of the player receiving the effect.
 */
void NetworkController::broadcastSupportEffect(SupportEffectType effectType, float magnitude, float duration, int playerID) {
	_serializer.writeSint32(MessageType::PLAYER_SUPPORT_EFFECT);
	_serializer.writeSint32(playerID);
	_serializer.writeSint32(static_cast<int>(effectType));
	_serializer.writeFloat(magnitude);
	_serializer.writeFloat(duration);
	sendOrQueue("host", _serializer.serialize());
	_serializer.reset();
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
        CULog("This was a real player");
        // Find the UUID for this slot.
        std::string uuid = "";
        for (const auto& pair : _uuidToSlot) {
            if (pair.second == playerID) {
                uuid = pair.first;
                break;
            }
        }
        sendOrQueue(uuid, _serializer.serialize());
    }
    else {
        CULog("This was not a real player");
        sendOrQueue("host", _serializer.serialize());
    }
    _serializer.reset();
}

/**
 * Broadcasts a game start message to all connected clients.
 * Should only be called by the host when the game is ready to begin.
 */
void NetworkController::broadcastGameStart(){
    _serializer.writeSint32(MessageType::GAME_START);
    sendOrQueue("broadcast", _serializer.serialize());
    _serializer.reset();
    _gameStarted = true;
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
    sendOrQueue("host", _serializer.serialize());
    _serializer.reset();
}

/**
 * Broadcasts the current authoritative game state to all clients.
 * Should be called by the host once per frame after processing all
 * incoming attack and heal messages for that frame.
 *
 * @param state     The current authoritative game state.
 */
void NetworkController::broadcastGameState(const GameState& state) {
    _serializer.writeSint32(MessageType::GAME_UPDATE);
    _serializer.writeFloat(state.getEnemy()->getCurrentHealth());
    _serializer.writeSint32(state.getEnemy()->getTargetIndex());
    _serializer.writeSint32(state.getEnemy()->getCurrentState());
    _serializer.writeFloat(state.getEnemy()->getStateTime());
    std::vector<shared_ptr<Player>> players = state.getPlayers();
    writePlayerRuntimeState(_serializer, players);
    sendOrQueue("broadcast", _serializer.serialize());
    _serializer.reset();
}

/**
* Broadcasts to clients if the game was lost
*/
void NetworkController::broadcastLostGame() {
    _serializer.writeSint32(MessageType::GAME_LOST);
    sendOrQueue("broadcast", _serializer.serialize());
    _serializer.reset();
}

/**
* Broacasts to clients if the game was won
*/
void NetworkController::broadcastWonGame() {
    _serializer.writeSint32(MessageType::GAME_WON);
    sendOrQueue("broadcast", _serializer.serialize());
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

    // AI count first — unambiguous anchor for the receiver
    serializablePlayers.push_back(std::to_string(_aIHouses.size()));
    for (const auto& pair : _aIHouses) {
        serializablePlayers.push_back(std::to_string(pair.first));
        serializablePlayers.push_back(pair.second);
    }

    // Sort by slot index before serializing so clients always receive
    // players in consistent circle order regardless of unordered_map
    // iteration order. Without this, players reorder every broadcast.
    std::vector<std::pair<int, std::string>> slotOrder;
    for (const auto& pair : _uuidToSlot) {
        slotOrder.emplace_back(pair.second, pair.first);
    }
    std::sort(slotOrder.begin(), slotOrder.end());

    for (const auto& pair : slotOrder) {
        const NetworkedPlayer& np = _playersInfo.at(pair.second);
        serializablePlayers.push_back(np.networkID);
        serializablePlayers.push_back(np.username);
        serializablePlayers.push_back(np.houseID);
        serializablePlayers.push_back(std::to_string(pair.first));
    }

    serializablePlayers.push_back(_enemy);

    _serializer.writeSint32(MessageType::LOBBY_UPDATE);
    _serializer.writeStringVector(serializablePlayers);
    sendOrQueue("broadcast", _serializer.serialize());
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
    sendOrQueue("host", _serializer.serialize());
    _serializer.reset();
}

/**
 * Broadcasts a PLAYER_DISCONNECT message to all clients.
 *
 * @param slotIndex  The 0-based player slot that disconnected.
 */
void NetworkController::broadcastPlayerDisconnected(int slotIndex) {
    _serializer.writeSint32(MessageType::PLAYER_DISCONNECT);
    _serializer.writeSint32(slotIndex);
    sendOrQueue("broadcast", _serializer.serialize());
    _serializer.reset();
}

/*HOST ONLY. Notifies all clients that the host has exited the lobby and the session is over.*/
void NetworkController::broadcastSessionTerminated() {
    _serializer.reset();
    _serializer.writeSint32(SESSION_TERMINATED);
    auto msg = _serializer.serialize();
    sendOrQueue("broadcast", msg);
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
    sendOrQueue("broadcast", _serializer.serialize());
    _serializer.reset();
}

/**
 * Broadcasts the host's house selection for an AI slot to all clients.
 * Clients will update that slot's houseID in their local _onlinePlayers
 * list upon receiving this message.
 *
 * @param slotIndex  The 0-based AI slot index being configured.
 * @param houseID    The selected house ID, or "" to clear the selection.
 */
void NetworkController::broadcastAIHouseSelection(int slotIndex, const std::string& houseID) {
    _serializer.writeSint32(MessageType::AI_HOUSE_SELECT);
    _serializer.writeSint32(slotIndex);
    _serializer.writeString(houseID);
    sendOrQueue("broadcast", _serializer.serialize());
    _serializer.reset();

    // Store locally — host doesn't receive its own broadcast
    _aIHouses[slotIndex] = houseID;
}

/**
 * Returns whether a given player index corresponds to a real (human) player.
 * A slot is real if any UUID maps to it.
 *
 * @param playerID  The 0-based player index to check.
 * @return          true if the player is a real networked player, false if AI.
 */
bool NetworkController::checkRealPlayer(int playerID) {
    for (const auto& pair : _uuidToSlot) {
        if (pair.second == playerID) return true;
    }
    return false;
}

/**
 * Returns whether the host has broadcast a game start message.
 *
 * @return  true if the game has started, false otherwise.
 */
bool NetworkController::checkGameStarted() {
	return _gameStarted;
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
    if (_network && _uuidToSlot.count(_network->getUUID()) == 0) {
        std::string uuid = _network->getUUID();
        int slot = (int)_uuidToSlot.size();
        _uuidToSlot[uuid] = slot;
        NetworkedPlayer np;
        np.networkID = uuid;
        np.username = name;
        _playersInfo[uuid] = np;
    }
}

/**
 * Returns the current list of networked players in lobby order.
 *
 * @return  A copy of the online players list.
 */
const std::vector<NetworkedPlayer> NetworkController::getNetworkedPlayers() {
    // Sort by slot index so callers always receive players in consistent
    // circle order. _playersInfo is an unordered_map so without sorting
    // the iteration order is non-deterministic, causing LobbyScene to
    // call setRealPlayer() with mismatched slot/player combinations.
    std::vector<NetworkedPlayer> result;
    for (const auto& pair : _playersInfo) {
        result.push_back(pair.second);
    }
    std::sort(result.begin(), result.end(),
        [this](const NetworkedPlayer& a, const NetworkedPlayer& b) {
            return _uuidToSlot.at(a.networkID) < _uuidToSlot.at(b.networkID);
        });
    return result;
}

/**
 * Returns the 0-based index of the local player in the online players list.
 * This index corresponds to the player's slot in the game's player array.
 * Returns -1 if the local player is not found in the list.
 *
 * @return  The local player's index, or -1 if not found.
 */
int NetworkController::getLocalPlayerNumber() {
    std::string localID = _network->getUUID();
    auto localSlot = _uuidToSlot.find(localID);
    return localSlot != _uuidToSlot.end() ? localSlot->second : -1;
}

/**
 * Returns the 0-based index of the player in the online players list given their networkID.
 * This index corresponds to the player's slot in the game's player array.
 * Returns -1 if the player is not found in the list.
 *
 * @return  The player's index, or -1 if not found.
 */
int NetworkController::getPlayerNumberByID(const std::string& networkID) {
    auto playerNumber = _uuidToSlot.find(networkID);
    return playerNumber != _uuidToSlot.end() ? playerNumber->second : -1;
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
    std::string localID = _network ? _network->getUUID() : "";
    auto pair = _playersInfo.find(localID);
    if (pair != _playersInfo.end()) {
        pair->second.houseID = houseID;
        broadcastLobbyState();
    }
}

/**
 * Returns true if every real player has selected a house AND every AI slot
 * has a house assigned by the host. The start button only activates when
 * this returns true, enforcing that no slot enters the game without a house.
 */
bool NetworkController::allPlayersSelectedHouse() const {
    if (_playersInfo.empty()) return false;

    // All real players must have a house.
    for (const auto& pair : _playersInfo) {
        if (pair.second.houseID.empty()) return false;
    }

    // All AI slots must have a house.
    for (int i = 0; i < 4; i++) {
        bool isRealSlot = false;
        for (const auto& pair : _uuidToSlot) {
            if (pair.second == i) {
                isRealSlot = true;
                break;
            }
        }
        if (!isRealSlot) {
            auto pair = _aIHouses.find(i);
            if (pair == _aIHouses.end() || pair->second.empty()) return false;
        }
    }
    return true;
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

    for (const auto& pair : _playersInfo) {
        if (pair.first == localID) continue;
        if (pair.second.houseID == houseID) return true;
    }
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

    for (const auto& pair : _playersInfo) {
        if (pair.first == localID) continue;
        if (!pair.second.houseID.empty()) taken.push_back(pair.second.houseID);
    }
    for (const auto& pair : _aIHouses) {
        if (!pair.second.empty()) taken.push_back(pair.second);
    }
    return taken;
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
 * Registers a disconnect callback on the NetcodeConnection so that when any
 * peer closes, their slot is cleaned up and remaining clients are notified.
 *
 * There are three cases depending on who we are when the disconnect fires:
 *
 * Case 1 — We are already host (isHost() == true):
 *   A non-host client dropped during normal gameplay. We handle it immediately
 *   by replacing their slot with an AI placeholder, broadcasting a
 *   PLAYER_DISCONNECT message so all clients remove the slot from their UI,
 *   and broadcasting a LOBBY_UPDATE so all clients have the updated player list.
 *   This is the straightforward case — no migration involved.
 *
 * Case 2 — We are a client and _migrating is true:
 *   The host just dropped and triggered migration. We CANNOT handle this
 *   disconnect immediately because:
 *     - isHost() is still false, so we have no authority to broadcast.
 *     - The connection is in MIGRATING state, so all sends would be rejected.
 *   Instead, we park the departing peer's UUID in _pendingDisconnectID.
 *   registerPromotionCallback() Phase 2 will drain it once this client is
 *   confirmed as the new host and the connection is live again.
 *
 * Case 3 — We are a client and _migrating is false:
 *   A non-host peer dropped and we are also a non-host. We compact our local
 *   _onlinePlayers list. No broadcast is needed — the host will send a
 *   LOBBY_UPDATE to all remaining clients on their end.
 *
 * Should be called once after open(), alongside registerPromotionCallback().
 */
void NetworkController::registerDisconnectCallback() {
    if (!_network) return;

    _network->onDisconnect([this](const std::string& peerID) {
        auto pair = _uuidToSlot.find(peerID);
        if (pair == _uuidToSlot.end()) return;

        int slot = pair->second;
        CULog("NetworkController: slot %d (uuid='%s') disconnected", slot, peerID.c_str());
        _disconnectedSlots.push_back(slot);
        _playersInfo.erase(peerID);
        _uuidToSlot.erase(pair);

        if (isHost()) {
            broadcastPlayerDisconnected(slot);
            broadcastLobbyState();
        }
    });
}

/**
 * Registers a promotion callback to handle host migration.
 * Invoked by the CUGL lobby server in two phases when the host
 * disconnects without calling close().
 *
 * Phase 1 (confirmed = false): Volunteer as a host candidate.
 * _migrating may already be true from registerDisconnectCallback() —
 * setting it again is a no-op.
 *
 * Phase 2 (confirmed = true): This client was selected as new host.
 *   1. Clear _migrating so sendOrQueue() routes messages live again.
 *   2. Set _promotedToHost to signal GameScene to call becomeHost().
 *   3. Set _postMigrationCooldown so sends are delayed while the RTC
 *      worker thread finishes closing the old host's dead peer channel.
 *   4. Flush _migrationQueue through sendOrQueue() in arrival order.
 *
 * No slot manipulation happens here. _onlinePlayers only tracks real
 * players — the disconnect callback already removed the old host from
 * it. GameScene::becomeHost() handles the GameState demotion via
 * demoteToAI() using _gameState.getHostSlot(). broadcastLobbyState()
 * is called by GameScene after becomeHost() to sync remaining clients.
 *
 * If no client volunteers in Phase 1, or this client returns false in
 * Phase 2, migration fails and all clients are disconnected.
 *
 * Should be called once after open(), alongside registerDisconnectCallback().
 */
void NetworkController::registerPromotionCallback() {
    if (!_network) return;
    _network->onPromotion([this](bool confirmed) -> bool {
        if (!confirmed) {
            // Phase 1: volunteer as candidate. _migrating may already be true
            // if the disconnect callback fired first — setting it again is safe.
            CULog("[MIGRATION] Phase 1: lobby offered promotion, volunteering. "
                  "Migration window open, sends will be queued.");
            _migrating = true;
            return true;
        } else {
            // Phase 2: confirmed as new host.

            // Step 1: clear _migrating so sendOrQueue routes live again.
            // Must happen before the flush below so those sends are not
            // re-queued.
            CULog("[MIGRATION] Phase 2: confirmed as new host. "
                  "Clearing migration flag, flushing %d queued messages.",
                  (int)_migrationQueue.size());
            _migrating = false;

            // Step 2: signal GameScene to call becomeHost() this frame.
            _promotedToHost = true;

            // Step 3: delay sends for a few frames so the RTC worker
            // thread finishes closing the old host's dead SCTP peer
            // channel. sendOrQueue() queues into _migrationQueue while
            // this is > 0, and getNetworkUpdates() flushes once it hits
            // zero.
            _postMigrationCooldown = 10;

            // Step 4: flush messages queued during the migration window.
            // Swap into a local vector first so _migrationQueue is empty
            // before the loop — prevents re-entry if sendOrQueue triggers
            // another queued send. Each message goes through sendOrQueue
            // so the cooldown check re-queues them if still active, and
            // getNetworkUpdates() flushes once the cooldown expires.
            std::vector<std::pair<std::string, std::vector<std::byte>>> toFlush;
            toFlush.swap(_migrationQueue);
            for (auto& [destination, data] : toFlush) {
                sendOrQueue(destination, data);
            }

            logSlotStates();
            CULog("[MIGRATION] Complete. This client is now host.");
            return true;
        }
    });
}

/**
 * Routes an outgoing message either immediately or into the migration queue.
 *
 * If _migrating is true, CUGL's underlying connection will reject any send
 * attempt. Instead of dropping the message, we store it with its destination
 * so it can be replayed in order once the new host is confirmed and the
 * connection is live. This preserves causal ordering — e.g. a damage message
 * sent just before the host dropped will still reach the new host on their
 * first active frame.
 *
 * If _migrating is false, the message is sent immediately via the appropriate
 * NetcodeConnection method based on the destination key.
 *
 * @param destination  "broadcast", "host", or a specific peer UUID.
 * @param data         The serialized payload to deliver.
 */
void NetworkController::sendOrQueue(const std::string& destination,
                                    const std::vector<std::byte>& data) {
    if (_migrating) {
        // Queue with destination tag for ordered replay after migration.
        _migrationQueue.emplace_back(destination, data);
        CULog("NetworkController: queued message for '%s' during migration (queue size=%d)",
              destination.c_str(), (int)_migrationQueue.size());
        return;
    }
    
    // Post-migration cooldown: the RTC worker thread needs a few frames to
    // finish closing the old host's dead peer channel. Sending during this
    // window throws errno=32 on a background thread which cannot be caught
    // here. Queue messages until the cooldown expires; they are flushed in
    // getNetworkUpdates() once it hits zero.
    if (_postMigrationCooldown > 0) {
        _migrationQueue.emplace_back(destination, data);
        CULog("[MIGRATION] Post-migration cooldown (%d), queuing '%s'",
              _postMigrationCooldown, destination.c_str());
        return;
    }

    try {
        if (destination == "broadcast") {
            _network->broadcast(data);
        } else if (destination == "host") {
            _network->sendToHost(data);
        } else {
            _network->sendTo(destination, data);
        }
    } catch (const std::runtime_error& e) {
        // A peer channel threw errno=32 (broken pipe). This happens in the
        // frame immediately after host migration completes — the old host's
        // WebRTC peer object still exists in CUGL's peer map but its SCTP
        // transport is closed. We log and swallow rather than crash; the
        // dead peer will be cleaned up by CUGL on the next connection cycle.
        // Remaining live peers will have received the message successfully
        // before the dead one was encountered.
        CULog("[MIGRATION] sendOrQueue caught send error (likely stale peer after migration): %s",
              e.what());
    }
}

/**
 * Removes the real player entry at the given GameState slot index from
 * _onlinePlayers. Called by GameScene::becomeHost() after demoteToAI()
 * to ensure the old host is no longer treated as a real player by
 * checkRealPlayer() and broadcastPass(). Cannot rely on onDisconnect
 * firing in time from the new host's perspective.
 *
 * @param slot  The 0-based GameState slot index to remove.
 */
void NetworkController::removePlayerAtSlot(int slot) {
    for (auto pair = _uuidToSlot.begin(); pair != _uuidToSlot.end(); ++pair) {
        if (pair->second == slot) {
            CULog("[MIGRATION] removePlayerAtSlot: removing uuid='%s' at slot %d",
                  pair->first.c_str(), slot);
            _playersInfo.erase(pair->first);
            _uuidToSlot.erase(pair);
            return;
        }
    }
    CULog("[MIGRATION] removePlayerAtSlot: slot %d not found", slot);
}

/** Logs the current state of every slot in _onlinePlayers for debugging. */
void NetworkController::logSlotStates() {
    CULog("[MIGRATION] --- Slot state after migration ---");
    for (const auto& pair : _uuidToSlot) {
        const NetworkedPlayer& np = _playersInfo.at(pair.first);
        CULog("[MIGRATION] Slot %d: REAL | username='%s' | house='%s' | uuid='%s'",
              pair.second,
              np.username.c_str(),
              np.houseID.c_str(),
              pair.first.c_str());
    }
    CULog("[MIGRATION] -----------------------------------");
}

/**
 * Returns the NetworkedPlayer at the given GameState slot index.
 * Returns an empty NetworkedPlayer if no real player occupies that slot.
 *
 * @param slot  The 0-based GameState slot index.
 * @return      The NetworkedPlayer at that slot, or a default-constructed
 *              empty NetworkedPlayer if the slot is AI or unoccupied.
 */
NetworkedPlayer NetworkController::getNetworkedPlayerAtSlot(int slot) const {
    for (const auto& pair : _uuidToSlot) {
        if (pair.second == slot) {
            auto info = _playersInfo.find(pair.first);
            if (info != _playersInfo.end()) return info->second;
        }
    }
    return NetworkedPlayer{};
}

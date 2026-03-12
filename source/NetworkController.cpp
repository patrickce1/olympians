#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "NetworkController.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace cugl::netcode;
using namespace std;

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
}

/**
 * Creates and opens a new room as the host.
 * The room code can be retrieved via getRoom() once the connection
 * state reaches CONNECTED.
 */
void NetworkController::hostRoom() {
	_network = NetcodeConnection::alloc(_config);
    _network->open();
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
			AttackMessage attackMsg;
			attackMsg.damage = damage;
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
		case MessageType::PLAYER_PASS: {
			std::string itemID = _deserializer.readString();
			int passRecieverID = _deserializer.readSint32();
			PassMessage passMsg;
			passMsg.itemID = itemID;
			passMsg.playerID = passRecieverID;
			passes.push_back(passMsg);
			break;
		}
		case MessageType::GAME_START: {
			gameStarted = true;
			break;
		}
		case MessageType::PLAYER_JOIN: {
			std::string playerName = _deserializer.readString();
			CULog("HOST received join from %s with name %s", senderID.c_str(), playerName.c_str());

			// check if player is already registered
			bool alreadyRegistered = false;
			for (NetworkedPlayer player : _onlinePlayers) {
				if (player.networkID == senderID) {
					alreadyRegistered = true;
					break;
				}
			}

			if (!alreadyRegistered) {
				NetworkedPlayer newPlayer;
				newPlayer.networkID = senderID;
				newPlayer.username = playerName;
				_onlinePlayers.push_back(newPlayer);
				broadcastLobbyState();
			}
			break;
		}
		case MessageType::LOBBY_UPDATE: {
			std::vector<std::string> playerData = _deserializer.readStringVector();
			_onlinePlayers.clear();
			CULog("CLIENT received lobby update with %d entries", (int)playerData.size());
			// re-pair the flattened vector back into pairs
			for (int i = 0; i < playerData.size(); i += 2) {
				NetworkedPlayer newPlayer;
				newPlayer.networkID = playerData[i];
				newPlayer.username = playerData[i + 1];
				_onlinePlayers.push_back(newPlayer);
			}
			break;
		}
		case MessageType::GAME_UPDATE : {
				GameStateMessage stateMsg;
				stateMsg.bossHealth = _deserializer.readFloat();
				stateMsg.player1HP = _deserializer.readFloat();
				stateMsg.player2HP = _deserializer.readFloat();
				stateMsg.player3HP = _deserializer.readFloat();
				stateMsg.player4HP = _deserializer.readFloat();
				_latestGameState = stateMsg;
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
	passes.clear();
}

/**
 * Sends an attack message to the host with the given damage value.
 * Called by non-host clients when the local player attacks the boss.
 *
 * @param damage    The amount of damage dealt to the boss.
 */
void NetworkController::broadcastDamage(float damage) {
	_serializer.writeSint32(MessageType::BOSS_DAMAGE);
	_serializer.writeFloat(damage);
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
 * Returns whether a given player index corresponds to a real (human) player.
 * A player is considered real if their index falls within the online players list.
 *
 * @param playerID  The 0-based player index to check.
 * @return          true if the player is a real networked player, false if AI.
 */
bool NetworkController::checkRealPlayer(int playerID) {
	if (playerID >= _onlinePlayers.size() ) {
		return false;
	}
	else {
		return true;
	}
}

/**
 * Sends an item pass message to the target player.
 * If the target is a real player, sends directly to their network UUID.
 * If the target is an AI slot, sends to the host to handle locally.
 *
 * @param itemDefID The definition ID of the item being passed.
 * @param playerID  The 0-based index of the player to pass the item to.
 */
void NetworkController::broadcastPass(const std::string& itemDefID, int playerID) {
	_serializer.writeSint32(MessageType::PLAYER_PASS);
	_serializer.writeString(itemDefID);
	_serializer.writeSint32(playerID);
	
	CULog("Sending broadcasting message to player %d", playerID);
	if (checkRealPlayer(playerID)) {
		CULog("This was a real player");
		std::string playerNetworkID = _onlinePlayers[playerID].networkID;
		_network->sendTo(playerNetworkID, _serializer.serialize());
	}
	else {
		CULog("This was not a real player");
		_network->sendToHost(_serializer.serialize());
	}
	_serializer.reset();
}

/**
 * Broadcasts a game start message to all connected clients.
 * Should only be called by the host when the game is ready to begin.
 */
void NetworkController::broadcastGameStart(){
	_serializer.writeSint32(MessageType::GAME_START);
	_network->broadcast(_serializer.serialize());
	_serializer.reset();
}

/**
 * Returns whether the host has broadcast a game start message.
 *
 * @return  true if the game has started, false otherwise.
 */
bool NetworkController::checkGameStarted() {
	return gameStarted;
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
 */
void NetworkController::broadcastGameState(const GameState& state) {
	_serializer.writeSint32(MessageType::GAME_UPDATE);
	_serializer.writeFloat(state.getEnemy()->getCurrentHealth());
	std::vector<shared_ptr<Player>> players = state.getPlayers();
	for (int i = 0; i < 4; i++) {
		if (i < players.size()) {
			_serializer.writeFloat(players[i]->getCurrentHealth());
		}
		else {
			_serializer.writeFloat(0.0f);
		}
	}
	_network->broadcast(_serializer.serialize());
	_serializer.reset();
}


/**
 * Broadcasts the current lobby player list to all connected clients.
 * Called by the host whenever a new player joins so all clients stay in sync.
 * Serializes the online players list as a flat string vector in the format:
 * [networkID_0, username_0, networkID_1, username_1, ...]
 */
void NetworkController::broadcastLobbyState() {
	std::vector<std::string> serializablePlayers;

	for (NetworkedPlayer player : _onlinePlayers) {
		serializablePlayers.push_back(player.networkID);
		serializablePlayers.push_back(player.username);
	}

	_serializer.writeSint32(MessageType::LOBBY_UPDATE);
	_serializer.writeStringVector(serializablePlayers);
	_network->broadcast(_serializer.serialize());
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
	if (_onlinePlayers.empty()) {
		NetworkedPlayer newPlayer;
		newPlayer.username = name;
		newPlayer.networkID = _network->getUUID();
		_onlinePlayers.push_back(newPlayer);
	}
}

/**
 * Returns the current list of networked players in lobby order.
 *
 * @return  A copy of the online players list.
 */
const std::vector<NetworkedPlayer> NetworkController::getNetworkedPlayers() {
	return _onlinePlayers;
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
	for (int i = 0; i < _onlinePlayers.size(); i++) {
		if (_onlinePlayers[i].networkID == localID) {
			return i;
		}
	}
	return -1; // not found
}
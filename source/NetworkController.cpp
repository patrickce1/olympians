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

/*Implementation of functions from the header.
Read the header for details on when/how to use these functions*/
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

void NetworkController::joinRoom(const std::string room) {
	_network = NetcodeConnection::alloc(_config, dec2hex(room));
	_network->open();
}

void NetworkController::hostRoom() {
	_network = NetcodeConnection::alloc(_config);
    _network->open();
}

std::string NetworkController::getRoom() {
	if (!_network) { return "nullstr"; }
	return hex2dec(_network->getRoom());
}

void NetworkController::disconnect() {
	_network->close();
	_network = nullptr;
}

void NetworkController::dispose() {
	_network = nullptr;
}

bool NetworkController::isHost() {
	if (!_network) { return false; }
	return _network->isHost();
}

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
			CULog("The message is %d", _network->getState());
			_network->close();
			return Status::FAILED;
			break;
	}
	return FAILED;
}

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
			CULog("I recieved a passing message");
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

void NetworkController::getNetworkUpdates() {
	if (_network) {
		_network->receive([this](const std::string source,
			const std::vector<std::byte>& data) {
				handleMessage(source, data);
			});
		checkConnection();
	}
}

void NetworkController::clearQueues() {
	attacks.clear();
	heals.clear();
	passes.clear();
}

void NetworkController::broadcastDamage(float damage) {
	_serializer.writeSint32(MessageType::BOSS_DAMAGE);
	_serializer.writeFloat(damage);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

void NetworkController::broadcastHeal(float heal, int playerID) {
	_serializer.writeSint32(MessageType::PLAYER_HEAL);
	_serializer.writeFloat(heal);
	_serializer.writeSint32(playerID);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

bool NetworkController::checkRealPlayer(int playerID) {
	if (playerID >= _onlinePlayers.size() ) {
		return false;
	}
	else {
		return true;
	}
}

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

void NetworkController::broadcastGameStart(){
	_serializer.writeSint32(MessageType::GAME_START);
	_network->broadcast(_serializer.serialize());
	_serializer.reset();
}

bool NetworkController::checkGameStarted() {
	return gameStarted;
}

void NetworkController::broadcastJoinedLobby() {
	_serializer.writeSint32(MessageType::PLAYER_JOIN);
	_serializer.writeString(_playerName);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

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

void NetworkController::broadcastLobbyState() {
	//for sending over the lobby state, we unwrap the players vector into a vector of strings back to back
	//so it would look like P1 Network ID -> P1 Name -> P2 Network ID -> P2 name -> etc
	//this is done because the serializer can only do vector of strings, not of custom structs
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

void NetworkController::setPlayerName(const std::string& name) { 
	_playerName = name; 
	if (_onlinePlayers.empty()) {
		NetworkedPlayer newPlayer;
		newPlayer.username = name;
		newPlayer.networkID = _network->getUUID();
		_onlinePlayers.push_back(newPlayer);
	}
}

const std::vector<NetworkedPlayer> NetworkController::getNetworkedPlayers() {
	return _onlinePlayers;
}

int NetworkController::getLocalPlayerNumber() {
	std::string localID = _network->getUUID();
	for (int i = 0; i < _onlinePlayers.size(); i++) {
		if (_onlinePlayers[i].networkID == localID) {
			return i;
		}
	}
	return -1; // not found
}
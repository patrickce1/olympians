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


/*MAIN FUNCTIONS*/

bool NetworkController::init(const std::shared_ptr<cugl::AssetManager>& assets) {
	auto json = assets->get<JsonValue>("server");
	_config.set(json);
	_serializer = NetcodeSerializer::NetcodeSerializer();
	_deserializer = NetcodeDeserializer::NetcodeDeserializer();
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
			std::string healRecieverID = _deserializer.readString();
			HealMessage healMsg;
			healMsg.heal = heal;
			healMsg.playerID = healRecieverID;
			heals.push_back(healMsg);
			break;
		}
		case MessageType::PLAYER_PASS: {
			std::string itemID = _deserializer.readString();
			std::string passRecieverID = _deserializer.readString();
			PassMessage passMsg;
			passMsg.itemID = itemID;
			passMsg.playerID = passRecieverID;
			passes.push_back(passMsg);
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

void NetworkController::broadcastHeal(float heal, const std::string& playerID) {
	_serializer.writeSint32(MessageType::BOSS_DAMAGE);
	_serializer.writeFloat(heal);
	_serializer.writeString(playerID);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}

void NetworkController::passItem(const std::string& itemDefID, const std::string& playerID) {
	_serializer.writeSint32(MessageType::PLAYER_PASS);
	_serializer.writeString(itemDefID);
	_serializer.writeString(playerID);
	_network->sendToHost(_serializer.serialize());
	_serializer.reset();
}
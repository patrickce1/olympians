#include "NetworkController.h"

bool NetworkController::init(const std::shared_ptr<cugl::AssetManager>& assets) {
	_config.set(assets);
	return true;
}

bool NetworkController::joinRoom(const std::string room) {

}

void NetworkController::hostRoom() {

}

std::string NetworkController::getRoom() {
	return _gameid;
}

void NetworkController::disconnect() {

}

void NetworkController::dispose() {

}
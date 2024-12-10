#pragma once
#include "enet/enet.h"

namespace gse::Network {
	void initializeENet();
	//void update();
	inline void closeENet() { enet_deinitialize(); }
	//void createMultiplayerSession();
	void joinNetworkSession();
	//void setOwnAddress(std::string IP, int port);
	//void setServerAddress(std::string IP, int port);
}

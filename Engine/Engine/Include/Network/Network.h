#pragma once
#include "enet/enet.h"

namespace gse::network {
	void initialize_enet();
	//void update();
	inline void close_enet() { enet_deinitialize(); }
	//void createMultiplayerSession();
	void join_network_session();
	//void setOwnAddress(std::string IP, int port);
	//void setServerAddress(std::string IP, int port);
}

#pragma once
#include "enet/enet.h"

namespace gse::network {
	void initialize_e_net();
	//void update();
	inline void close_e_net() { enet_deinitialize(); }
	//void createMultiplayerSession();
	void join_network_session();
	//void setOwnAddress(std::string IP, int port);
	//void setServerAddress(std::string IP, int port);
}

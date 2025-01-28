module;

#include <enet/enet.h>
#include <unordered_map>
#include <iostream>
#include <string>

export module gse.network;

export namespace gse::network {
	auto initialize_enet() -> void;
	//void update();
	inline auto close_enet() -> void {  }
	//void createMultiplayerSession();
	auto join_network_session() -> void;
	//void setOwnAddress(std::string IP, int port);
	//void setServerAddress(std::string IP, int port);
}

bool g_as_server = false;
//std::unordered_map<ENetPeer, char*> clientData;
ENetAddress g_server_address;
char* g_server_ip;
ENetHost* g_server;
ENetHost* g_client;

auto gse::network::initialize_enet() -> void {
	if (enet_initialize() != 0) {
		std::cerr << "An error occurred while initializing ENet." << '\n';
	}

	if (!g_as_server) {
		g_client = enet_host_create(nullptr, 1, 1, 0, 0);

		if (g_client == nullptr) {
			std::cerr << "An error occurred while creating the client host." << '\n';
		}
	}

}

//void Engine::Network::update() {
//	ENetEvent event;
//	ENetPeer* peer;
//
//
//	while (enet_host_service(client, &event, 1000) > 0) {
//		switch (event.type) {
//		case ENET_EVENT_TYPE_CONNECT:
//			std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << std::endl;
//			//When a new client connects, add them to the list of clients along with an empty byte as their last received packet.
//			clientData[*event.peer] = reinterpret_cast<char*>(00000000);
//			break;
//		case ENET_EVENT_TYPE_RECEIVE:
//			std::cout << "A packet of length " << event.packet->dataLength << " containing " << event.packet->data << " was received from " << event.peer->address.host << ":" << event.peer->address.port << std::endl;
//
//			//Store the packet for future use and associate it with this user.
//			clientData[*event.peer] = reinterpret_cast<char*>(event.packet->data);
//
//			enet_packet_destroy(event.packet);
//			break;
//		case ENET_EVENT_TYPE_DISCONNECT:
//			std::cout << event.peer->data << " disconnected." << std::endl;
//			event.peer->data = NULL;
//
//			//Remove the client from the map
//			clientData.erase(*event.peer);
//
//			break;
//		}
//	}
//
//	ENetPacket* packet = enet_packet_create("message", strlen("message") + 1, ENET_PACKET_FLAG_RELIABLE);
//	enet_host_broadcast(server, 0, packet);
//	enet_host_flush(server);
//
//}

auto gse::network::join_network_session() -> void {
	ENetEvent event;
	enet_address_set_host(&g_server_address, g_server_ip);
	g_server_address.port = 1234;

	ENetPeer* peer = enet_host_connect(g_client, &g_server_address, 1, 0);
	if (peer == nullptr) { std::cerr << "No available peers for initiating an ENet connection." << '\n'; }

	if (enet_host_service(g_client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		std::cout << "Connection to " << g_server_address.host << ":" << g_server_address.port << " succeeded." <<
			'\n';
	}
	else {
		enet_peer_reset(peer);
		std::cerr << "Connection to " << g_server_ip << ":" << g_server_address.port << " failed." << '\n';
	}
}

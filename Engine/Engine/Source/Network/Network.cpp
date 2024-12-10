#include "Network/Network.h"

#include <unordered_map>
#include <iostream>
#include <string>

bool asServer = false;
//std::unordered_map<ENetPeer, char*> clientData;
ENetAddress serverAddress;
char* serverIP;
ENetHost* server;
ENetHost* client;

void gse::network::initialize_e_net() {
	if (enet_initialize() != 0) {
		std::cerr << "An error occurred while initializing ENet." << std::endl;
	}

	if (!asServer) {
		client = enet_host_create(NULL, 1, 1, 0, 0);

		if (client == NULL) {
			std::cerr << "An error occurred while creating the client host." << std::endl;
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

void gse::network::join_network_session() {
	ENetPeer* peer;
	ENetEvent event;
	enet_address_set_host(&serverAddress, serverIP);
	serverAddress.port = 1234;

	peer = enet_host_connect(client, &serverAddress, 1, 0);
	if (peer == NULL) {std::cerr << "No available peers for initiating an ENet connection." << std::endl;}

	if (enet_host_service (client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {std::cout << "Connection to " << serverAddress.host << ":" << serverAddress.port << " succeeded." << std::endl;}
	else {
		enet_peer_reset(peer);
		std::cerr << "Connection to " << serverIP << ":" << serverAddress.port << " failed." << std::endl;
	}	
}
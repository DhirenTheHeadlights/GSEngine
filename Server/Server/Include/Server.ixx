module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

export module gse.server;

import game;
import gse;
import std;

export namespace gse::server {
	auto initialize() -> void;
	auto run() -> bool;
	auto stop() -> void;
}

namespace gse::server
{
	auto game_update(const std::function<bool()>& update_function) -> void;
	auto network_update() -> void;
	auto get_client_key(const gse::network::address& address) -> std::string;
	auto generate_new_player_id() -> uint32_t;
}


struct client_info {
    uint32_t player_id;
    gse::network::address address;
    bool connected;
	gse::platform::glfw::input::keyboard client_inputs;
};

gse::network::udp_socket server_socket;
sockaddr_in server_addr{};

std::unordered_map<std::string, client_info> clients;


auto gse::server::initialize() -> void {
	game::initialize();

	gse::network::initialize_socket(server_socket);
	uint32_t port = 12345; // Default port for the server
	server_socket.bind_socket(port);

	std::cout << "Server is running on port: " + std::to_string(port) + "\n";

	std::cout << "Server initialized." << std::endl;
}


auto gse::server::game_update(const std::function<bool()>& update_function) -> void {

	/*gse::main_clock::update();
	gse::scene_loader::update();
	gse::input::update();
	if (!update_function()) {
		gse::request_shutdown();
	}
	gse::registry::periodically_clean_up_registry();
	if (!update_function()) {
		gse::request_shutdown();
	}*/
}

auto gse::server::network_update() -> void {

	gse::network::address client_addr;
	gse::network::receive_components(client_addr, server_socket);
	std::string key = get_client_key(client_addr);

	if (clients.find(key) == clients.end()) {
		// New client detected
		uint32_t new_player_id = generate_new_player_id();
		clients[key] = { new_player_id, client_addr, true };
		std::cout << "New client registered: " << key << " with Player ID " << new_player_id << "\n";
	}

	for (auto& [client_key, client_info] : clients) {
		if (client_info.connected) {
			gse::network::send_components(client_info.address, server_socket);
		}
	}
}

auto gse::server::get_client_key(const gse::network::address& address) -> std::string {
	return address.ip + ":" + std::to_string(address.port);
}

auto gse::server::generate_new_player_id() -> uint32_t {
	static uint32_t next_id = 1;
	return next_id++;
}

auto gse::server::run() -> bool {
	//gse::server::game_update(update_function);
	game::update();
	gse::server::network_update();
	return true;
}

auto gse::server::stop() -> void {
	//server_socket.~udp_socket();
	//gse::shutdown();
	//std::cout << "Server stopped." << std::endl;
}
export module gse.server;

import game;
import gse;
import std;

export namespace gse::server {
	auto initialize() -> void;
	auto update() -> bool;
	auto exit() -> void;
}

namespace gse::server {
	auto network_update() -> void;
	auto get_client_key(const network::address& address) -> std::string;
	auto generate_new_player_id() -> std::uint32_t;
}


struct client_info {
    std::uint32_t player_id;
    gse::network::address address;
    bool connected;
	std::vector<gse::input::callback> callbacks;
};

gse::network::udp_socket server_socket;
gse::network::address server_address;

std::unordered_map<std::string, client_info> clients;

auto gse::server::initialize() -> void {
	game::initialize();

	initialize_socket(server_socket);
	std::uint32_t port = 12345; // Default port
	perma_assert(server_socket.bind_socket(port) == network::socket_state::ready, "Failed to bind socket.");

	std::cout << "Server is running on port: " + std::to_string(port) + "\n";
	std::cout << "Server initialized." << std::endl;
}

auto gse::server::network_update() -> void {

	
}

auto gse::server::get_client_key(const network::address& address) -> std::string {
	return address.ip + ":" + std::to_string(address.port);
}

auto gse::server::generate_new_player_id() -> uint32_t {
	static uint32_t next_id = 1;
	return next_id++;
}

auto gse::server::update() -> bool {
	game::update();

	network::address client_addr;
	receive_components(client_addr, server_socket);

	if (const std::string key = get_client_key(client_addr); !clients.contains(key)) {
		uint32_t new_player_id = generate_new_player_id();
		clients[key] = { new_player_id, client_addr, true };
		std::cout << "New client registered: " << key << " with Player ID " << new_player_id << "\n";
	}

	for (auto& client_info : clients | std::views::values) {
		if (client_info.connected) {
			send_components(client_info.address, server_socket);
		}
	}

	return true;
}

auto gse::server::exit() -> void {
	
}
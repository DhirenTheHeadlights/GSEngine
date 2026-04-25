export module gse.network:server_info;

import std;

import :message;

export namespace gse::network {
	struct [[= network_message{}]] server_info_request {
	};

	struct [[= network_message{}]] server_info_response {
		std::uint8_t players{};
		std::uint8_t max_players{};
	};
}

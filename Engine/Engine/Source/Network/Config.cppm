export module gse.network:config;

import std;

export namespace gse::network {
	constexpr std::uint16_t default_port = 9000;

	enum struct session_role : std::uint8_t {
		offline,
		client,
		dedicated
	};

	struct config {
		session_role role = session_role::offline;
		std::string connect;
		std::uint16_t listen_port = default_port;
		std::uint8_t max_players = 8;
	};

	auto resolve_role(
		const config& c
	) -> session_role;
}

auto gse::network::resolve_role(const config& c) -> session_role {
	if (c.role != session_role::offline) {
		return c.role;
	}
	return c.connect.empty() ? session_role::offline : session_role::client;
}

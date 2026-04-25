export module gse.network:ping_pong;

import std;

import :message;

export namespace gse::network {
	struct [[= network_message{}]] ping {
		std::uint32_t sequence{};
	};

	struct [[= network_message{}]] pong {
		std::uint32_t sequence{};
	};
}

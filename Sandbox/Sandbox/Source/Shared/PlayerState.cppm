export module sandbox:player_state;

import std;
import gse;

export namespace sandbox {
	struct [[= gse::network::network_message{}]] player_state {
		gse::id entity;
		std::uint64_t server_step = 0;
		std::uint32_t input_sequence = 0;
		gse::vec3<gse::current_position> position;
		gse::vec3<gse::velocity> current_velocity;
		gse::quat orientation;
		gse::vec3<gse::velocity> drive;
	};
}

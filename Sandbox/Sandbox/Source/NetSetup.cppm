export module sandbox:net_setup;

import gse;

import :player_state;
import :sidearm;

export namespace sandbox {
	using networked_components = gse::engine_networked_components;

	using network_messages = gse::type_pack<gse::network::input_frame, player_state, sidearm::fire_request>;

	auto server_setup(
		gse::engine& e
	) -> void;
}

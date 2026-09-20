export module sandbox:net_setup;

import gse;

import :player_state;
import :sidearm;

export namespace sandbox {
	using sandbox_networked_components = gse::type_pack<>;

	using networked_components = gse::type_pack_concat<gse::engine_networked_components, sandbox_networked_components>::type;

	using network_messages = gse::type_pack<gse::network::input_frame, player_state, sidearm::fire_request>;

	auto server_setup(
		gse::engine& e
	) -> void;
}

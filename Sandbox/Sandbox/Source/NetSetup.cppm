export module sandbox:net_setup;

import gse;

export namespace sandbox {
	using networked_components = gse::engine_networked_components;

	using network_messages = gse::type_pack<>;

	auto server_setup(
		gse::engine& e
	) -> void;
}

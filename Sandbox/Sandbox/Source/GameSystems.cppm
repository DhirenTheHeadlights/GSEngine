export module sandbox:game_systems;

import gse.ecs;
import gse.runtime;

export namespace sandbox {
	auto register_client_systems(
		gse::context& ctx
	) -> void;

	auto register_server_systems(
		gse::engine& e
	) -> void;
}
export module sandbox:role;

import gse.ecs;
import gse.runtime;

export namespace sandbox {
	auto is_server(
		gse::shared_view<gse::world_system::data> world
	) -> bool;

	auto is_client(
		gse::shared_view<gse::world_system::data> world
	) -> bool;

	auto is_offline(
		gse::shared_view<gse::world_system::data> world
	) -> bool;
}

auto sandbox::is_server(const gse::shared_view<gse::world_system::data> world) -> bool {
	return world.networked && world.authoritative;
}

auto sandbox::is_client(const gse::shared_view<gse::world_system::data> world) -> bool {
	return world.networked && !world.authoritative;
}

auto sandbox::is_offline(const gse::shared_view<gse::world_system::data> world) -> bool {
	return !world.networked;
}
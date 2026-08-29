export module gse.runtime:network_setup;

import std;

import gse.containers;
import gse.ecs;
import gse.meta;
import gse.network;
import gse.system_manifest;

import :engine;

export namespace gse {
	template <typename MessagePack, typename... Components>
	auto network_setup(
		engine& e,
		type_pack<Components...> components = {},
		MessagePack messages = {}
	) -> void;
}

namespace gse {
	template <typename MessagePack, typename... Components>
	consteval auto network_run_hook(
		type_pack<Components...>
	) -> std::meta::info;
}

template <typename MessagePack, typename... Components>
consteval auto gse::network_run_hook(type_pack<Components...>) -> std::meta::info {
	return ^^network::run<MessagePack, Components...>;
}

template <typename MessagePack, typename... Components>
auto gse::network_setup(engine& e, type_pack<Components...>, MessagePack) -> void {
	constexpr auto run_hook = network_run_hook<MessagePack>(type_pack<Components...>{});
	static_assert(
		meta::find_system_hook_anno(run_hook) != std::meta::info{},
		"network run hook annotation not visible on the templated instantiation"
	);

	system_manifest<^^network::data, run_hook, ^^network::shutdown>{}.register_with(e);
}

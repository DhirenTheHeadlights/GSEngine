export module gse.runtime:renderer_api;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.graphics;
import gse.os;
import gse.assets;
import gse.gpu;

import :core_api;

export namespace gse {
	template <typename Resource>
	auto get(
		const id& id
	) -> resource::handle<Resource>;

	template <typename Resource>
	auto get(
		const std::string& filename
	) -> resource::handle<Resource>;

	template <typename Resource, typename... Args>
	auto queue(
		const std::string& name,
		Args&&... args
	) -> resource::handle<Resource>;

	template <typename Resource>
	auto instantly_load(
		const id& id
	) -> resource::handle<Resource>;

	template <typename Resource>
	auto add(
		Resource&& resource
	) -> void;

	template <typename Resource>
	auto resource_state(
		const id& id
	) -> resource::state;

	auto set_ui_focus(
		const bool focus
	) -> void;
}

template <typename Resource>
auto gse::get(const id& id) -> resource::handle<Resource> {
	auto* assets = const_cast<asset::registry::state*>(try_state_of<asset::registry::state>());
	if (!assets) {
		return {};
	}
	return asset::registry::get<Resource>(*assets, id);
}

template <typename Resource>
auto gse::get(const std::string& filename) -> resource::handle<Resource> {
	auto* assets = const_cast<asset::registry::state*>(try_state_of<asset::registry::state>());
	if (!assets) {
		return {};
	}
	return asset::registry::get<Resource>(*assets, filename);
}

template <typename Resource, typename... Args>
auto gse::queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
	auto* assets = const_cast<asset::registry::state*>(try_state_of<asset::registry::state>());
	if (!assets) {
		return {};
	}
	return asset::registry::queue<Resource>(*assets, name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::instantly_load(const id& id) -> resource::handle<Resource> {
	auto* assets = const_cast<asset::registry::state*>(try_state_of<asset::registry::state>());
	if (!assets) {
		return {};
	}
	const auto handle = asset::registry::get<Resource>(*assets, id);
	asset::registry::instantly_load(*assets, handle);
	return handle;
}

template <typename Resource>
auto gse::add(Resource&& resource) -> void {
	auto* assets = const_cast<asset::registry::state*>(try_state_of<asset::registry::state>());
	if (!assets) {
		return;
	}
	asset::registry::add<Resource>(*assets, std::forward<Resource>(resource));
}

template <typename Resource>
auto gse::resource_state(const id& id) -> resource::state {
	const auto* assets = try_state_of<asset::registry::state>();
	if (!assets) {
		return resource::state::unloaded;
	}
	return asset::registry::resource_state<Resource>(*assets, id);
}

auto gse::set_ui_focus(const bool focus) -> void {
	channel_add(ui_focus_request{ .focus = focus });
}

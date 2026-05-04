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
	if (!has_state<renderer::system::state>()) {
		return {};
	}
	return resources_of<renderer::system::resources>().assets->get<Resource>(id);
}

template <typename Resource>
auto gse::get(const std::string& filename) -> resource::handle<Resource> {
	if (!has_state<renderer::system::state>()) {
		return {};
	}
	return resources_of<renderer::system::resources>().assets->get<Resource>(filename);
}

template <typename Resource, typename... Args>
auto gse::queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
	if (!has_state<renderer::system::state>()) {
		return {};
	}
	return resources_of<renderer::system::resources>().assets->queue<Resource>(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::instantly_load(const id& id) -> resource::handle<Resource> {
	if (!has_state<renderer::system::state>()) {
		return {};
	}
	return resources_of<renderer::system::resources>().assets->instantly_load<Resource>(id);
}

template <typename Resource>
auto gse::add(Resource&& resource) -> void {
	if (!has_state<renderer::system::state>()) {
		return;
	}
	resources_of<renderer::system::resources>().assets->add<Resource>(std::forward<Resource>(resource));
}

template <typename Resource>
auto gse::resource_state(const id& id) -> resource::state {
	if (!has_state<renderer::system::state>()) {
		return resource::state::unloaded;
	}
	return resources_of<renderer::system::resources>().assets->resource_state<Resource>(id);
}

auto gse::set_ui_focus(const bool focus) -> void {
	if (!has_state<renderer::system::state>()) {
		return;
	}
	resources_of<renderer::system::resources>().ctx->set_ui_focus(focus);
}

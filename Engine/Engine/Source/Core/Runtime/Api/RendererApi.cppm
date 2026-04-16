export module gse.runtime:renderer_api;

import std;

import gse.utility;
import gse.graphics;
import gse.platform;

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

namespace gse {
	auto renderer_resources() -> const renderer::system::resources& {
		return resources_of<renderer::system::resources>();
	}
}

template <typename Resource>
auto gse::get(const id& id) -> resource::handle<Resource> {
	if (!has_state<renderer::state>()) {
		return {};
	}
	return renderer_resources().get<Resource>(id);
}

template <typename Resource>
auto gse::get(const std::string& filename) -> resource::handle<Resource> {
	if (!has_state<renderer::state>()) {
		return {};
	}
	return renderer_resources().get<Resource>(filename);
}

template <typename Resource, typename... Args>
auto gse::queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
	if (!has_state<renderer::state>()) {
		return {};
	}
	return renderer_resources().queue<Resource>(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::instantly_load(const id& id) -> resource::handle<Resource> {
	if (!has_state<renderer::state>()) {
		return {};
	}
	return renderer_resources().instantly_load<Resource>(id);
}

template <typename Resource>
auto gse::add(Resource&& resource) -> void {
	if (!has_state<renderer::state>()) {
		return;
	}
	renderer_resources().add<Resource>(std::forward<Resource>(resource));
}

template <typename Resource>
auto gse::resource_state(const id& id) -> resource::state {
	if (!has_state<renderer::state>()) {
		return resource::state::unloaded;
	}
	return renderer_resources().resource_state<Resource>(id);
}

auto gse::set_ui_focus(const bool focus) -> void {
	if (!has_state<renderer::state>()) {
		return;
	}
	renderer_resources().set_ui_focus(focus);
}

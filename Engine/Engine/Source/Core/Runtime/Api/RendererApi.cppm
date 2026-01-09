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
	) -> resource::handle<Resource> {
		return system_of<renderer::system>().get<Resource>(id);
	}

	template <typename Resource>
	auto get(
		const std::string& filename
	) -> resource::handle<Resource> {
		return system_of<renderer::system>().get<Resource>(filename);
	}

	template <typename Resource, typename... Args>
	auto queue(
		const std::string& name,
		Args&&... args
	) -> resource::handle<Resource> {
		return system_of<renderer::system>().queue<Resource>(name, std::forward<Args>(args)...);
	}

	template <typename Resource>
	auto instantly_load(
		const id& id
	) -> resource::handle<Resource> {
		return system_of<renderer::system>().instantly_load<Resource>(id);
	}

	template <typename Resource>
	auto add(
		Resource&& resource
	) -> void {
		system_of<renderer::system>().add<Resource>(std::forward<Resource>(resource));
	}

	template <typename Resource>
	auto resource_state(
		const id& id
	) -> resource::state {
		return system_of<renderer::system>().resource_state<Resource>(id);
	}

	auto camera(
	) -> camera& {
		return system_of<renderer::system>().camera();
	}

	auto set_ui_focus(
		const bool focus
	) -> void {
		system_of<renderer::system>().set_ui_focus(focus);
	}

	auto frame_begun(
	) -> bool {
		return system_of<renderer::system>().frame_begun();
	}
}
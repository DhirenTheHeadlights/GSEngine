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
		return state_of<renderer::state>().get<Resource>(id);
	}

	template <typename Resource>
	auto get(
		const std::string& filename
	) -> resource::handle<Resource> {
		return state_of<renderer::state>().get<Resource>(filename);
	}

	template <typename Resource, typename... Args>
	auto queue(
		const std::string& name,
		Args&&... args
	) -> resource::handle<Resource> {
		return state_of<renderer::state>().queue<Resource>(name, std::forward<Args>(args)...);
	}

	template <typename Resource>
	auto instantly_load(
		const id& id
	) -> resource::handle<Resource> {
		return state_of<renderer::state>().instantly_load<Resource>(id);
	}

	template <typename Resource>
	auto add(
		Resource&& resource
	) -> void {
		state_of<renderer::state>().add<Resource>(std::forward<Resource>(resource));
	}

	template <typename Resource>
	auto resource_state(
		const id& id
	) -> resource::state {
		return state_of<renderer::state>().resource_state<Resource>(id);
	}

	auto camera(
	) -> camera& {
		return state_of<renderer::state>().camera();
	}

	auto set_ui_focus(
		const bool focus
	) -> void {
		state_of<renderer::state>().set_ui_focus(focus);
	}

	auto frame_begun(
	) -> bool {
		return state_of<renderer::state>().is_frame_begun();
	}
}
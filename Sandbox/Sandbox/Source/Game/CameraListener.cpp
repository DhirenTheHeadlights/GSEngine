module sandbox:camera_listener_impl;

import std;
import gse;

import :camera_listener;

namespace sandbox::camera_listener {
	struct archetype {
		gse::physics::transform_component transform;
		gse::audio::listener_component listener;
		component follows_camera;
	};
}

auto sandbox::camera_listener::run(data&, const gse::shared_view<gse::camera::data> cam_s, gse::read<component> followers, gse::read<gse::camera::follow_component> follows, gse::write<gse::audio::listener_component> listeners, const pose_out pose) -> gse::async::task<> {
	const gse::id controller = cam_s.active_controller_entity;
	const auto* view = follows.find(controller);
	if (view == nullptr) {
		return {};
	}

	const auto owners = listeners.owner_ids();
	for (std::size_t index = 0; index < listeners.size(); ++index) {
		const gse::id owner = owners[index];
		if (followers.find(owner) == nullptr) {
			continue;
		}
		listeners[index].body = controller;
		pose.push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
			.target = owner,
			.value = view->position + view->offset,
		});
		pose.push<gse::physics::set_request<^^gse::physics::transform_component::orientation>>({
			.target = owner,
			.value = view->orientation,
		});
	}

	return {};
}

auto sandbox::camera_listener::spawn(gse::scene& s) -> gse::id {
	return s.spawn("Camera Listener", archetype{});
}

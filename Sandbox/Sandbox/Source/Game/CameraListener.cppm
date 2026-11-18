export module sandbox:camera_listener;

import std;
import gse;

export namespace sandbox::camera_listener {
	struct component {};

	struct [[= gse::system_state<"CameraListener">{}]]
	data {};

	using pose_out = gse::channel_write<
		gse::physics::set_request<^^gse::physics::transform_component::position>,
		gse::physics::set_request<^^gse::physics::transform_component::orientation>
	>;

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		gse::shared_view<gse::camera::data> cam_s,
		gse::read<component> followers,
		gse::read<gse::camera::follow_component> follows,
		gse::write<gse::audio::listener_component> listeners,
		pose_out pose
	) -> gse::async::task<>;

	auto spawn(
		gse::scene& s
	) -> gse::id;
}

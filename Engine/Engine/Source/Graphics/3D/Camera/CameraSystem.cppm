export module gse.graphics:camera_system;

import std;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.network;
import gse.os;
import gse.assets;
import gse.gpu;

import :camera_data;
import :camera_component;

export namespace gse::camera {
	struct viewport_update {
		vec2f size{ 1920.f, 1080.f };
	};
}

export namespace gse::camera {
	struct [[= gse::system_state<"Camera">{}, = gse::deferred_system{}]] data {
		target current{};
		target blend_from{};
		target blend_to{};
		time blend_elapsed{};
		time blend_duration{};
		bool blending = false;

		[[= gse::shared]] id active_controller_entity{};
		int active_priority = -1;

		angle yaw = degrees(0.f);

		[[= gse::shared]] gse::view_matrix view_matrix{};
		[[= gse::shared]] gse::projection_matrix projection_matrix{};
		[[= gse::shared]] gse::view_matrix prev_view_matrix{};
		[[= gse::shared]] gse::projection_matrix prev_projection_matrix{};
		[[= gse::shared]] vec2f jitter_ndc{};
		[[= gse::shared]] vec2f prev_jitter_ndc{};

		std::uint32_t jitter_index = 1;
		vec2f viewport{ 1920.f, 1080.f };
		[[= gse::shared]] bool ui_focus = false;
	};

	[[= gse::system_init{}]]
	auto init(
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		read<follow_component> cameras
	) -> async::task<>;

	auto position(
		const data& d
	) -> vec3<gse::position>;

	auto orientation(
		const data& d
	) -> quat;

	auto direction_relative_to_origin(
		const data& d,
		const vec3f& direction
	) -> vec3f;

	auto interpolate_target(
		const target& from,
		const target& to,
		float t
	) -> target;

	auto compute_view_matrix(
		const target& t
	) -> view_matrix;

	auto compute_projection_matrix(
		const target& t,
		const vec2f& viewport
	) -> projection_matrix;
}

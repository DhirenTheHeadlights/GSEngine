export module gse.graphics:camera_system;

import std;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;

import :camera_data;
import :camera_component;

export namespace gse::camera {
	struct ui_focus_update {
		bool focus = false;
	};

	struct viewport_update {
		vec2f size{ 1920.f, 1080.f };
	};
}

export namespace gse::camera {
	class system {
	public:
		struct state {
			target current{};
			target blend_from{};
			target blend_to{};
			time blend_elapsed{};
			time blend_duration{};
			bool blending = false;

			id active_controller_entity{};
			int active_priority = -1;

			angle yaw = degrees(-90.0f);
			angle pitch = degrees(0.0f);
			float mouse_sensitivity = 0.1f;

			gse::view_matrix view_matrix{};
			gse::projection_matrix projection_matrix{};

			vec2f viewport{ 1920.f, 1080.f };
			bool ui_focus = false;
		};

		static auto initialize(
			init_context& phase,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			state& s
		) -> async::task<>;

		static auto position(
			const state& s
		) -> vec3<gse::position>;

		static auto orientation(
			const state& s
		) -> quat;

		static auto direction_relative_to_origin(
			const state& s,
			const vec3f& direction
		) -> vec3f;

	private:
		static auto interpolate_target(
			const target& from,
			const target& to,
			float t
		) -> target;

		static auto compute_view_matrix(
			const target& t
		) -> view_matrix;

		static auto compute_projection_matrix(
			const target& t,
			const vec2f& viewport
		) -> projection_matrix;
	};
}

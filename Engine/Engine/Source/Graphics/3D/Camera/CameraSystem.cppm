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

namespace gse::camera {
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

export namespace gse::camera {
	struct system {
		static auto initialize(
			init_context& phase,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			state& s
		) -> async::task<>;
	};
}

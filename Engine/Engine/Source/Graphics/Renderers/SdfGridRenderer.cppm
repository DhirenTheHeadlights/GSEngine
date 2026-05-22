export module gse.graphics:sdf_grid_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;

import :camera_system;

export namespace gse::renderer::sdf_grid {
	struct system {
		struct [[= gse::settings::category<"Grid">{}]] data {
			[[
				= gse::settings::describe<"Render a procedural SDF-based grid on the y=0 plane.">{},
				= gse::shared
			]]
			bool enabled = true;
			[[
				= gse::settings::describe<"Stamp distance labels along the X and Z axes at every major gridline.">{},
				= gse::shared
			]]
			bool show_labels = true;

			length minor_spacing = meters(1.f);
			[[= gse::shared]] length major_spacing = meters(10.f);
			[[= gse::shared]] length fade_distance = meters(200.f);
			[[= gse::shared]] length label_size = meters(0.5f);
			float minor_thickness = 1.0f;
			float major_thickness = 1.5f;
			float axis_thickness = 2.0f;
			vec3f minor_color{ 0.20f, 0.20f, 0.24f };
			vec3f major_color{ 0.40f, 0.40f, 0.46f };
			vec3f axis_color{ 0.85f, 0.30f, 0.30f };
			[[= gse::shared]] vec3f label_color{ 1.0f, 1.0f, 1.0f };

			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
			per_frame_resource<gpu::buffer> camera_ubo_buffers;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			data& d
		) -> async::task<>;

		static auto frame(
			const frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<camera::system> cam_state
		) -> async::task<>;
	};
}

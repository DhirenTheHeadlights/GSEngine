export module gse.graphics:sdf_grid_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;
import gse.gpu_record;

import :camera_system;

export namespace gse::renderer::sdf_grid {
	struct [[= system_state<"SdfGrid">{}, = settings::category<"Grid">{}]] data {
		[[
			= settings::describe<"Render a procedural SDF-based grid on the y=0 plane.">{},
			= shared
		]]
		bool enabled = true;
		[[
			= settings::describe<"Stamp distance labels along the X and Z axes at every major gridline.">{},
			= shared
		]]
		bool show_labels = true;

		[[
			= settings::describe<"Spacing of the fine gridlines. Lines fade out once a cell projects to fewer than "
									  "a few pixels, so this — not fade_distance — is what decides how far the grid "
									  "reaches.">{}
		]]
		length minor_spacing = meters(1.f);

		[[
			= settings::describe<"Spacing of the emphasized gridlines. Widen this along with minor_spacing for a "
									  "shot that needs the grid to survive out to the horizon.">{},
			= shared
		]]
		length major_spacing = meters(10.f);
		[[
			= settings::describe<"Distance at which the world grid fades to nothing. The default suits close-up "
									  "dev work; a shot that shows the horizon needs this out past it or the grid "
									  "stops well short of where the ground ends.">{},
			= shared
		]]
		length fade_distance = meters(200.f);
		[[= shared]] length label_size = meters(0.5f);
		float minor_thickness = 1.0f;
		float major_thickness = 1.5f;
		float axis_thickness = 2.0f;
		vec3f minor_color{ 0.20f, 0.20f, 0.24f };
		vec3f major_color{ 0.40f, 0.40f, 0.46f };
		vec3f axis_color{ 0.85f, 0.30f, 0.30f };
		[[= shared]] vec3f label_color{ 1.0f, 1.0f, 1.0f };

		gpu::shader_program pipeline;
		per_frame_resource<gpu::buffer> camera_ubo_buffers;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<camera::data> cam_state
	) -> async::task<>;
}
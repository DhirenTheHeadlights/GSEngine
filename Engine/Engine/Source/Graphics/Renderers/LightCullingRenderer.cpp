module gse.graphics:light_culling_renderer_impl;

import std;

import :light_culling_renderer;
import :geometry_collector;
import :light_packing;
import :point_light;
import :spot_light;
import :directional_light;
import :camera_system;
import :depth_prepass_renderer;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

namespace gse::renderer::light_culling {
	struct [[= shaders::shader_struct]] culling_params_data {
		projection_matrix projection;
		inverse_projection_matrix inv_proj;
		vec2u screen_size;
		std::uint32_t num_lights;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::texture2d
	]] depth_texture {
		using element = vec4f;
	};

	struct [[= shaders::binding<0, 1>{}]] culling_params {
		using element = culling_params_data;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::ssbo_readonly
	]] lights {
		using element = shaders::forward::light;
	};

	struct [[
		= shaders::binding<0, 3>{},
		= shaders::ssbo_readwrite
	]] light_index_list {
		using element = std::uint32_t;
	};

	struct [[
		= shaders::binding<0, 4>{},
		= shaders::ssbo_readwrite
	]] tile_light_table {
		using element = vec2u;
	};

	struct [[
		= shaders::binding<0, 5>{},
		= shaders::sampler_state
	]] depth_sampler {};

	using shader_binding_types = type_pack<depth_texture, culling_params, lights, light_index_list, tile_light_table, depth_sampler>;

	using shader_types = type_pack<culling_params_data, light_culling_limits>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/light_culling">,
		gpu::types<shaders::forward::shader_types, shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<16, 16, 1>,
		gpu::system_values<gpu::group_id, gpu::group_thread_id, gpu::group_index>
	>;
}

namespace gse::renderer::light_culling {
	auto update_depth_descriptor(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
		if (!d.depth_view.valid()) { d.depth_view = gpu_s.device->allocate_image_slot(); }
		gpu_s.device->write_sampled_image(d.depth_view.slot(), gpu_s.render_graph->depth_image());
	}

	auto rebuild_tile_buffers(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
		const auto ext = gpu_s.render_graph->extent();
		d.current_width = ext.x();
		d.current_height = ext.y();

		const auto tiles = tile_count(d.current_width, d.current_height);
		const std::uint32_t total_tiles = tiles.x() * tiles.y();
		const std::uint32_t index_list_size = total_tiles * max_lights_per_tile * sizeof(std::uint32_t);
		const std::uint32_t tile_table_size = total_tiles * 2 * sizeof(std::uint32_t);

		for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
			d.light_index_list_buffers[i] = gpu_s.device->create_buffer(
				{
					.size = index_list_size,
					.stride = sizeof(std::uint32_t),
					.usage = gpu::buffer_flag::storage,
					.bindless = true,
					.writable = true
			}
			).handle();

			d.tile_light_table_buffers[i] = gpu_s.device->create_buffer(
				{
					.size = tile_table_size,
					.stride = sizeof(std::uint32_t),
					.usage = gpu::buffer_flag::storage,
					.bindless = true,
					.writable = true
			}
			).handle();
		}

		update_depth_descriptor(gpu_s, d);
	}
}

auto gse::renderer::light_culling::tile_count(const std::uint32_t width, const std::uint32_t height) -> vec2u {
	return { (width + tile_size - 1) / tile_size, (height + tile_size - 1) / tile_size };
}

auto gse::renderer::light_culling::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.culling_params_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(culling_params_data),
				.stride = sizeof(culling_params_data),
				.usage = gpu::buffer_flag::storage,
				.bindless = true
			}
		);

		d.light_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::forward::light) * max_lights,
				.stride = sizeof(shaders::forward::light),
				.usage = gpu::buffer_flag::storage,
				.bindless = true
			}
		);
	}

	d.depth_sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::nearest,
			.mag = gpu::sampler_filter::nearest,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
			.border = gpu::border_color::float_opaque_white,
			.max_lod = 1.0f
		}
	);

	rebuild_tile_buffers(gpu_s, d);

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[gpu_s, &d]() {
			rebuild_tile_buffers(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::light_culling::frame(context& ctx, shared_view<gpu::context::data> gpu_s, const data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera::data> cam_state, shared_view<atmosphere::data> atm_state, read<directional_light_component> dir_lights, read<spot_light_component> spot_lights, read<point_light_component> point_lights, read<physics::transform_component> transforms) -> async::task<> {
	auto& graph = *gpu_s.render_graph;

	if (!graph.frame_in_progress()) {
		co_return;
	}

	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto frame_index = graph.current_frame();

	const auto proj = cam_state.projection_matrix;
	const auto view = cam_state.view_matrix;
	const auto inv_proj = proj.inverse();
	const auto extent = graph.extent();

	const auto& light_alloc = d.light_buffers[frame_index];

	std::array<shaders::forward::light, max_lights> lights{};
	const auto light_count = light_packing::pack(lights, view, atm_state, dir_lights, spot_lights, point_lights, transforms);

	if (light_count > 0) {
		light_alloc.host_write(lights.data(), light_count * sizeof(shaders::forward::light));
	}

	const culling_params_data params{
		.projection = proj,
		.inv_proj = inv_proj,
		.screen_size = vec2u{ extent.x(), extent.y() },
		.num_lights = static_cast<std::uint32_t>(light_count),
	};
	d.culling_params_buffers[frame_index].host_write(params);

	const auto tiles = tile_count(d.current_width, d.current_height);

	auto rec = co_await gpu::pass<^^frame>(pass_out).pipeline(d.pipeline).after<^^depth_prepass::frame>();

	rec.sample_image(gpu_s.render_graph->depth_image(), gpu::pipeline_stage_flag::compute_shader);

	rec.dispatch<entry>(
		{
			.depth_texture = d.depth_view.slot(),
			.culling_params = d.culling_params_buffers[frame_index].slot(),
			.lights = d.light_buffers[frame_index].slot(),
			.light_index_list = gpu_s.device->buffer_slot(d.light_index_list_buffers[frame_index]),
			.tile_light_table = gpu_s.device->buffer_slot(d.tile_light_table_buffers[frame_index]),
			.depth_sampler = d.depth_sampler.slot(),
		},
		vec3u{ tiles.x(), tiles.y(), 1u }
	);
}
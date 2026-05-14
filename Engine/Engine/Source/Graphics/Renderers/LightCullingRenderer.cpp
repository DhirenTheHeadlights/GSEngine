module gse.graphics;

import std;

import :light_culling_renderer;
import :geometry_collector;
import :point_light;
import :spot_light;
import :directional_light;
import :camera_system;
import :depth_prepass_renderer;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.shader;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

auto gse::renderer::light_culling::system::tile_count(const state& s) -> vec2u {
	return {
		(s.current_extent.x() + tile_size - 1) / tile_size,
		(s.current_extent.y() + tile_size - 1) / tile_size
	};
}

auto gse::renderer::light_culling::system::update_depth_descriptor(const gpu::context::state& gpu_s, resources& r) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("light_culling"), r.descriptors[i])
			.image("g_depth", gpu_s.render_graph->depth_image(), r.depth_sampler, gpu::image_layout::general)
			.commit();
	}
}

auto gse::renderer::light_culling::system::rebuild_tile_buffers(const gpu::context::state& gpu_s, resources& r, state& s) -> void {
	const auto ext = gpu_s.render_graph->extent();
	s.current_extent = ext;

	const auto tiles = tile_count(s);
	const std::uint32_t total_tiles = tiles.x() * tiles.y();
	const std::uint32_t index_list_size = total_tiles * max_lights_per_tile * sizeof(std::uint32_t);
	const std::uint32_t tile_table_size = total_tiles * 2 * sizeof(std::uint32_t);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.light_index_list_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = index_list_size,
			.usage = gpu::buffer_flag::storage
		});

		r.tile_light_table_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = tile_table_size,
			.usage = gpu::buffer_flag::storage
		});

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("light_culling"), r.descriptors[i])
			.buffer("culling_params", r.culling_params_buffers[i], 0, sizeof(shaders::light_culling::culling_params_data))
			.buffer("lights", r.light_buffers[i], 0, sizeof(shaders::forward::light) * max_lights)
			.buffer("light_index_list", r.light_index_list_buffers[i], 0, index_list_size)
			.buffer("tile_light_table", r.tile_light_table_buffers[i], 0, tile_table_size)
			.commit();
	}

	update_depth_descriptor(gpu_s, r);
}

auto gse::renderer::light_culling::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, resources& r, frame_data& fd, state& s) -> async::task<> {
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), std::string_view("light_culling"));
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.culling_params_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = sizeof(shaders::light_culling::culling_params_data),
			.usage = gpu::buffer_flag::uniform
		});

		r.light_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = sizeof(shaders::forward::light) * max_lights,
			.usage = gpu::buffer_flag::storage
		});
	}

	r.depth_sampler = gpu::sampler::create(gpu_s.device->allocator(), {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
		.border = gpu::border_color::float_opaque_white,
		.max_lod = 1.0f
	});

	r.pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, shaders::light_culling::entry::pod);

	rebuild_tile_buffers(gpu_s, r, s);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &r, &s]() {
		rebuild_tile_buffers(gpu_s, r, s);
	});

	co_return;
}

auto gse::renderer::light_culling::system::frame(frame_context& ctx, const gpu::context::state& gpu_s, const resources& r, frame_data& fd, const state& s, const camera::system::state& cam_state) -> async::task<> {
	auto& graph = *gpu_s.render_graph;

	if (!graph.frame_in_progress()) {
		co_return;
	}

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto frame_index = graph.current_frame();

	const auto proj = cam_state.projection_matrix;
	const auto view = cam_state.view_matrix;
	const auto inv_proj = proj.inverse();
	const auto extent = graph.extent();

	const auto dir_chunk = ctx.components<directional_light_component>();
	const auto spot_chunk = ctx.components<spot_light_component>();
	const auto point_chunk = ctx.components<point_light_component>();

	const auto& light_alloc = r.light_buffers[frame_index];

	std::array<shaders::forward::light, max_lights> lights{};
	std::size_t light_count = 0;

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		lights[light_count] = {
			.light_type = shaders::forward::light_type::directional,
			.direction = view.transform_direction(comp.direction),
			.world_direction = comp.direction,
			.color = comp.color,
			.intensity = comp.intensity,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	for (const auto& comp : spot_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		lights[light_count] = {
			.light_type = shaders::forward::light_type::spot,
			.position = view.transform_point(comp.position),
			.direction = view.transform_direction(comp.direction),
			.world_position = comp.position,
			.world_direction = comp.direction,
			.color = comp.color,
			.intensity = comp.intensity,
			.constant = comp.constant,
			.linear = comp.linear,
			.quadratic = comp.quadratic,
			.cut_off = gse::cos(comp.cut_off),
			.outer_cut_off = gse::cos(comp.outer_cut_off),
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		lights[light_count] = {
			.light_type = shaders::forward::light_type::point,
			.position = view.transform_point(comp.position),
			.world_position = comp.position,
			.color = comp.color,
			.intensity = comp.intensity,
			.constant = comp.constant,
			.linear = comp.linear,
			.quadratic = comp.quadratic,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	if (light_count > 0) {
		gse::memcpy(light_alloc.mapped(), lights.data(), light_count * sizeof(shaders::forward::light));
	}

	const shaders::light_culling::culling_params_data params{
		.projection = proj,
		.inv_proj = inv_proj,
		.screen_size = vec2u{ extent.x(), extent.y() },
		.num_lights = static_cast<std::uint32_t>(light_count),
	};
	gse::memcpy(r.culling_params_buffers[frame_index].mapped(), &params, sizeof(params));

	const auto tiles = tile_count(s);

	auto rec = co_await gpu::pass<state>(ctx)
		.after<depth_prepass::system>()
		.reads(gpu::sampled(graph.depth_image(), gpu::pipeline_stage::compute_shader))
		.writes(
			gpu::storage_write(r.tile_light_table_buffers[frame_index], gpu::pipeline_stage::compute_shader),
			gpu::storage_write(r.light_index_list_buffers[frame_index], gpu::pipeline_stage::compute_shader)
		)
		.tracks(r.culling_params_buffers[frame_index], r.light_buffers[frame_index]);

	rec.bind(r.pipeline);
	rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
	rec.dispatch(tiles.x(), tiles.y(), 1);
}

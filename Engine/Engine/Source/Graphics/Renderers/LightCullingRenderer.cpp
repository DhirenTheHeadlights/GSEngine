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
		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.descriptors[i])
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

	const auto light_block = r.shader_handle->uniform_block("lights");
	const auto params_block = r.shader_handle->uniform_block("CullingParams");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.light_index_list_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = index_list_size,
			.usage = gpu::buffer_flag::storage
		});

		r.tile_light_table_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = tile_table_size,
			.usage = gpu::buffer_flag::storage
		});

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.descriptors[i])
			.buffer("CullingParams", r.culling_params_buffers[i], 0, params_block.size)
			.buffer("lights", r.light_buffers[i], 0, light_block.size * max_lights)
			.buffer("light_index_list", r.light_index_list_buffers[i], 0, index_list_size)
			.buffer("tile_light_table", r.tile_light_table_buffers[i], 0, tile_table_size)
			.commit();
	}

	update_depth_descriptor(gpu_s, r);
}

auto gse::renderer::light_culling::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, resources& r, frame_data& fd, state& s) -> async::task<> {
	r.shader_handle = co_await asset::load<shader>(ctx, "Shaders/Compute/light_culling");
	assert(r.shader_handle->is_compute(), "Shader for light culling system must be a compute shader");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);
	}

	const auto params_block = r.shader_handle->uniform_block("CullingParams");
	const auto light_block = r.shader_handle->uniform_block("lights");
	fd.light_staging.reserve(light_block.size * max_lights);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.culling_params_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = params_block.size,
			.usage = gpu::buffer_flag::uniform
		});

		r.light_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = light_block.size * max_lights,
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

	r.pipeline = gpu::create_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, r.shader_handle);

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

	const auto& params_alloc = r.culling_params_buffers[frame_index];
	r.shader_handle->set_uniform(params_alloc.bytes(), "CullingParams.projection", proj);
	r.shader_handle->set_uniform(params_alloc.bytes(), "CullingParams.inv_proj", inv_proj);

	const auto extent = graph.extent();
	const std::array screen_size_arr = { extent.x(), extent.y() };
	r.shader_handle->set_uniform(params_alloc.bytes(), "CullingParams.screen_size", screen_size_arr);

	const auto dir_chunk = ctx.components<directional_light_component>();
	const auto spot_chunk = ctx.components<spot_light_component>();
	const auto point_chunk = ctx.components<point_light_component>();

	const auto& light_alloc = r.light_buffers[frame_index];
	const auto light_block = r.shader_handle->uniform_block("lights");
	const auto stride = light_block.size;

	const std::size_t total_lights = std::min(
		dir_chunk.size() + spot_chunk.size() + point_chunk.size(),
		max_lights
	);
	auto& staging = fd.light_staging;
	staging.assign(total_lights * stride, std::byte{ 0 });
	std::size_t light_count = 0;

	auto write = [&](const std::size_t index, const std::string_view member, const auto& v) {
		gse::memcpy(staging.data() + index * stride + light_block.members.at(std::string(member)).offset, v);
	};

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		int type = 0;
		write(light_count, "light_type", type);
		write(light_count, "direction", view.transform_direction(comp.direction));
		write(light_count, "world_direction", comp.direction);
		write(light_count, "color", comp.color);
		write(light_count, "intensity", comp.intensity);
		write(light_count, "ambient_strength", comp.ambient_strength);
		write(light_count, "source_radius", comp.source_radius);
		++light_count;
	}

	for (const auto& comp : spot_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		int type = 2;
		const float cut_off_cos = gse::cos(comp.cut_off);
		const float outer_cut_off_cos = gse::cos(comp.outer_cut_off);
		write(light_count, "light_type", type);
		write(light_count, "position", view.transform_point(comp.position));
		write(light_count, "direction", view.transform_direction(comp.direction));
		write(light_count, "world_position", comp.position);
		write(light_count, "world_direction", comp.direction);
		write(light_count, "color", comp.color);
		write(light_count, "intensity", comp.intensity);
		write(light_count, "constant", comp.constant);
		write(light_count, "linear", comp.linear);
		write(light_count, "quadratic", comp.quadratic);
		write(light_count, "cut_off", cut_off_cos);
		write(light_count, "outer_cut_off", outer_cut_off_cos);
		write(light_count, "ambient_strength", comp.ambient_strength);
		write(light_count, "source_radius", comp.source_radius);
		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		int type = 1;
		write(light_count, "light_type", type);
		write(light_count, "position", view.transform_point(comp.position));
		write(light_count, "world_position", comp.position);
		write(light_count, "color", comp.color);
		write(light_count, "intensity", comp.intensity);
		write(light_count, "constant", comp.constant);
		write(light_count, "linear", comp.linear);
		write(light_count, "quadratic", comp.quadratic);
		write(light_count, "ambient_strength", comp.ambient_strength);
		write(light_count, "source_radius", comp.source_radius);
		++light_count;
	}

	if (light_count > 0) {
		gse::memcpy(light_alloc.mapped(), staging.data(), light_count * stride);
	}

	const std::uint32_t num_lights = static_cast<std::uint32_t>(light_count);
	r.shader_handle->set_uniform(params_alloc.bytes(), "CullingParams.num_lights", num_lights);

	const auto tiles = tile_count(s);

	auto& rec = co_await gpu::pass<state>(ctx)
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

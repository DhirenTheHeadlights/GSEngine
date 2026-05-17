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

namespace gse::renderer::light_culling {
	struct[[= shaders::shader_struct]] culling_params_data {
		mat4f projection;
		mat4f inv_proj;
		vec2u screen_size;
		std::uint32_t num_lights;
		std::uint32_t depth_index;
	};

	struct[[= shaders::binding<0, 1>{}]] culling_params {
		using element = culling_params_data;
	};

	struct[[= shaders::binding<0, 2>{}, = shaders::ssbo_readonly]] lights {
		using element = shaders::forward::light;
	};

	struct[[= shaders::binding<0, 3>{}, = shaders::ssbo_readwrite]] light_index_list {
		using element = std::uint32_t;
	};

	struct[[= shaders::binding<0, 4>{}, = shaders::ssbo_readwrite]] tile_light_table {
		using element = vec2u;
	};

	using shader_binding_types = type_pack<
		culling_params,
		lights,
		light_index_list,
		tile_light_table,
		shaders::bindless::textures>;

	using shader_types = type_pack<culling_params_data>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/light_culling">,
		gpu::layout<"light_culling">,
		gpu::types<shaders::forward::shader_types, shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<16, 16, 1>,
		gpu::system_values<gpu::group_id, gpu::group_thread_id, gpu::group_index>>;
}

auto gse::renderer::light_culling::system::tile_count(const data& d) -> vec2u {
	return {
		(d.current_extent.x() + tile_size - 1) / tile_size,
		(d.current_extent.y() + tile_size - 1) / tile_size
	};
}

auto gse::renderer::light_culling::system::update_depth_descriptor(const gpu::context::data& gpu_s, data& d) -> void {
	if (d.depth_slot) {
		gpu_s.bindless_textures->release(d.depth_slot);
	}
	d.depth_slot = gpu_s.bindless_textures->allocate(
		gpu_s.render_graph->depth_image().view(),
		d.depth_sampler.native()
	);
}

auto gse::renderer::light_culling::system::rebuild_tile_buffers(const gpu::context::data& gpu_s, data& d) -> void {
	const auto ext = gpu_s.render_graph->extent();
	d.current_extent = ext;

	const auto tiles = tile_count(d);
	const std::uint32_t total_tiles = tiles.x() * tiles.y();
	const std::uint32_t index_list_size = total_tiles * max_lights_per_tile * sizeof(std::uint32_t);
	const std::uint32_t tile_table_size = total_tiles * 2 * sizeof(std::uint32_t);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.light_index_list_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = index_list_size, .usage = gpu::buffer_flag::storage });

		d.tile_light_table_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = tile_table_size, .usage = gpu::buffer_flag::storage });

		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.descriptors[i])
			.buffer<culling_params>(d.culling_params_buffers[i], 0, sizeof(culling_params_data))
			.buffer<lights>(d.light_buffers[i], 0, sizeof(shaders::forward::light) * max_lights)
			.buffer<light_index_list>(d.light_index_list_buffers[i], 0, index_list_size)
			.buffer<tile_light_table>(d.tile_light_table_buffers[i], 0, tile_table_size)
			.commit();
	}

	update_depth_descriptor(gpu_s, d);
}

auto gse::renderer::light_culling::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.culling_params_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = sizeof(culling_params_data), .usage = gpu::buffer_flag::uniform });

		d.light_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = sizeof(shaders::forward::light) * max_lights, .usage = gpu::buffer_flag::storage });
	}

	d.depth_sampler = gpu::sampler::create(gpu_s.device->allocator(), { .min = gpu::sampler_filter::nearest, .mag = gpu::sampler_filter::nearest, .address_u = gpu::sampler_address_mode::clamp_to_edge, .address_v = gpu::sampler_address_mode::clamp_to_edge, .address_w = gpu::sampler_address_mode::clamp_to_edge, .border = gpu::border_color::float_opaque_white, .max_lod = 1.0f });

	rebuild_tile_buffers(gpu_s, d);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &d]() {
		rebuild_tile_buffers(gpu_s, d);
	});

	co_return;
}

auto gse::renderer::light_culling::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, const data& d, shared_view<camera::system> cam_state) -> async::task<> {
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

	const auto& light_alloc = d.light_buffers[frame_index];

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
		light_alloc.host_write(lights.data(), light_count * sizeof(shaders::forward::light));
	}

	const culling_params_data params{
		.projection = proj,
		.inv_proj = inv_proj,
		.screen_size = vec2u{ extent.x(), extent.y() },
		.num_lights = static_cast<std::uint32_t>(light_count),
		.depth_index = d.depth_slot ? d.depth_slot.index : shaders::bindless::invalid_index,
	};
	d.culling_params_buffers[frame_index].host_write(params);

	const auto tiles = tile_count(d);

	auto rec = co_await gpu::pass<system>(ctx)
				   .pipeline(d.pipeline)
				   .after<depth_prepass::system>();

	rec.sample_image(gpu_s.render_graph->depth_image(), gpu::pipeline_stage_flag::compute_shader);
	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.dispatch(tiles.x(), tiles.y(), 1);
}

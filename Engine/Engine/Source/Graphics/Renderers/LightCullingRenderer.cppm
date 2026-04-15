export module gse.graphics:light_culling_renderer;

import std;

import :point_light;
import :spot_light;
import :directional_light;
import :camera_system;
import :depth_prepass_renderer;
import gse.platform;
import gse.utility;
export namespace gse::renderer::light_culling {
	constexpr std::uint32_t tile_size = 16;
	constexpr std::uint32_t max_lights_per_tile = 64;
	constexpr std::size_t max_lights = 1024;

	struct state {
		vec2u current_extent{};

		auto tile_count() const -> vec2u {
			return {
				(current_extent.x() + tile_size - 1) / tile_size,
				(current_extent.y() + tile_size - 1) / tile_size
			};
		}
	};

	struct system {
		struct resources {
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;

			resource::handle<shader> shader_handle;

			per_frame_resource<gpu::buffer> culling_params_buffers;
			per_frame_resource<gpu::buffer> light_buffers;
			per_frame_resource<gpu::buffer> light_index_list_buffers;
			per_frame_resource<gpu::buffer> tile_light_table_buffers;

			gpu::sampler depth_sampler;

			gpu::context* ctx = nullptr;

			auto light_index_list(const std::uint32_t frame) const -> const gpu::buffer& {
				return light_index_list_buffers[frame];
			}

			auto tile_light_table(const std::uint32_t frame) const -> const gpu::buffer& {
				return tile_light_table_buffers[frame];
			}
		};

		struct frame_data {
			linear_vector<std::byte> light_staging;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			frame_data& fd,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			const resources& r,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};
}

namespace gse::renderer::light_culling {
	auto update_depth_descriptor(system::resources& r) -> void;
	auto rebuild_tile_buffers(system::resources& r, state& s) -> void;
}

auto gse::renderer::light_culling::update_depth_descriptor(system::resources& r) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer(r.ctx->device_ref(), r.shader_handle, r.descriptors[i])
			.image("g_depth", r.ctx->graph().depth_image(), r.depth_sampler, gpu::image_layout::general)
			.commit();
	}
}

auto gse::renderer::light_culling::rebuild_tile_buffers(system::resources& r, state& s) -> void {
	const auto ext = r.ctx->graph().extent();
	s.current_extent = ext;

	const auto tiles = s.tile_count();
	const std::uint32_t total_tiles = tiles.x() * tiles.y();
	const std::uint32_t index_list_size = total_tiles * max_lights_per_tile * sizeof(std::uint32_t);
	const std::uint32_t tile_table_size = total_tiles * 2 * sizeof(std::uint32_t);

	const auto light_block = r.shader_handle->uniform_block("lights");
	const auto params_block = r.shader_handle->uniform_block("CullingParams");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.light_index_list_buffers[i] = gpu::create_buffer(r.ctx->device_ref(), {
			.size = index_list_size,
			.usage = gpu::buffer_flag::storage
		});

		r.tile_light_table_buffers[i] = gpu::create_buffer(r.ctx->device_ref(), {
			.size = tile_table_size,
			.usage = gpu::buffer_flag::storage
		});

		gpu::descriptor_writer(r.ctx->device_ref(), r.shader_handle, r.descriptors[i])
			.buffer("CullingParams", r.culling_params_buffers[i], 0, params_block.size)
			.buffer("lights", r.light_buffers[i], 0, light_block.size * max_lights)
			.buffer("light_index_list", r.light_index_list_buffers[i], 0, index_list_size)
			.buffer("tile_light_table", r.tile_light_table_buffers[i], 0, tile_table_size)
			.commit();
	}

	update_depth_descriptor(r);
}

auto gse::renderer::light_culling::system::initialize(const init_context& phase, resources& r, frame_data& fd, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	r.ctx = &ctx;

	r.shader_handle = ctx.get<shader>("Shaders/Compute/light_culling");
	ctx.instantly_load(r.shader_handle);
	assert(r.shader_handle->is_compute(), std::source_location::current(), "Shader for light culling system must be a compute shader");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *r.shader_handle);
	}

	const auto params_block = r.shader_handle->uniform_block("CullingParams");
	const auto light_block = r.shader_handle->uniform_block("lights");
	fd.light_staging.reserve(light_block.size * max_lights);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.culling_params_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = params_block.size,
			.usage = gpu::buffer_flag::uniform
		});

		r.light_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = light_block.size * max_lights,
			.usage = gpu::buffer_flag::storage
		});
	}

	r.depth_sampler = gpu::create_sampler(ctx.device_ref(), {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
		.border = gpu::border_color::float_opaque_white,
		.max_lod = 1.0f
	});

	r.pipeline = gpu::create_compute_pipeline(ctx.device_ref(), *r.shader_handle);

	rebuild_tile_buffers(r, s);

	ctx.on_swap_chain_recreate([&r, &s]() {
		rebuild_tile_buffers(r, s);
	});
}

auto gse::renderer::light_culling::system::frame(frame_context& ctx, const resources& r, frame_data& fd, const state& s) -> async::task<> {
	co_await ctx.after<geometry_collector::state>();

	auto& gpu = ctx.get<gpu::context>();
	auto& graph = gpu.graph();

	if (!graph.frame_in_progress()) {
		co_return;
	}

	const auto frame_index = graph.current_frame();

	const auto* cam_state = ctx.try_state_of<camera::state>();
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto inv_proj = proj.inverse();

	const auto& params_alloc = r.culling_params_buffers[frame_index];
	r.shader_handle->set_uniform("CullingParams.projection", proj, params_alloc);
	r.shader_handle->set_uniform("CullingParams.inv_proj", inv_proj, params_alloc);

	const auto extent = graph.extent();
	const std::array screen_size_arr = { extent.x(), extent.y() };
	r.shader_handle->set_uniform("CullingParams.screen_size", screen_size_arr, params_alloc);

	const auto dir_chunk = ctx.reg.linked_objects_read<directional_light_component>();
	const auto spot_chunk = ctx.reg.linked_objects_read<spot_light_component>();
	const auto point_chunk = ctx.reg.linked_objects_read<point_light_component>();

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
		if (light_count >= max_lights) break;
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
		if (light_count >= max_lights) break;
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
		if (light_count >= max_lights) break;
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

	gse::memcpy(light_alloc.mapped(), staging.data(), light_count * stride);

	const std::uint32_t num_lights = static_cast<std::uint32_t>(light_count);
	r.shader_handle->set_uniform("CullingParams.num_lights", num_lights, params_alloc);

	const auto tiles = s.tile_count();

	auto pass = graph.add_pass<state>();
	pass.after<depth_prepass::state>();

	pass.track(r.culling_params_buffers[frame_index]);
	pass.track(r.light_buffers[frame_index]);

	pass.reads(gpu::sampled(graph.depth_image(), gpu::pipeline_stage::compute_shader))
		.writes(
			gpu::storage_write(r.tile_light_table_buffers[frame_index], gpu::pipeline_stage::compute_shader),
			gpu::storage_write(r.light_index_list_buffers[frame_index], gpu::pipeline_stage::compute_shader)
		)
		.record([&r, frame_index, tiles](const gpu::recording_context& rec) {
			rec.bind(r.pipeline);
			rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
			rec.dispatch(tiles.x(), tiles.y(), 1);
		});
}

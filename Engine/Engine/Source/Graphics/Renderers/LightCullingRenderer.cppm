export module gse.graphics:light_culling_renderer;

import std;

import :point_light;
import :spot_light;
import :directional_light;
import :camera_system;
import :depth_prepass_renderer;
import :shadow_data;

import gse.platform;
import gse.utility;

export namespace gse::renderer::light_culling {
	constexpr std::uint32_t tile_size = 16;
	constexpr std::uint32_t max_lights_per_tile = 64;

	struct state {
		gpu::pipeline pipeline;
		per_frame_resource<vulkan::descriptor_region> descriptors;

		resource::handle<shader> shader_handle;

		per_frame_resource<gpu::buffer> culling_params_buffers;
		per_frame_resource<gpu::buffer> light_buffers;
		per_frame_resource<gpu::buffer> light_index_list_buffers;
		per_frame_resource<gpu::buffer> tile_light_table_buffers;

		gpu::sampler depth_sampler;

		vec2u current_extent{};
		gpu::context* ctx = nullptr;

		state() = default;

		auto tile_count() const -> vec2u {
			return {
				(current_extent.x() + tile_size - 1) / tile_size,
				(current_extent.y() + tile_size - 1) / tile_size
			};
		}

		auto light_index_list_buffer(std::uint32_t frame) const -> vk::Buffer {
			return light_index_list_buffers[frame].native().buffer;
		}

		auto tile_light_table_buffer(std::uint32_t frame) const -> vk::Buffer {
			return tile_light_table_buffers[frame].native().buffer;
		}

		auto light_index_list_size(std::uint32_t frame) const -> vk::DeviceSize {
			return light_index_list_buffers[frame].size();
		}

		auto tile_light_table_size(std::uint32_t frame) const -> vk::DeviceSize {
			return tile_light_table_buffers[frame].size();
		}
	};

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto render(const render_phase& phase, const state& s) -> void;
	};
}

namespace gse::renderer::light_culling {
	auto update_depth_descriptor(state& s) -> void;
	auto rebuild_tile_buffers(state& s) -> void;
}

auto gse::renderer::light_culling::update_depth_descriptor(state& s) -> void {
	const vk::DescriptorImageInfo depth_info{
		.sampler = s.depth_sampler.native(),
		.imageView = s.ctx->graph().depth_image_view(),
		.imageLayout = vk::ImageLayout::eGeneral
	};

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		const std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {
			{ "g_depth", depth_info }
		};

		gpu::write_descriptors(*s.ctx, s.descriptors[i], *s.shader_handle, {}, image_infos);
	}
}

auto gse::renderer::light_culling::rebuild_tile_buffers(state& s) -> void {
	const auto ext = s.ctx->graph().extent();
	s.current_extent = ext;

	const auto tiles = s.tile_count();
	const std::uint32_t total_tiles = tiles.x() * tiles.y();
	const vk::DeviceSize index_list_size = total_tiles * max_lights_per_tile * sizeof(std::uint32_t);
	const vk::DeviceSize tile_table_size = total_tiles * 2 * sizeof(std::uint32_t);

	const auto light_block = s.shader_handle->uniform_block("lights");
	const auto params_block = s.shader_handle->uniform_block("CullingParams");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		s.light_index_list_buffers[i] = gpu::create_buffer(*s.ctx, {
			.size = index_list_size,
			.usage = gpu::buffer_usage::storage
		});

		s.tile_light_table_buffers[i] = gpu::create_buffer(*s.ctx, {
			.size = tile_table_size,
			.usage = gpu::buffer_usage::storage
		});

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos = {
			{
				"CullingParams",
				{
					.buffer = s.culling_params_buffers[i].native().buffer,
					.offset = 0,
					.range = params_block.size
				}
			},
			{
				"lights",
				{
					.buffer = s.light_buffers[i].native().buffer,
					.offset = 0,
					.range = light_block.size
				}
			},
			{
				"light_index_list",
				{
					.buffer = s.light_index_list_buffers[i].native().buffer,
					.offset = 0,
					.range = index_list_size
				}
			},
			{
				"tile_light_table",
				{
					.buffer = s.tile_light_table_buffers[i].native().buffer,
					.offset = 0,
					.range = tile_table_size
				}
			}
		};

		gpu::write_descriptors(*s.ctx, s.descriptors[i], *s.shader_handle, buffer_infos);
	}

	update_depth_descriptor(s);
}

auto gse::renderer::light_culling::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	s.ctx = &ctx;

	s.shader_handle = ctx.get<shader>("Shaders/Compute/light_culling");
	ctx.instantly_load(s.shader_handle);
	assert(s.shader_handle->is_compute(), std::source_location::current(), "Shader for light culling system must be a compute shader");

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		s.descriptors[i] = gpu::allocate_descriptors(ctx, *s.shader_handle);
	}

	const auto params_block = s.shader_handle->uniform_block("CullingParams");
	const auto light_block = s.shader_handle->uniform_block("lights");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		s.culling_params_buffers[i] = gpu::create_buffer(ctx, {
			.size = params_block.size,
			.usage = gpu::buffer_usage::uniform
		});

		s.light_buffers[i] = gpu::create_buffer(ctx, {
			.size = light_block.size,
			.usage = gpu::buffer_usage::storage
		});
	}

	s.depth_sampler = gpu::create_sampler(ctx, {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
		.border = gpu::border_color::float_opaque_white,
		.max_lod = 1.0f
	});

	s.pipeline = gpu::create_compute_pipeline(ctx, *s.shader_handle);

	rebuild_tile_buffers(s);

	ctx.on_swap_chain_recreate([&s]() {
		rebuild_tile_buffers(s);
	});
}

auto gse::renderer::light_culling::system::render(const render_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& graph = ctx.graph();

	if (!graph.frame_in_progress()) {
		return;
	}

	const auto frame_index = graph.current_frame();

	const auto* cam_state = phase.try_state_of<camera::state>();
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto inv_proj = proj.inverse();

	const auto& params_alloc = s.culling_params_buffers[frame_index].native().allocation;
	s.shader_handle->set_uniform("CullingParams.projection", proj, params_alloc);
	s.shader_handle->set_uniform("CullingParams.inv_proj", inv_proj, params_alloc);

	const auto extent = graph.extent();
	const std::array screen_size_arr = { extent.x(), extent.y() };
	s.shader_handle->set_uniform("CullingParams.screen_size", screen_size_arr, params_alloc);

	const auto dir_chunk = phase.registry.view<directional_light_component>();
	const auto spot_chunk = phase.registry.view<spot_light_component>();
	const auto point_chunk = phase.registry.view<point_light_component>();

	const auto& light_alloc = s.light_buffers[frame_index].native().allocation;
	const auto light_block = s.shader_handle->uniform_block("lights");
	const auto stride = light_block.size;
	std::vector zero_elem(stride, std::byte{ 0 });

	auto zero_at = [&](const std::size_t index) {
		s.shader_handle->set_ssbo_struct(
			"lights",
			index,
			std::span<const std::byte>(zero_elem.data(), zero_elem.size()),
			light_alloc
		);
	};

	auto set = [&](const std::size_t index, const std::string_view member, auto const& v) {
		s.shader_handle->set_ssbo_element(
			"lights",
			index,
			member,
			std::as_bytes(std::span(std::addressof(v), 1)),
			light_alloc
		);
	};

	std::size_t light_count = 0;

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_shadow_lights) break;
		zero_at(light_count);
		int type = 0;
		set(light_count, "light_type", type);
		set(light_count, "direction", view.transform_direction(comp.direction));
		set(light_count, "color", comp.color);
		set(light_count, "intensity", comp.intensity);
		set(light_count, "ambient_strength", comp.ambient_strength);
		++light_count;
	}

	for (const auto& comp : spot_chunk) {
		if (light_count >= max_shadow_lights) break;
		zero_at(light_count);
		int type = 2;
		const float cut_off_cos = gse::cos(comp.cut_off);
		const float outer_cut_off_cos = gse::cos(comp.outer_cut_off);
		set(light_count, "light_type", type);
		set(light_count, "position", view.transform_point(comp.position));
		set(light_count, "direction", view.transform_direction(comp.direction));
		set(light_count, "color", comp.color);
		set(light_count, "intensity", comp.intensity);
		set(light_count, "constant", comp.constant);
		set(light_count, "linear", comp.linear);
		set(light_count, "quadratic", comp.quadratic);
		set(light_count, "cut_off", cut_off_cos);
		set(light_count, "outer_cut_off", outer_cut_off_cos);
		set(light_count, "ambient_strength", comp.ambient_strength);
		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_shadow_lights) break;
		zero_at(light_count);
		int type = 1;
		set(light_count, "light_type", type);
		set(light_count, "position", view.transform_point(comp.position));
		set(light_count, "color", comp.color);
		set(light_count, "intensity", comp.intensity);
		set(light_count, "constant", comp.constant);
		set(light_count, "linear", comp.linear);
		set(light_count, "quadratic", comp.quadratic);
		set(light_count, "ambient_strength", comp.ambient_strength);
		++light_count;
	}

	const std::uint32_t num_lights = static_cast<std::uint32_t>(light_count);
	s.shader_handle->set_uniform("CullingParams.num_lights", num_lights, params_alloc);

	const auto tiles = s.tile_count();
	const auto& depth_image = graph.depth_image();

	auto pass = graph.add_pass<state>();
	pass.after<depth_prepass::state>();

	pass.track(s.culling_params_buffers[frame_index].native());
	pass.track(s.light_buffers[frame_index].native());

	pass.reads(vulkan::sampled(depth_image, vk::PipelineStageFlagBits2::eComputeShader))
		.writes(
			vulkan::storage(s.tile_light_table_buffers[frame_index].native(), vk::PipelineStageFlagBits2::eComputeShader),
			vulkan::storage(s.light_index_list_buffers[frame_index].native(), vk::PipelineStageFlagBits2::eComputeShader)
		)
		.record([&s, frame_index, tiles](vulkan::recording_context& ctx) {
			ctx.bind_pipeline(vk::PipelineBindPoint::eCompute, s.pipeline);
			ctx.bind_descriptors(vk::PipelineBindPoint::eCompute, s.pipeline, s.descriptors[frame_index]);
			ctx.dispatch(tiles.x(), tiles.y(), 1);
		});
}

export module gse.graphics:forward_renderer;

import std;

import :geometry_collector;
import :depth_prepass_renderer;
import :shadow_data;
import :light_culling_renderer;
import :camera_system;
import :texture;
import :point_light;
import :spot_light;
import :directional_light;

import gse.math;
import gse.utility;
import gse.platform;

export namespace gse::renderer::forward {
	struct state {
		gpu::pipeline pipeline;
		per_frame_resource<vulkan::descriptor_region> descriptors;
		resource::handle<shader> shader_handle;

		gpu::pipeline skinned_pipeline;
		per_frame_resource<vulkan::descriptor_region> skinned_descriptors;
		resource::handle<shader> skinned_shader;

		per_frame_resource<gpu::buffer> light_buffers;
		per_frame_resource<gpu::buffer> shadow_params_buffers;
		per_frame_resource<gpu::buffer> point_shadow_params_buffers;

		gpu::sampler shadow_sampler;

		resource::handle<texture> blank_texture;

		gpu::image placeholder_depth;
		gpu::image placeholder_cube_depth;
		gpu::sampler placeholder_cube_sampler;

		std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;

		state() = default;
	};

	struct system {
		static auto initialize(
			initialize_phase& phase,
			state& s
		) -> void;

		static auto render(
			const render_phase& phase,
			const state& s
		) -> void;
	};
}

auto gse::renderer::forward::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.shader_handle = ctx.get<shader>("Shaders/Standard3D/meshlet_geometry");
	ctx.instantly_load(s.shader_handle);

	const auto camera_ubo = s.shader_handle->uniform_block("CameraUBO");
	const auto light_block = s.shader_handle->uniform_block("lights_ssbo");
	const auto shadow_block = s.shader_handle->uniform_block("ShadowParams");
	const auto point_shadow_block = s.shader_handle->uniform_block("PointShadowParams");

	s.shadow_sampler = gpu::create_sampler(ctx, {
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::clamp_to_border,
		.address_v = gpu::sampler_address_mode::clamp_to_border,
		.address_w = gpu::sampler_address_mode::clamp_to_border,
		.border = gpu::border_color::float_opaque_white,
		.max_lod = 1.0f
	});

	s.placeholder_depth = gpu::create_image(ctx, {
		.format = gpu::image_format::d32_sfloat,
		.view = gpu::image_view_type::e2d,
		.usage = gpu::image_flag::sampled | gpu::image_flag::depth_attachment,
		.ready_layout = gpu::image_layout::general
	});

	s.placeholder_cube_depth = gpu::create_image(ctx, {
		.format = gpu::image_format::d32_sfloat,
		.view = gpu::image_view_type::cube,
		.usage = gpu::image_flag::sampled | gpu::image_flag::depth_attachment,
		.ready_layout = gpu::image_layout::general
	});

	s.placeholder_cube_sampler = gpu::create_sampler(ctx, {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge
	});

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx, {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.light_buffers[i] = gpu::create_buffer(ctx, {
			.size = light_block.size,
			.usage = gpu::buffer_flag::storage
		});

		s.shadow_params_buffers[i] = gpu::create_buffer(ctx, {
			.size = shadow_block.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.point_shadow_params_buffers[i] = gpu::create_buffer(ctx, {
			.size = point_shadow_block.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.descriptors[i] = gpu::allocate_descriptors(ctx, *s.shader_handle);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].native().buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			},
			{
				"lights_ssbo",
				{
					.buffer = s.light_buffers[i].native().buffer,
					.offset = 0,
					.range = light_block.size
				}
			},
			{
				"ShadowParams",
				{
					.buffer = s.shadow_params_buffers[i].native().buffer,
					.offset = 0,
					.range = shadow_block.size
				}
			},
			{
				"PointShadowParams",
				{
					.buffer = s.point_shadow_params_buffers[i].native().buffer,
					.offset = 0,
					.range = point_shadow_block.size
				}
			}
		};

		gpu::write_descriptors(ctx, s.descriptors[i], *s.shader_handle, buffer_infos);
	}

	const auto* shadow_state = phase.try_state_of<shadow::state>();

	std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>> array_image_infos;
	std::vector<vk::DescriptorImageInfo> shadow_infos;
	shadow_infos.reserve(max_shadow_lights);

	for (std::size_t i = 0; i < max_shadow_lights; ++i) {
		shadow_infos.push_back({
			.sampler = s.shadow_sampler.native(),
			.imageView = shadow_state ? shadow_state->shadow_map_view(i) : s.placeholder_depth.native().view,
			.imageLayout = vk::ImageLayout::eGeneral
		});
	}
	array_image_infos.emplace("shadow_maps", std::move(shadow_infos));

	std::vector<vk::DescriptorImageInfo> point_shadow_infos;
	point_shadow_infos.reserve(max_point_shadow_lights);

	for (std::size_t i = 0; i < max_point_shadow_lights; ++i) {
		point_shadow_infos.push_back({
			.sampler = shadow_state ? shadow_state->point_shadow_sampler(i) : s.placeholder_cube_sampler.native(),
			.imageView = shadow_state ? shadow_state->point_shadow_cube_view(i) : s.placeholder_cube_depth.native().view,
			.imageLayout = vk::ImageLayout::eGeneral
		});
	}
	array_image_infos.emplace("point_shadow_maps", std::move(point_shadow_infos));

	const auto* lc_state = phase.try_state_of<light_culling::state>();

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		std::unordered_map<std::string, vk::DescriptorBufferInfo> tile_buffer_infos;
		if (lc_state) {
			const auto fi = static_cast<std::uint32_t>(i);
			tile_buffer_infos = {
				{
					"light_index_list",
					{
						.buffer = lc_state->light_index_list_buffer(fi),
						.offset = 0,
						.range = lc_state->light_index_list_size(fi)
					}
				},
				{
					"tile_light_table",
					{
						.buffer = lc_state->tile_light_table_buffer(fi),
						.offset = 0,
						.range = lc_state->tile_light_table_size(fi)
					}
				}
			};
		}

		gpu::write_descriptors(
			ctx,
			s.descriptors[i],
			*s.shader_handle,
			tile_buffer_infos,
			{},
			array_image_infos
		);
	}

	auto* gpu = &ctx;
	ctx.on_swap_chain_recreate([&s, lc_state, gpu]() {
		if (!lc_state) return;
		const auto* lc = lc_state;
		for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
			const auto fi = static_cast<std::uint32_t>(i);
			const std::unordered_map<std::string, vk::DescriptorBufferInfo> tile_buffer_infos = {
				{
					"light_index_list",
					{
						.buffer = lc->light_index_list_buffer(fi),
						.offset = 0,
						.range = lc->light_index_list_size(fi)
					}
				},
				{
					"tile_light_table",
					{
						.buffer = lc->tile_light_table_buffer(fi),
						.offset = 0,
						.range = lc->tile_light_table_size(fi)
					}
				}
			};
			gpu::write_descriptors(*gpu, s.descriptors[i], *s.shader_handle, tile_buffer_infos);
		}
	});

	s.pipeline = gpu::create_graphics_pipeline(ctx, *s.shader_handle, {
		.depth = { .test = true, .write = false, .compare = gpu::compare_op::less_or_equal },
		.push_constant_block = "push_constants"
	});

	s.skinned_shader = ctx.get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	ctx.instantly_load(s.skinned_shader);

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		s.skinned_descriptors[i] = gpu::allocate_descriptors(ctx, *s.skinned_shader);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> skinned_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].native().buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		gpu::write_descriptors(ctx, s.skinned_descriptors[i], *s.skinned_shader, skinned_buffer_infos);
	}

	s.skinned_pipeline = gpu::create_graphics_pipeline(ctx, *s.skinned_shader, {
		.depth = { .test = true, .write = false, .compare = gpu::compare_op::less_or_equal }
	});

	s.blank_texture = ctx.queue<texture>("blank", vec4f(1, 1, 1, 1));
	ctx.instantly_load(s.blank_texture);
}

auto gse::renderer::forward::system::render(const render_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	const auto& data = render_items[0];
	const auto frame_index = data.frame_index;

	if (!ctx.graph().frame_in_progress()) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};

	const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index].native().allocation;
	s.shader_handle->set_uniform("CameraUBO.view", view, cam_alloc);
	s.shader_handle->set_uniform("CameraUBO.proj", proj, cam_alloc);
	s.shader_handle->set_uniform("CameraUBO.inv_view", view.inverse(), cam_alloc);

	auto dir_chunk = phase.registry.view<directional_light_component>();
	auto spot_chunk = phase.registry.view<spot_light_component>();
	auto point_chunk = phase.registry.view<point_light_component>();

	const auto& light_alloc = s.light_buffers[frame_index].native().allocation;
	const auto light_block = s.shader_handle->uniform_block("lights_ssbo");
	const auto stride = light_block.size;
	std::vector zero_elem(stride, std::byte{ 0 });

	auto zero_at = [&](const std::size_t index) {
		s.shader_handle->set_ssbo_struct(
			"lights_ssbo", index,
			std::span<const std::byte>(zero_elem.data(), zero_elem.size()),
			light_alloc
		);
	};

	auto set_light = [&](const std::size_t index, const std::string_view member, auto const& v) {
		s.shader_handle->set_ssbo_element(
			"lights_ssbo", index, member,
			std::as_bytes(std::span(std::addressof(v), 1)),
			light_alloc
		);
	};

	std::size_t light_count = 0;

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_shadow_lights) break;
		zero_at(light_count);
		int type = 0;
		set_light(light_count, "light_type", type);
		set_light(light_count, "direction", view.transform_direction(comp.direction));
		set_light(light_count, "color", comp.color);
		set_light(light_count, "intensity", comp.intensity);
		set_light(light_count, "ambient_strength", comp.ambient_strength);
		++light_count;
	}

	for (const auto& comp : spot_chunk) {
		if (light_count >= max_shadow_lights) break;
		zero_at(light_count);
		int type = 2;
		const float cut_off_cos = gse::cos(comp.cut_off);
		const float outer_cut_off_cos = gse::cos(comp.outer_cut_off);
		set_light(light_count, "light_type", type);
		set_light(light_count, "position", view.transform_point(comp.position));
		set_light(light_count, "direction", view.transform_direction(comp.direction));
		set_light(light_count, "color", comp.color);
		set_light(light_count, "intensity", comp.intensity);
		set_light(light_count, "constant", comp.constant);
		set_light(light_count, "linear", comp.linear);
		set_light(light_count, "quadratic", comp.quadratic);
		set_light(light_count, "cut_off", cut_off_cos);
		set_light(light_count, "outer_cut_off", outer_cut_off_cos);
		set_light(light_count, "ambient_strength", comp.ambient_strength);
		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_shadow_lights) break;
		zero_at(light_count);
		int type = 1;
		set_light(light_count, "light_type", type);
		set_light(light_count, "position", view.transform_point(comp.position));
		set_light(light_count, "color", comp.color);
		set_light(light_count, "intensity", comp.intensity);
		set_light(light_count, "constant", comp.constant);
		set_light(light_count, "linear", comp.linear);
		set_light(light_count, "quadratic", comp.quadratic);
		set_light(light_count, "ambient_strength", comp.ambient_strength);
		++light_count;
	}

	const auto* shadow_state = phase.try_state_of<shadow::state>();
	vec2f texel_size{};
	std::span<const shadow_light_entry> shadow_entries{};
	std::span<const point_shadow_light_entry> point_shadow_entries{};
	const auto& shadow_items = phase.read_channel<shadow::render_data>();
	if (shadow_state && !shadow_items.empty()) {
		texel_size = shadow_state->shadow_texel_size();
		shadow_entries = shadow_items[0].lights;
		point_shadow_entries = shadow_items[0].point_lights;
	}

	const auto& shadow_alloc = s.shadow_params_buffers[frame_index].native().allocation;
	std::array<int, max_shadow_lights> shadow_indices{};
	std::array<view_projection_matrix, max_shadow_lights> shadow_view_proj{};
	const std::size_t shadow_light_count = std::min<std::size_t>(shadow_entries.size(), max_shadow_lights);

	for (std::size_t idx = 0; idx < shadow_light_count; ++idx) {
		shadow_indices[idx] = static_cast<int>(idx);
		shadow_view_proj[idx] = shadow_entries[idx].proj * shadow_entries[idx].view;
	}

	int shadow_count_i = static_cast<int>(shadow_light_count);
	std::unordered_map<std::string, std::span<const std::byte>> shadow_data;
	shadow_data.emplace("shadow_light_count", std::as_bytes(std::span(std::addressof(shadow_count_i), 1)));
	shadow_data.emplace("shadow_light_indices", std::as_bytes(std::span(shadow_indices.data(), shadow_indices.size())));
	shadow_data.emplace("shadow_view_proj", std::as_bytes(std::span(shadow_view_proj.data(), shadow_view_proj.size())));
	shadow_data.emplace("shadow_texel_size", std::as_bytes(std::span(std::addressof(texel_size), 1)));
	s.shader_handle->set_uniform_block("ShadowParams", shadow_data, shadow_alloc);

	const auto& point_shadow_alloc = s.point_shadow_params_buffers[frame_index].native().allocation;
	std::size_t dir_count = 0;
	for (const auto& comp : dir_chunk) {
		(void)comp; ++dir_count;
	}
	std::size_t spot_count = 0;
	for (const auto& comp : spot_chunk) {
		(void)comp; ++spot_count;
	}

	const std::size_t point_shadow_count = std::min<std::size_t>(point_shadow_entries.size(), max_point_shadow_lights);
	std::array<int, max_point_shadow_lights> ps_light_indices{};
	std::array<vec3<length>, max_point_shadow_lights> ps_positions{};
	std::array<length, max_point_shadow_lights> ps_near{};
	std::array<length, max_point_shadow_lights> ps_far{};

	for (std::size_t idx = 0; idx < point_shadow_count; ++idx) {
		const auto& entry = point_shadow_entries[idx];
		ps_light_indices[idx] = static_cast<int>(dir_count + spot_count + idx);
		ps_positions[idx] = entry.world_position;
		ps_near[idx] = entry.near_plane;
		ps_far[idx] = entry.far_plane;
	}

	int ps_count_i = static_cast<int>(point_shadow_count);
	std::unordered_map<std::string, std::span<const std::byte>> point_shadow_data;
	point_shadow_data.emplace("point_shadow_count", std::as_bytes(std::span(std::addressof(ps_count_i), 1)));
	point_shadow_data.emplace("point_shadow_light_indices", std::as_bytes(std::span(ps_light_indices.data(), ps_light_indices.size())));
	point_shadow_data.emplace("point_shadow_positions_world", std::as_bytes(std::span(ps_positions.data(), ps_positions.size())));
	point_shadow_data.emplace("point_shadow_near", std::as_bytes(std::span(ps_near.data(), ps_near.size())));
	point_shadow_data.emplace("point_shadow_far", std::as_bytes(std::span(ps_far.data(), ps_far.size())));
	s.shader_handle->set_uniform_block("PointShadowParams", point_shadow_data, point_shadow_alloc);

	const auto& normal_batches = data.normal_batches;
	const auto& skinned_batches = data.skinned_batches;

	const auto* gc_state = phase.try_state_of<geometry_collector::state>();
	const auto* lc_state = phase.try_state_of<light_culling::state>();
	if (!gc_state || !lc_state) {
		return;
	}

	const auto ext = ctx.graph().extent();
	const auto ext_w = ext.x();
	const auto ext_h = ext.y();
	const int num_lights_i = static_cast<int>(light_count);
	const std::array screen_sz = { ext_w, ext_h };

	auto pass = ctx.graph().add_pass<state>();
	pass.track(s.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.track(s.light_buffers[frame_index]);
	pass.track(s.shadow_params_buffers[frame_index]);
	pass.track(s.point_shadow_params_buffers[frame_index]);
	pass.track(gc_state->instance_buffer[frame_index].native());

	pass
		.after<light_culling::state>()
		.after<depth_prepass::state>()
		.reads(
			vulkan::storage_read(lc_state->tile_light_table_buffers[frame_index].native(), vk::PipelineStageFlagBits2::eFragmentShader),
			vulkan::storage_read(lc_state->light_index_list_buffers[frame_index].native(), vk::PipelineStageFlagBits2::eFragmentShader),
			vulkan::storage_read(gc_state->skin_buffer[frame_index].native(), vk::PipelineStageFlagBits2::eVertexShader),
			vulkan::indirect_read(gc_state->skinned_indirect_commands_buffer[frame_index].native(), vk::PipelineStageFlagBits2::eDrawIndirect)
		)
		.color_output(vulkan::color_clear{ 0.1f, 0.1f, 0.1f, 1.0f })
		.depth_output_load()
		.record([&s, &normal_batches, &skinned_batches, gc_state, frame_index, num_lights_i, screen_sz, ext_w, ext_h](vulkan::recording_context& ctx) {
			const vec2u ext_size{ ext_w, ext_h };
			ctx.set_viewport(ext_size);
			ctx.set_scissor(ext_size);

			if (!normal_batches.empty()) {
				const vk::DescriptorBufferInfo instance_buffer_info{
					.buffer = gc_state->instance_buffer[frame_index].native().buffer,
					.offset = 0,
					.range = gc_state->instance_buffer[frame_index].size()
				};

				bool pipeline_bound = false;

				for (std::size_t i = 0; i < normal_batches.size(); ++i) {
					const auto& batch = normal_batches[i];
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					if (!mesh.has_meshlets()) {
						continue;
					}

					const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
					const auto& tex_info = has_texture ? mesh.material()->diffuse_texture->descriptor_info() : s.blank_texture->descriptor_info();

					if (!pipeline_bound) {
						ctx.bind(s.pipeline);
						ctx.bind_descriptors(s.pipeline, s.descriptors[frame_index]);
						pipeline_bound = true;
					}

					const vk::DescriptorBufferInfo vertex_storage_info{
						.buffer = mesh.vertex_storage_buffer(),
						.offset = 0,
						.range = mesh.vertex_storage_buffer_size()
					};
					const vk::DescriptorBufferInfo meshlet_desc_info{
						.buffer = mesh.meshlet_descriptor_buffer(),
						.offset = 0,
						.range = mesh.meshlet_descriptor_buffer_size()
					};
					const vk::DescriptorBufferInfo meshlet_vert_info{
						.buffer = mesh.meshlet_vertex_buffer(),
						.offset = 0,
						.range = mesh.meshlet_vertex_buffer_size()
					};
					const vk::DescriptorBufferInfo meshlet_tri_info{
						.buffer = mesh.meshlet_triangle_buffer(),
						.offset = 0,
						.range = mesh.meshlet_triangle_buffer_size()
					};
					const vk::DescriptorBufferInfo meshlet_bounds_info{
						.buffer = mesh.meshlet_bounds_buffer(),
						.offset = 0,
						.range = mesh.meshlet_bounds_buffer_size()
					};

					const std::array<vk::WriteDescriptorSet, 7> descriptor_writes{
						vk::WriteDescriptorSet{
							.dstBinding = 0,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &vertex_storage_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 1,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &meshlet_desc_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 2,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &meshlet_vert_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 3,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &meshlet_tri_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 4,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &meshlet_bounds_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 5,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &instance_buffer_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 6,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eCombinedImageSampler,
							.pImageInfo = &tex_info
						}
					};

					ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, s.pipeline.native_layout(), 1, descriptor_writes);

					const std::uint32_t ml_count = mesh.meshlet_count();
					for (std::uint32_t inst = 0; inst < batch.instance_count; ++inst) {
						auto pc = s.shader_handle->cache_push_block("push_constants");
						pc.set("meshlet_offset", std::uint32_t(0));
						pc.set("meshlet_count", ml_count);
						pc.set("instance_index", batch.first_instance + inst);
						pc.set("num_lights", num_lights_i);
						pc.set("screen_size", screen_sz);
						ctx.push(s.pipeline, pc);

						const std::uint32_t task_groups = (ml_count + 31) / 32;
						ctx.draw_mesh_tasks(task_groups, 1, 1);
					}
				}
			}

			if (!skinned_batches.empty()) {
				ctx.bind(s.skinned_pipeline);
				ctx.bind_descriptors(s.skinned_pipeline, s.skinned_descriptors[frame_index]);

				const vk::DescriptorBufferInfo skin_buffer_info{
					.buffer = gc_state->skin_buffer[frame_index].native().buffer,
					.offset = 0,
					.range = gc_state->skin_buffer[frame_index].size()
				};

				const vk::DescriptorBufferInfo skinned_instance_buffer_info{
					.buffer = gc_state->instance_buffer[frame_index].native().buffer,
					.offset = 0,
					.range = gc_state->instance_buffer[frame_index].size()
				};

				for (std::size_t i = 0; i < skinned_batches.size(); ++i) {
					const auto& batch = skinned_batches[i];
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
					const auto& tex_info = has_texture ? mesh.material()->diffuse_texture->descriptor_info() : s.blank_texture->descriptor_info();

					const std::array<vk::WriteDescriptorSet, 3> skinned_descriptor_writes{
						vk::WriteDescriptorSet{
							.dstBinding = 2,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eCombinedImageSampler,
							.pImageInfo = &tex_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 3,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &skin_buffer_info
						},
						vk::WriteDescriptorSet{
							.dstBinding = 4,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = vk::DescriptorType::eStorageBuffer,
							.pBufferInfo = &skinned_instance_buffer_info
						}
					};

					ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, s.skinned_pipeline.native_layout(), 1, skinned_descriptor_writes);

					ctx.bind_vertex(mesh.vertex_gpu_buffer());
					ctx.bind_index(mesh.index_gpu_buffer());

					ctx.draw_indirect(
						gc_state->skinned_indirect_commands_buffer[frame_index],
						i * sizeof(vk::DrawIndexedIndirectCommand),
						1,
						0
					);
				}
			}
		});
}

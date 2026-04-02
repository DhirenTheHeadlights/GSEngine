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
		per_frame_resource<gpu::descriptor_region> descriptors;
		resource::handle<shader> shader_handle;

		gpu::pipeline skinned_pipeline;
		per_frame_resource<gpu::descriptor_region> skinned_descriptors;
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

	s.shadow_sampler = gpu::create_sampler(ctx.device_ref(), {
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::clamp_to_border,
		.address_v = gpu::sampler_address_mode::clamp_to_border,
		.address_w = gpu::sampler_address_mode::clamp_to_border,
		.border = gpu::border_color::float_opaque_white,
		.max_lod = 1.0f
	});

	s.placeholder_depth = gpu::create_image(ctx.device_ref(), {
		.format = gpu::image_format::d32_sfloat,
		.view = gpu::image_view_type::e2d,
		.usage = gpu::image_flag::sampled | gpu::image_flag::depth_attachment,
		.ready_layout = gpu::image_layout::general
	});

	s.placeholder_cube_depth = gpu::create_image(ctx.device_ref(), {
		.format = gpu::image_format::d32_sfloat,
		.view = gpu::image_view_type::cube,
		.usage = gpu::image_flag::sampled | gpu::image_flag::depth_attachment,
		.ready_layout = gpu::image_layout::general
	});

	s.placeholder_cube_sampler = gpu::create_sampler(ctx.device_ref(), {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge
	});

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx.device_ref(), {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.light_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = light_block.size,
			.usage = gpu::buffer_flag::storage
		});

		s.shadow_params_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = shadow_block.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.point_shadow_params_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = point_shadow_block.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.shader_handle);

		gpu::descriptor_writer(ctx.device_ref(), s.shader_handle, s.descriptors[i])
			.buffer("CameraUBO", s.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.buffer("lights_ssbo", s.light_buffers[i], 0, light_block.size)
			.buffer("ShadowParams", s.shadow_params_buffers[i], 0, shadow_block.size)
			.buffer("PointShadowParams", s.point_shadow_params_buffers[i], 0, point_shadow_block.size)
			.commit();
	}

	const auto* shadow_state = phase.try_state_of<shadow::state>();

	std::vector<const gpu::image*> shadow_images;
	shadow_images.reserve(max_shadow_lights);
	for (std::size_t i = 0; i < max_shadow_lights; ++i) {
		shadow_images.push_back(shadow_state ? &shadow_state->shadow_map(i) : &s.placeholder_depth);
	}

	std::vector<const gpu::image*> point_shadow_images;
	std::vector<const gpu::sampler*> point_shadow_samplers;
	point_shadow_images.reserve(max_point_shadow_lights);
	point_shadow_samplers.reserve(max_point_shadow_lights);
	for (std::size_t i = 0; i < max_point_shadow_lights; ++i) {
		point_shadow_images.push_back(shadow_state ? &shadow_state->point_shadow_gpu_image(i) : &s.placeholder_cube_depth);
		point_shadow_samplers.push_back(shadow_state ? &shadow_state->point_shadow_gpu_sampler(i) : &s.placeholder_cube_sampler);
	}

	const auto* lc_state = phase.try_state_of<light_culling::state>();

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer writer(ctx.device_ref(), s.shader_handle, s.descriptors[i]);

		if (lc_state) {
			const auto fi = static_cast<std::uint32_t>(i);
			writer.buffer("light_index_list", lc_state->light_index_list(fi))
				.buffer("tile_light_table", lc_state->tile_light_table(fi));
		}

		writer.image_array("shadow_maps", shadow_images, s.shadow_sampler, gpu::image_layout::general)
			.image_array("point_shadow_maps", point_shadow_images, point_shadow_samplers, gpu::image_layout::general)
			.commit();
	}

	ctx.on_swap_chain_recreate([&s, lc_state, &ctx]() {
		if (!lc_state) return;
		for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
			const auto fi = static_cast<std::uint32_t>(i);
			gpu::descriptor_writer(ctx.device_ref(), s.shader_handle, s.descriptors[i])
				.buffer("light_index_list", lc_state->light_index_list(fi))
				.buffer("tile_light_table", lc_state->tile_light_table(fi))
				.commit();
		}
	});

	s.pipeline = gpu::create_graphics_pipeline(ctx.device_ref(), *s.shader_handle, {
		.depth = { .test = true, .write = false, .compare = gpu::compare_op::less_or_equal },
		.push_constant_block = "push_constants"
	});


	s.skinned_shader = ctx.get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	ctx.instantly_load(s.skinned_shader);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		s.skinned_descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.skinned_shader);

		gpu::descriptor_writer(ctx.device_ref(), s.skinned_shader, s.skinned_descriptors[i])
			.buffer("CameraUBO", s.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.commit();
	}

	s.skinned_pipeline = gpu::create_graphics_pipeline(ctx.device_ref(), *s.skinned_shader, {
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

	const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index];
	s.shader_handle->set_uniform("CameraUBO.view", view, cam_alloc);
	s.shader_handle->set_uniform("CameraUBO.proj", proj, cam_alloc);
	s.shader_handle->set_uniform("CameraUBO.inv_view", view.inverse(), cam_alloc);

	auto dir_chunk = phase.registry.view<directional_light_component>();
	auto spot_chunk = phase.registry.view<spot_light_component>();
	auto point_chunk = phase.registry.view<point_light_component>();

	const auto& light_alloc = s.light_buffers[frame_index];
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

	const auto& shadow_alloc = s.shadow_params_buffers[frame_index];
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

	const auto& point_shadow_alloc = s.point_shadow_params_buffers[frame_index];
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

	auto meshlet_writer = gpu::create_push_writer(ctx.device_ref(), s.shader_handle);
	auto skinned_writer = gpu::create_push_writer(ctx.device_ref(), s.skinned_shader);

	auto pass = ctx.graph().add_pass<state>();
	pass.track(s.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.track(s.light_buffers[frame_index]);
	pass.track(s.shadow_params_buffers[frame_index]);
	pass.track(s.point_shadow_params_buffers[frame_index]);
	pass.track(gc_state->instance_buffer[frame_index]);

	pass.after<light_culling::state>()
		.after<depth_prepass::state>()
		.reads(
			gpu::storage_read(lc_state->tile_light_table_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(lc_state->light_index_list_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(gc_state->skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_state->skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.color_output(gpu::color_clear{ 0.1f, 0.1f, 0.1f, 1.0f })
		.depth_output_load()
		.record([&s, &normal_batches, &skinned_batches, gc_state, frame_index, num_lights_i, screen_sz, ext_w, ext_h,
			meshlet_writer = std::move(meshlet_writer), skinned_writer = std::move(skinned_writer)](gpu::recording_context& ctx) mutable {
			const vec2u ext_size{ ext_w, ext_h };
			ctx.set_viewport(ext_size);
			ctx.set_scissor(ext_size);

			if (!normal_batches.empty()) {
				const auto& instance_buf = gc_state->instance_buffer[frame_index];

				bool pipeline_bound = false;

				for (std::size_t i = 0; i < normal_batches.size(); ++i) {
					const auto& batch = normal_batches[i];
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					if (!mesh.has_meshlets()) {
						continue;
					}

					const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
					const auto& tex_img  = has_texture ? mesh.material()->diffuse_texture->gpu_image()  : s.blank_texture->gpu_image();
					const auto& tex_samp = has_texture ? mesh.material()->diffuse_texture->gpu_sampler() : s.blank_texture->gpu_sampler();

					if (!pipeline_bound) {
						ctx.bind(s.pipeline);
						ctx.bind_descriptors(s.pipeline, s.descriptors[frame_index]);
						pipeline_bound = true;
					}

					meshlet_writer.begin(frame_index);
					mesh.meshlet_gpu().bind(meshlet_writer);
					meshlet_writer
						.buffer("instanceData", instance_buf)
						.image("diffuseSampler", tex_img, tex_samp, gpu::image_layout::shader_read_only);
					ctx.commit(meshlet_writer.native_writer(), s.pipeline, 1);

					const std::uint32_t ml_count = mesh.meshlet_count();
					for (std::uint32_t inst = 0; inst < batch.instance_count; ++inst) {
						auto pc = s.shader_handle->cache_push_block("push_constants");
						pc.set("meshlet_offset", static_cast<std::uint32_t>(0));
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

				const auto& skin_buf     = gc_state->skin_buffer[frame_index];
				const auto& instance_buf = gc_state->instance_buffer[frame_index];

				for (std::size_t i = 0; i < skinned_batches.size(); ++i) {
					const auto& batch = skinned_batches[i];
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
					const auto& tex_img  = has_texture ? mesh.material()->diffuse_texture->gpu_image()  : s.blank_texture->gpu_image();
					const auto& tex_samp = has_texture ? mesh.material()->diffuse_texture->gpu_sampler() : s.blank_texture->gpu_sampler();

					skinned_writer.begin(frame_index);
					skinned_writer
						.image("diffuseSampler", tex_img, tex_samp, gpu::image_layout::shader_read_only)
						.buffer("skinMatrices", skin_buf)
						.buffer("instanceData", instance_buf);
					ctx.commit(skinned_writer.native_writer(), s.skinned_pipeline, 1);

					ctx.bind_vertex(mesh.vertex_gpu_buffer());
					ctx.bind_index(mesh.index_gpu_buffer());

					ctx.draw_indirect(
						gc_state->skinned_indirect_commands_buffer[frame_index],
						i * sizeof(gpu::draw_indexed_indirect_command),
						1,
						0
					);
				}
			}
		});
}

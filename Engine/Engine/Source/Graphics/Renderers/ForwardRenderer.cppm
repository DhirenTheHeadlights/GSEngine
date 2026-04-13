export module gse.graphics:forward_renderer;

import std;

import :geometry_collector;
import :depth_prepass_renderer;
import :rt_shadow_renderer;
import :light_culling_renderer;
import :camera_system;
import :texture;
import :point_light;
import :spot_light;
import :directional_light;

import gse.math;
import gse.utility;
import gse.platform;
import gse.log;

export namespace gse::renderer::forward {
	constexpr std::size_t max_lights = 1024;
	constexpr std::size_t max_materials = 1024;

	enum class shadow_quality_level : int {
		off    = 0,
		hard   = 1,
		low    = 2,
		medium = 3,
		high   = 4
	};

	enum class ao_quality_level : int {
		off   = 0,
		low   = 1,
		medium = 2,
		high  = 3,
		ultra = 4
	};

	enum class reflection_quality_level : int {
		off    = 0,
		low    = 1,
		medium = 2,
		high   = 3
	};

	struct state {
		gpu::pipeline pipeline;
		per_frame_resource<gpu::descriptor_region> descriptors;
		resource::handle<shader> shader_handle;

		gpu::pipeline skinned_pipeline;
		per_frame_resource<gpu::descriptor_region> skinned_descriptors;
		resource::handle<shader> skinned_shader;

		per_frame_resource<gpu::buffer> light_buffers;
		per_frame_resource<gpu::buffer> material_palette_buffers;

		resource::handle<texture> blank_texture;

		std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;

		shadow_quality_level shadow_quality = shadow_quality_level::medium;
		ao_quality_level ao_quality = ao_quality_level::medium;
		reflection_quality_level reflection_quality = reflection_quality_level::medium;
	};

	struct system {
		static auto initialize(
			const initialize_phase& phase,
			state& s
		) -> void;

		static auto render(
			const render_phase& phase,
			const state& s
		) -> void;
	};
}

auto gse::renderer::forward::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Shadow Quality",
		.description = "Off: no shadows | Hard: 1 ray, 50m range | Low: 1 ray, distance-adaptive, 100m | Medium: 2 rays near/1 far, 200m | High: 4 rays near/2 mid/1 far, 500m",
		.ref = reinterpret_cast<void*>(&s.shadow_quality),
		.type = typeid(int),
		.enum_options = { { "Off", 0 }, { "Hard", 1 }, { "Low", 2 }, { "Medium", 3 }, { "High", 4 } }
	});

	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "AO Quality",
		.description = "Off: no AO | Low: 1 ray, 5m | Medium: 2 rays, 10m | High: 4 rays, 20m | Ultra: 8 rays, 30m",
		.ref = reinterpret_cast<void*>(&s.ao_quality),
		.type = typeid(int),
		.enum_options = { { "Off", 0 }, { "Low", 1 }, { "Medium", 2 }, { "High", 3 }, { "Ultra", 4 } }
	});

	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Reflection Quality",
		.description = "Off: no reflections | Low: mirror only (roughness<0.1), 100m | Medium: glossy (roughness<0.3), 200m | High: 2 rays glossy (roughness<0.5), 500m",
		.ref = reinterpret_cast<void*>(&s.reflection_quality),
		.type = typeid(int),
		.enum_options = { { "Off", 0 }, { "Low", 1 }, { "Medium", 2 }, { "High", 3 } }
	});

	s.shader_handle = ctx.get<shader>("Shaders/Standard3D/meshlet_geometry");
	ctx.instantly_load(s.shader_handle);

	const auto camera_ubo = s.shader_handle->uniform_block("CameraUBO");
	const auto light_block = s.shader_handle->uniform_block("lights_ssbo");
	const auto light_buffer_size = light_block.size * max_lights;
	const auto material_block = s.shader_handle->uniform_block("material_palette");
	const auto material_buffer_size = material_block.size * max_materials;

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx.device_ref(), {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		s.light_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = light_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		s.material_palette_buffers[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = material_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		s.descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.shader_handle);

		gpu::descriptor_writer(ctx.device_ref(), s.shader_handle, s.descriptors[i])
			.buffer("CameraUBO", s.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.buffer("lights_ssbo", s.light_buffers[i], 0, light_buffer_size)
			.buffer("material_palette", s.material_palette_buffers[i], 0, material_buffer_size)
			.commit();
	}

	const auto* rt_state = phase.try_state_of<rt_shadow::state>();
	const auto* lc_state = phase.try_state_of<light_culling::state>();

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		gpu::descriptor_writer writer(ctx.device_ref(), s.shader_handle, s.descriptors[i]);

		if (rt_state) {
			writer.acceleration_structure("tlas", rt_state->tlas(fi).native_handle());
		}

		if (lc_state) {
			writer.buffer("light_index_list", lc_state->light_index_list(fi))
				.buffer("tile_light_table", lc_state->tile_light_table(fi));
		}

		writer.commit();
	}

	ctx.on_swap_chain_recreate([&s, lc_state, rt_state, &ctx]() {
		for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
			const auto fi = static_cast<std::uint32_t>(i);
			gpu::descriptor_writer writer(ctx.device_ref(), s.shader_handle, s.descriptors[i]);

			if (rt_state) {
				writer.acceleration_structure("tlas", rt_state->tlas(fi).native_handle());
			}

			if (lc_state) {
				writer.buffer("light_index_list", lc_state->light_index_list(fi))
					.buffer("tile_light_table", lc_state->tile_light_table(fi));
			}

			writer.commit();
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
	const auto frame_index = ctx.graph().current_frame();

	if (!ctx.graph().frame_in_progress()) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const auto* rt_state = phase.try_state_of<rt_shadow::state>();
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

	const std::size_t total_lights = std::min(
		dir_chunk.size() + spot_chunk.size() + point_chunk.size(), max_lights);
	std::vector<std::byte> staging(total_lights * stride, std::byte{ 0 });
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

	const auto& material_alloc = s.material_palette_buffers[frame_index];
	const auto material_block = s.shader_handle->uniform_block("material_palette");
	const auto mat_stride = material_block.size;
	const auto material_count = std::min(data.material_palette_map.size(), max_materials);

	if (material_count > 0) {
		std::vector<std::byte> mat_staging(material_count * mat_stride, std::byte{ 0 });

		auto mat_write = [&](const std::size_t index, const std::string_view member, const auto& v) {
			gse::memcpy(mat_staging.data() + index * mat_stride + material_block.members.at(std::string(member)).offset, v);
		};

		for (const auto& [mat_ptr, idx] : data.material_palette_map) {
			if (idx >= max_materials) continue;
			mat_write(idx, "base_color", mat_ptr->base_color);
			mat_write(idx, "roughness", mat_ptr->roughness);
			mat_write(idx, "metallic", mat_ptr->metallic);
		}

		gse::memcpy(material_alloc.mapped(), mat_staging.data(), material_count * mat_stride);
	}

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
	const int shadow_quality_i = static_cast<int>(s.shadow_quality);
	const int ao_quality_i = static_cast<int>(s.ao_quality);
	const int reflection_quality_i = static_cast<int>(s.reflection_quality);

	auto meshlet_writer = gpu::create_push_writer(ctx.device_ref(), s.shader_handle);
	auto skinned_writer = gpu::create_push_writer(ctx.device_ref(), s.skinned_shader);

	auto pass = ctx.graph().add_pass<state>();
	pass.track(s.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.track(s.light_buffers[frame_index]);
	pass.track(s.material_palette_buffers[frame_index]);
	pass.track(gc_state->instance_buffer[frame_index]);

	pass.after<rt_shadow::render_state>()
		.after<light_culling::state>()
		.after<depth_prepass::state>()
		.reads(
			gpu::storage_read(lc_state->tile_light_table_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(lc_state->light_index_list_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(gc_state->skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_state->skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.color_output(gpu::color_clear{ 0.1f, 0.1f, 0.1f, 1.0f })
		.depth_output_load()
		.record([&s, &normal_batches, &skinned_batches, gc_state, frame_index, num_lights_i, screen_sz, shadow_quality_i, ao_quality_i, reflection_quality_i, ext_w, ext_h,
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

					const bool has_texture = mesh.material().diffuse_texture.valid();
					const auto& tex_img  = has_texture ? mesh.material().diffuse_texture->gpu_image()  : s.blank_texture->gpu_image();
					const auto& tex_samp = has_texture ? mesh.material().diffuse_texture->gpu_sampler() : s.blank_texture->gpu_sampler();

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
						pc.set("shadow_quality", shadow_quality_i);
						pc.set("ao_quality", ao_quality_i);
						pc.set("reflection_quality", reflection_quality_i);
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

					const bool has_texture = mesh.material().diffuse_texture.valid();
					const auto& tex_img  = has_texture ? mesh.material().diffuse_texture->gpu_image()  : s.blank_texture->gpu_image();
					const auto& tex_samp = has_texture ? mesh.material().diffuse_texture->gpu_sampler() : s.blank_texture->gpu_sampler();

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

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
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.save;

export namespace gse::renderer::forward {
	constexpr std::size_t max_lights = 1024;
	constexpr std::size_t max_materials = 1024;

	enum class shadow_quality_level : int {
		off = 0,
		hard = 1,
		low = 2,
		medium = 3,
		high = 4
	};

	enum class ao_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
		ultra = 4
	};

	enum class reflection_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3
	};

	struct state {
		shadow_quality_level shadow_quality = shadow_quality_level::medium;
		ao_quality_level ao_quality = ao_quality_level::medium;
		reflection_quality_level reflection_quality = reflection_quality_level::medium;
	};

	struct system {
		struct resources {
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
		};

		struct frame_data {
			linear_vector<std::byte> light_staging;
			linear_vector<std::byte> material_staging;
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
			const state& s,
			const geometry_collector::state& gc_s
		) -> async::task<>;
	};
}

auto gse::renderer::forward::system::initialize(const init_context& phase, resources& r, frame_data& fd, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& assets = phase.assets<gpu::context>();

	phase.channels.push<save::register_property>({
		.category = "Graphics",
		.name = "Shadow Quality",
		.description = "Off: no shadows | Hard: 1 ray, 50m range | Low: 1 ray, distance-adaptive, 100m | Medium: 2 rays near/1 far, 200m | High: 4 rays near/2 mid/1 far, 500m",
		.ref = reinterpret_cast<void*>(&s.shadow_quality),
		.type = typeid(int),
		.enum_options = { { "Off", 0 }, { "Hard", 1 }, { "Low", 2 }, { "Medium", 3 }, { "High", 4 } }
	});

	phase.channels.push<save::register_property>({
		.category = "Graphics",
		.name = "AO Quality",
		.description = "Off: no AO | Low: 1 ray, 5m | Medium: 2 rays, 10m | High: 4 rays, 20m | Ultra: 8 rays, 30m",
		.ref = reinterpret_cast<void*>(&s.ao_quality),
		.type = typeid(int),
		.enum_options = { { "Off", 0 }, { "Low", 1 }, { "Medium", 2 }, { "High", 3 }, { "Ultra", 4 } }
	});

	phase.channels.push<save::register_property>({
		.category = "Graphics",
		.name = "Reflection Quality",
		.description = "Off: no reflections | Low: mirror only (roughness<0.1), 100m | Medium: glossy (roughness<0.3), 200m | High: 2 rays glossy (roughness<0.5), 500m",
		.ref = reinterpret_cast<void*>(&s.reflection_quality),
		.type = typeid(int),
		.enum_options = { { "Off", 0 }, { "Low", 1 }, { "Medium", 2 }, { "High", 3 } }
	});

	r.shader_handle = assets.get<shader>("Shaders/Standard3D/meshlet_geometry");
	assets.instantly_load(r.shader_handle);

	const auto camera_ubo = r.shader_handle->uniform_block("CameraUBO");
	const auto light_block = r.shader_handle->uniform_block("lights_ssbo");
	const auto light_buffer_size = light_block.size * max_lights;
	const auto material_block = r.shader_handle->uniform_block("material_palette");
	const auto material_buffer_size = material_block.size * max_materials;
	fd.light_staging.reserve(light_buffer_size);
	fd.material_staging.reserve(material_buffer_size);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.ubo_allocations["CameraUBO"][i] = gpu::buffer::create(ctx.allocator(), {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		r.light_buffers[i] = gpu::buffer::create(ctx.allocator(), {
			.size = light_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		r.material_palette_buffers[i] = gpu::buffer::create(ctx.allocator(), {
			.size = material_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		r.descriptors[i] = gpu::allocate_descriptors(ctx.shader_registry(), ctx.descriptor_heap(), r.shader_handle);

		gpu::descriptor_writer(ctx.shader_registry(), ctx.device_handle(), r.shader_handle, r.descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.buffer("lights_ssbo", r.light_buffers[i], 0, light_buffer_size)
			.buffer("material_palette", r.material_palette_buffers[i], 0, material_buffer_size)
			.commit();
	}

	const auto* rt_state = phase.try_state_of<rt_shadow::state>();
	const auto* lc_r = phase.try_resources_of<light_culling::system::resources>();

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		gpu::descriptor_writer writer(ctx.shader_registry(), ctx.device_handle(), r.shader_handle, r.descriptors[i]);

		if (rt_state) {
			writer.acceleration_structure("tlas", rt_state->tlas(fi).handle());
		}

		if (lc_r) {
			writer.buffer("light_index_list", lc_r->light_index_list(fi))
				.buffer("tile_light_table", lc_r->tile_light_table(fi));
		}

		writer.commit();
	}

	ctx.on_swap_chain_recreate([&r, lc_r, rt_state, &ctx]() {
		for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
			const auto fi = static_cast<std::uint32_t>(i);
			gpu::descriptor_writer writer(ctx.shader_registry(), ctx.device_handle(), r.shader_handle, r.descriptors[i]);

			if (rt_state) {
				writer.acceleration_structure("tlas", rt_state->tlas(fi).handle());
			}

			if (lc_r) {
				writer.buffer("light_index_list", lc_r->light_index_list(fi))
					.buffer("tile_light_table", lc_r->tile_light_table(fi));
			}

			writer.commit();
		}
	});

	r.pipeline = gpu::create_graphics_pipeline(ctx.device(), ctx.shader_registry(), ctx.bindless_textures(), r.shader_handle, {
		.depth = { 
			.test = true, 
			.write = false, 
			.compare = gpu::compare_op::less_or_equal
		},
		.push_constant_block = "push_constants"
	});


	r.skinned_shader = assets.get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	assets.instantly_load(r.skinned_shader);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.skinned_descriptors[i] = gpu::allocate_descriptors(ctx.shader_registry(), ctx.descriptor_heap(), r.skinned_shader);

		gpu::descriptor_writer(ctx.shader_registry(), ctx.device_handle(), r.skinned_shader, r.skinned_descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.commit();
	}

	r.skinned_pipeline = gpu::create_graphics_pipeline(ctx.device(), ctx.shader_registry(), ctx.bindless_textures(), r.skinned_shader, {
		.depth = { 
			.test = true, 
			.write = false, 
			.compare = gpu::compare_op::less_or_equal
		}
	});


	r.blank_texture = assets.queue<texture>("blank", vec4f(1, 1, 1, 1));
	assets.instantly_load(r.blank_texture);
}

auto gse::renderer::forward::system::frame(frame_context& ctx, const resources& r, frame_data& fd, const state& s, const geometry_collector::state& gc_s) -> async::task<> {

	auto& gpu = ctx.get<gpu::context>();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu.graph().current_frame();

	if (!gpu.graph().frame_in_progress()) {
		co_return;
	}

	const auto* cam_state = ctx.try_state_of<camera::state>();
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};
	const auto& cam_alloc = r.ubo_allocations.at("CameraUBO")[frame_index];

	r.shader_handle->set_uniform(cam_alloc.bytes(), "CameraUBO.view", view);
	r.shader_handle->set_uniform(cam_alloc.bytes(), "CameraUBO.proj", proj);
	r.shader_handle->set_uniform(cam_alloc.bytes(), "CameraUBO.inv_view", view.inverse());

	auto dir_chunk = ctx.components<directional_light_component>();
	auto spot_chunk = ctx.components<spot_light_component>();
	auto point_chunk = ctx.components<point_light_component>();

	const auto& light_alloc = r.light_buffers[frame_index];
	const auto light_block = r.shader_handle->uniform_block("lights_ssbo");
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

	const auto& material_alloc = r.material_palette_buffers[frame_index];
	const auto material_block = r.shader_handle->uniform_block("material_palette");
	const auto mat_stride = material_block.size;
	const auto material_count = std::min(data.material_palette_map.size(), max_materials);

	if (material_count > 0) {
		auto& mat_staging = fd.material_staging;
		mat_staging.assign(material_count * mat_stride, std::byte{ 0 });

		auto mat_write = [&](const std::size_t index, const std::string_view member, const auto& v) {
			gse::memcpy(mat_staging.data() + index * mat_stride + material_block.members.at(std::string(member)).offset, v);
		};

		for (const auto& [mat_ptr, idx] : data.material_palette_map) {
			if (idx >= max_materials) {
				continue;
			}
			mat_write(idx, "base_color", mat_ptr->base_color);
			mat_write(idx, "roughness", mat_ptr->roughness);
			mat_write(idx, "metallic", mat_ptr->metallic);
		}

		gse::memcpy(material_alloc.mapped(), mat_staging.data(), material_count * mat_stride);
	}

	const auto& normal_batches = data.normal_batches;
	const auto& skinned_batches = data.skinned_batches;

	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();
	const auto* lc_r = ctx.try_resources_of<light_culling::system::resources>();
	if (!gc_r || !lc_r) {
		co_return;
	}

	const auto ext = gpu.graph().extent();
	const int num_lights_i = static_cast<int>(light_count);
	const int shadow_quality_i = static_cast<int>(s.shadow_quality);
	const int ao_quality_i = static_cast<int>(s.ao_quality);
	const int reflection_quality_i = static_cast<int>(s.reflection_quality);

	auto meshlet_writer = gpu::descriptor_writer(gpu.shader_registry(), gpu.device_handle(), gpu.descriptor_heap(), r.shader_handle);
	auto skinned_writer = gpu::descriptor_writer(gpu.shader_registry(), gpu.device_handle(), gpu.descriptor_heap(), r.skinned_shader);

	auto pass = gpu.graph().add_pass<state>();
	pass.track(r.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.track(r.light_buffers[frame_index]);
	pass.track(r.material_palette_buffers[frame_index]);
	pass.track(gc_r->instance_buffer[frame_index]);

	pass.after<rt_shadow::state>()
		.after<light_culling::state>()
		.after<depth_prepass::state>()
		.reads(
			gpu::storage_read(lc_r->tile_light_table_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(lc_r->light_index_list_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(gc_r->skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_r->skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.color_output(gpu::color_clear{ 0.1f, 0.1f, 0.1f, 1.0f })
		.depth_output_load();

	auto& rec = co_await pass.record();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (!normal_batches.empty()) {
		const auto& instance_buf = gc_r->instance_buffer[frame_index];

		bool pipeline_bound = false;

		for (std::size_t i = 0; i < normal_batches.size(); ++i) {
			const auto& batch = normal_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			if (!mesh.has_meshlets()) {
				continue;
			}

			const bool has_texture = mesh.material().diffuse_texture.valid();
			const auto& tex_img = has_texture ? mesh.material().diffuse_texture->gpu_image() : r.blank_texture->gpu_image();
			const auto& tex_samp = has_texture ? mesh.material().diffuse_texture->gpu_sampler() : r.blank_texture->gpu_sampler();

			if (!pipeline_bound) {
				rec.bind(r.pipeline);
				rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
				pipeline_bound = true;
			}

			meshlet_writer.begin(frame_index);
			mesh.meshlet_gpu().bind(meshlet_writer);
			meshlet_writer
				.buffer("instanceData", instance_buf)
				.image("diffuseSampler", tex_img, tex_samp, gpu::image_layout::shader_read_only);
			rec.commit(meshlet_writer.native_writer(), r.pipeline, 1);

			const std::uint32_t ml_count = mesh.meshlet_count();
			const std::uint32_t task_groups = (ml_count + 31) / 32;

			auto pc = gpu::cache_push_block(r.shader_handle, "push_constants");
			pc.set("meshlet_offset", static_cast<std::uint32_t>(0));
			pc.set("meshlet_count", ml_count);
			pc.set("first_instance", batch.first_instance);
			pc.set("num_lights", num_lights_i);
			pc.set("screen_size", ext);
			pc.set("shadow_quality", shadow_quality_i);
			pc.set("ao_quality", ao_quality_i);
			pc.set("reflection_quality", reflection_quality_i);
			rec.push(r.pipeline, pc);

			rec.draw_mesh_tasks(task_groups, batch.instance_count, 1);
		}
	}

	if (!skinned_batches.empty()) {
		rec.bind(r.skinned_pipeline);
		rec.bind_descriptors(r.skinned_pipeline, r.skinned_descriptors[frame_index]);

		const auto& skin_buf = gc_r->skin_buffer[frame_index];
		const auto& instance_buf = gc_r->instance_buffer[frame_index];

		for (std::size_t i = 0; i < skinned_batches.size(); ++i) {
			const auto& batch = skinned_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			const bool has_texture = mesh.material().diffuse_texture.valid();
			const auto& tex_img = has_texture ? mesh.material().diffuse_texture->gpu_image() : r.blank_texture->gpu_image();
			const auto& tex_samp = has_texture ? mesh.material().diffuse_texture->gpu_sampler() : r.blank_texture->gpu_sampler();

			skinned_writer.begin(frame_index);
			skinned_writer
				.image("diffuseSampler", tex_img, tex_samp, gpu::image_layout::shader_read_only)
				.buffer("skinMatrices", skin_buf)
				.buffer("instanceData", instance_buf);
			rec.commit(skinned_writer.native_writer(), r.skinned_pipeline, 1);

			rec.bind_vertex(mesh.vertex_gpu_buffer());
			rec.bind_index(mesh.index_gpu_buffer());

			rec.draw_indirect(
				gc_r->skinned_indirect_commands_buffer[frame_index],
				i * sizeof(gpu::draw_indexed_indirect_command),
				1,
				0
			);
		}
	}
}

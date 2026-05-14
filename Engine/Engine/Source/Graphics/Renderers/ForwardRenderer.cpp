module gse.graphics;

import std;

import :forward_renderer;
import :geometry_collector;
import :depth_prepass_renderer;
import :rt_shadow_renderer;
import :light_culling_renderer;
import :skin_compute_renderer;
import :cull_compute_renderer;
import :camera_system;
import :texture;
import :point_light;
import :spot_light;
import :directional_light;
import :settings;

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
import gse.meta;
import gse.shader;

auto gse::renderer::forward::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, const rt_shadow::system::state& rt_state, const light_culling::system::resources& lc_r, settings& cfg, resources& r, frame_data& fd) -> async::task<> {
	auto& assets = const_cast<asset::state&>(assets_s);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);
	constexpr std::size_t light_stride = sizeof(shaders::forward::light);
	constexpr std::size_t material_stride = sizeof(shaders::forward::material_data);
	constexpr std::size_t light_buffer_size = light_stride * max_lights;
	constexpr std::size_t material_buffer_size = material_stride * max_materials;

	fd.light_staging.reserve(light_buffer_size);
	fd.material_staging.reserve(material_buffer_size);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.camera_ubo_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = camera_ubo_size,
			.usage = gpu::buffer_flag::uniform
		});

		r.light_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = light_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		r.material_palette_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = material_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		r.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), std::string_view("forward_3d"));

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("forward_3d"), r.descriptors[i])
			.buffer("camera_ubo", r.camera_ubo_buffers[i], 0, camera_ubo_size)
			.buffer("lights_ssbo", r.light_buffers[i], 0, light_buffer_size)
			.buffer("material_palette", r.material_palette_buffers[i], 0, material_buffer_size)
			.commit();
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		gpu::descriptor_writer writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("forward_3d"), r.descriptors[i]);

		writer.acceleration_structure("scene_tlas", (*rt_state.tlas_ptrs[fi]).handle());
		writer.buffer("light_index_list", lc_r.light_index_list_buffers[fi])
			.buffer("tile_light_table", lc_r.tile_light_table_buffers[fi]);

		writer.commit();
	}

	gpu::context::on_swap_chain_recreate(gpu_s, [&r, &lc_r, &rt_state, &gpu_s]() {
		for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
			const auto fi = static_cast<std::uint32_t>(i);
			gpu::descriptor_writer writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("forward_3d"), r.descriptors[i]);

			writer.acceleration_structure("scene_tlas", (*rt_state.tlas_ptrs[fi]).handle());
			writer.buffer("light_index_list", lc_r.light_index_list_buffers[fi])
				.buffer("tile_light_table", lc_r.tile_light_table_buffers[fi]);

			writer.commit();
		}
	});

	r.pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, shaders::forward::meshlet_entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.skinned_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), std::string_view("standard_3d"));

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("standard_3d"), r.skinned_descriptors[i])
			.buffer("camera_ubo", r.camera_ubo_buffers[i], 0, camera_ubo_size)
			.commit();
	}

	r.skinned_pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, shaders::standard_3d::skinned_geometry_entry::pod);

	r.blank_texture = asset::queue<texture>(assets, "blank", vec4f(1, 1, 1, 1));
	while (asset::resource_state<texture>(assets, r.blank_texture.id()) != resource::state::loaded) {
		co_await ctx.next_tick();
	}

	co_return;
}

auto gse::renderer::forward::system::frame(frame_context& ctx, const gpu::context::state& gpu_s, const settings& cfg, const resources& r, frame_data& fd, const camera::system::state& cam_state, const geometry_collector::system::resources& gc_r, const light_culling::system::resources& lc_r) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		const auto ext = gpu_s.render_graph->extent();
		auto rec = co_await gpu::pass<system>(ctx)
			.color(gpu::clear_color(gpu::color_clear{ 0.1f, 0.1f, 0.1f, 1.0f }))
			.depth(gpu::clear_depth(gpu::depth_clear{ .depth = 1.0f }));
		rec.set_viewport(ext);
		rec.set_scissor(ext);
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;

	const shaders::common::camera_data camera{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
	};
	gse::memcpy(r.camera_ubo_buffers[frame_index].mapped(), camera);

	auto dir_chunk = ctx.components<directional_light_component>();
	auto spot_chunk = ctx.components<spot_light_component>();
	auto point_chunk = ctx.components<point_light_component>();

	const std::size_t total_lights = std::min(
		dir_chunk.size() + spot_chunk.size() + point_chunk.size(),
		max_lights
	);
	auto& staging = fd.light_staging;
	staging.assign(total_lights * sizeof(shaders::forward::light), std::byte{ 0 });
	auto* staging_lights = reinterpret_cast<shaders::forward::light*>(staging.data());
	std::size_t light_count = 0;

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		staging_lights[light_count] = {
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
		const float cut_off_cos = gse::cos(comp.cut_off);
		const float outer_cut_off_cos = gse::cos(comp.outer_cut_off);
		staging_lights[light_count] = {
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
			.cut_off = cut_off_cos,
			.outer_cut_off = outer_cut_off_cos,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		staging_lights[light_count] = {
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
		gse::memcpy(r.light_buffers[frame_index].mapped(), staging.data(), light_count * sizeof(shaders::forward::light));
	}

	const auto material_count = std::min(data.material_palette_map.size(), max_materials);

	if (material_count > 0) {
		auto& mat_staging = fd.material_staging;
		mat_staging.assign(material_count * sizeof(shaders::forward::material_data), std::byte{ 0 });
		auto* staging_materials = reinterpret_cast<shaders::forward::material_data*>(mat_staging.data());

		for (const auto& [mat_ptr, idx] : data.material_palette_map) {
			if (idx >= max_materials) {
				continue;
			}
			staging_materials[idx] = {
				.base_color = mat_ptr->base_color,
				.roughness = mat_ptr->roughness,
				.metallic = mat_ptr->metallic,
			};
		}

		gse::memcpy(r.material_palette_buffers[frame_index].mapped(), mat_staging.data(), material_count * sizeof(shaders::forward::material_data));
	}

	const auto& normal_batches = data.normal_batches;
	const auto& skinned_batches = data.skinned_batches;

	const auto ext = gpu_s.render_graph->extent();
	const int num_lights_i = static_cast<int>(light_count);
	const int shadow_quality_i = static_cast<int>(cfg.shadow_quality);
	const int ao_quality_i = static_cast<int>(cfg.ao_quality);
	const int reflection_quality_i = static_cast<int>(cfg.reflection_quality);

	auto meshlet_writer = gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), gpu_s.device->descriptor_heap(), std::string_view("forward_3d"));
	auto skinned_writer = gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), gpu_s.device->descriptor_heap(), std::string_view("standard_3d"));

	auto rec = co_await gpu::pass<system>(ctx)
		.color(gpu::clear_color(gpu::color_clear{ 0.1f, 0.1f, 0.1f, 1.0f }))
		.depth(gpu::load_depth())
		.after<rt_shadow::system, light_culling::system, depth_prepass::system>()
		.reads(
			gpu::storage_read(lc_r.tile_light_table_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(lc_r.light_index_list_buffers[frame_index], gpu::pipeline_stage::fragment_shader),
			gpu::storage_read(gc_r.skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_r.normal_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect),
			gpu::indirect_read(gc_r.skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.tracks(
			r.camera_ubo_buffers[frame_index],
			r.light_buffers[frame_index],
			r.material_palette_buffers[frame_index],
			gc_r.instance_buffer[frame_index]
		);
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (!normal_batches.empty()) {
		const auto& instance_buf = gc_r.instance_buffer[frame_index];

		bool pipeline_bound = false;

		for (std::size_t i = 0; i < normal_batches.size(); ++i) {
			const auto& batch = normal_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			if (!mesh.has_meshlets()) {
				continue;
			}

			if (!mesh.upload_token().ready()) {
				continue;
			}

			const auto& diffuse = mesh.material().diffuse_texture;
			const bool has_texture = diffuse.valid() && diffuse->upload_token().ready();
			const auto& tex_img = has_texture ? diffuse->gpu_image() : r.blank_texture->gpu_image();
			const auto& tex_samp = has_texture ? diffuse->gpu_sampler() : r.blank_texture->gpu_sampler();

			if (!pipeline_bound) {
				rec.bind(r.pipeline);
				rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
				pipeline_bound = true;
			}

			meshlet_writer.begin(frame_index);
			mesh.meshlet_gpu().bind(meshlet_writer);
			meshlet_writer
				.buffer("instance_data_buffer", instance_buf)
				.image("diffuse_sampler", tex_img, tex_samp, gpu::image_layout::shader_read_only);
			rec.commit(meshlet_writer.native_writer(), r.pipeline, 1);

			const std::uint32_t meshlet_count = mesh.meshlet_count();

			const gpu::typed_push_constants<shaders::forward::push_constants> pc{
				.data = {
					.meshlet_offset = 0,
					.meshlet_count = meshlet_count,
					.first_instance = batch.first_instance,
					.num_lights = num_lights_i,
					.screen_size = vec2u{ ext.x(), ext.y() },
					.shadow_quality = shadow_quality_i,
					.ao_quality = ao_quality_i,
					.reflection_quality = reflection_quality_i,
				},
				.stages = gpu::stage_flag::task | gpu::stage_flag::mesh | gpu::stage_flag::fragment,
			};
			rec.push(r.pipeline, pc);

			rec.draw_mesh_tasks_indirect(
				gc_r.normal_indirect_commands_buffer[frame_index],
				i * sizeof(gpu::draw_mesh_tasks_indirect_command),
				1,
				sizeof(gpu::draw_mesh_tasks_indirect_command)
			);
		}
	}

	if (!skinned_batches.empty()) {
		rec.bind(r.skinned_pipeline);
		rec.bind_descriptors(r.skinned_pipeline, r.skinned_descriptors[frame_index]);

		const auto& skin_buf = gc_r.skin_buffer[frame_index];
		const auto& instance_buf = gc_r.instance_buffer[frame_index];

		for (std::size_t i = 0; i < skinned_batches.size(); ++i) {
			const auto& batch = skinned_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			const bool has_texture = mesh.material().diffuse_texture.valid();
			const auto& tex_img = has_texture ? mesh.material().diffuse_texture->gpu_image() : r.blank_texture->gpu_image();
			const auto& tex_samp = has_texture ? mesh.material().diffuse_texture->gpu_sampler() : r.blank_texture->gpu_sampler();

			skinned_writer.begin(frame_index);
			skinned_writer
				.image("diffuse_sampler", tex_img, tex_samp, gpu::image_layout::shader_read_only)
				.buffer("skin_matrices", skin_buf)
				.buffer("instance_data_buffer", instance_buf);
			rec.commit(skinned_writer.native_writer(), r.skinned_pipeline, 1);

			rec.bind_vertex(mesh.vertex_gpu_buffer());
			rec.bind_index(mesh.index_gpu_buffer());

			rec.draw_indirect(
				gc_r.skinned_indirect_commands_buffer[frame_index],
				i * sizeof(gpu::draw_indexed_indirect_command),
				1,
				0
			);
		}
	}
}

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

auto gse::renderer::forward::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, const rt_shadow::system::state& rt_state, const light_culling::system::resources& lc_r, settings& cfg, resources& r, frame_data& fd) -> async::task<> {
	auto& assets = const_cast<asset::state&>(assets_s);

	r.shader_handle = co_await asset::load<shader>(ctx, "Shaders/Standard3D/meshlet_geometry");

	const auto camera_ubo = r.shader_handle->uniform_block("CameraUBO");
	const auto light_block = r.shader_handle->uniform_block("lights_ssbo");
	const auto light_buffer_size = light_block.size * max_lights;
	const auto material_block = r.shader_handle->uniform_block("material_palette");
	const auto material_buffer_size = material_block.size * max_materials;
	fd.light_staging.reserve(light_buffer_size);
	fd.material_staging.reserve(material_buffer_size);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.ubo_allocations["CameraUBO"][i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = camera_ubo.size,
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

		r.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.buffer("lights_ssbo", r.light_buffers[i], 0, light_buffer_size)
			.buffer("material_palette", r.material_palette_buffers[i], 0, material_buffer_size)
			.commit();
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		gpu::descriptor_writer writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.descriptors[i]);

		writer.acceleration_structure("tlas", (*rt_state.tlas_ptrs[fi]).handle());
		writer.buffer("light_index_list", lc_r.light_index_list_buffers[fi])
			.buffer("tile_light_table", lc_r.tile_light_table_buffers[fi]);

		writer.commit();
	}

	gpu::context::on_swap_chain_recreate(gpu_s, [&r, &lc_r, &rt_state, &gpu_s]() {
		for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
			const auto fi = static_cast<std::uint32_t>(i);
			gpu::descriptor_writer writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.descriptors[i]);

			writer.acceleration_structure("tlas", (*rt_state.tlas_ptrs[fi]).handle());
			writer.buffer("light_index_list", lc_r.light_index_list_buffers[fi])
				.buffer("tile_light_table", lc_r.tile_light_table_buffers[fi]);

			writer.commit();
		}
	});

	r.pipeline = gpu::create_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, r.shader_handle, {
		.depth = {
			.test = true,
			.write = false,
			.compare = gpu::compare_op::less_or_equal
		},
		.push_constant_block = "push_constants"
	});


	r.skinned_shader = co_await asset::load<shader>(ctx, "Shaders/Standard3D/skinned_geometry_pass");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.skinned_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.skinned_shader);

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.skinned_shader, r.skinned_descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.commit();
	}

	r.skinned_pipeline = gpu::create_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, r.skinned_shader, {
		.depth = {
			.test = true,
			.write = false,
			.compare = gpu::compare_op::less_or_equal
		}
	});


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

	const auto ext = gpu_s.render_graph->extent();
	const int num_lights_i = static_cast<int>(light_count);
	const int shadow_quality_i = static_cast<int>(cfg.shadow_quality);
	const int ao_quality_i = static_cast<int>(cfg.ao_quality);
	const int reflection_quality_i = static_cast<int>(cfg.reflection_quality);

	auto meshlet_writer = gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), gpu_s.device->descriptor_heap(), r.shader_handle);
	auto skinned_writer = gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), gpu_s.device->descriptor_heap(), r.skinned_shader);

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
			r.ubo_allocations.at("CameraUBO")[frame_index],
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
				.buffer("instanceData", instance_buf)
				.image("diffuseSampler", tex_img, tex_samp, gpu::image_layout::shader_read_only);
			rec.commit(meshlet_writer.native_writer(), r.pipeline, 1);

			const std::uint32_t meshlet_count = mesh.meshlet_count();

			auto pc = gpu::cache_push_block(r.shader_handle, "push_constants");
			pc.set("meshlet_offset", static_cast<std::uint32_t>(0));
			pc.set("meshlet_count", meshlet_count);
			pc.set("first_instance", batch.first_instance);
			pc.set("num_lights", num_lights_i);
			pc.set("screen_size", vec2u{ ext.x(), ext.y() });
			pc.set("shadow_quality", shadow_quality_i);
			pc.set("ao_quality", ao_quality_i);
			pc.set("reflection_quality", reflection_quality_i);
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
				.image("diffuseSampler", tex_img, tex_samp, gpu::image_layout::shader_read_only)
				.buffer("skinMatrices", skin_buf)
				.buffer("instanceData", instance_buf);
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

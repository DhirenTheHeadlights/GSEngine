module gse.graphics:skin_renderer_impl;

import std;

import :skin_renderer;
import :animation_components;
import :mesh;
import :shared_shaders;
import :skinned_model;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.physics;

namespace gse::renderer::skin {
	struct [[= shaders::shader_struct]] palette_push_constants {
		std::uint32_t bone_count;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::ssbo_readonly
	]] bone_data {
		using element = rig_bone_binding;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readwrite
	]] palette_out {
		using element = skin_matrix;
	};

	using palette_binding_types = type_pack<bone_data, palette_out>;
	using palette_shader_types = type_pack<rig_bone_binding, skin_matrix>;

	using palette_entry = gpu::compute_entry<
		gpu::body_path<"Compute/skin_palette">,
		gpu::types<palette_shader_types>,
		gpu::bindings<palette_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<palette_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	struct [[= shaders::shader_struct]] deform_push_constants {
		std::uint32_t vertex_count;
		std::uint32_t vertex_offset;
		std::uint32_t bone_offset;
		std::uint32_t bone_count;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::ssbo_readonly
	]] palette_data {
		using element = skin_matrix;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] bind_vertices {
		using element = shaders::forward::vertex;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::ssbo_readonly
	]] influence_data {
		using element = skin_influence;
	};

	struct [[
		= shaders::binding<0, 3>{},
		= shaders::ssbo_readwrite
	]] deformed_vertices {
		using element = shaders::forward::vertex;
	};

	using deform_binding_types = type_pack<palette_data, bind_vertices, influence_data, deformed_vertices>;
	using deform_shader_types = type_pack<shaders::forward::vertex, skin_matrix, skin_influence>;

	using deform_entry = gpu::compute_entry<
		gpu::body_path<"Compute/skin_deform">,
		gpu::types<deform_shader_types>,
		gpu::bindings<deform_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<deform_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

auto gse::renderer::skin::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	d.palette_pipeline = gpu::build_compute_program(*gpu_s.device, palette_entry::pod);
	d.deform_pipeline = gpu::build_compute_program(*gpu_s.device, deform_entry::pod);
	d.initialized = true;
	return {};
}

auto gse::renderer::skin::collect(context& ctx, data& d, const channel_read<physics::interpolation_state> interp_in, read<skeleton_instance_component> skeletons, read<physics::transform_component> transforms, read<physics::motion_component> motions, read<physics::collision_component> collisions) -> async::task<> {
	d.instances.clear();
	d.bone_bindings.clear();
	d.bounds.clear();

	auto lag = system_clock::fixed_lag();
	if (const auto& interpolation = interp_in.of<physics::interpolation_state>(); !interpolation.empty() && !interpolation[0].advancing) {
		lag = time_t<float, seconds>{};
	}

	const auto owners = skeletons.owner_ids();

	for (std::size_t i = 0; i < skeletons.size(); ++i) {
		const auto& skeleton = skeletons[i];
		if (!skeleton.model.valid()) {
			continue;
		}

		const auto model = skeleton.model.resolve();
		if (!model->uploads_ready()) {
			continue;
		}

		const auto bones = model->bones();
		const auto bone_offset = static_cast<std::uint32_t>(d.bone_bindings.size());

		auto bounds = aabb{
			.max = vec3<position>(meters(std::numeric_limits<float>::lowest())),
			.min = vec3<position>(meters(std::numeric_limits<float>::max())),
		};
		bool any_bounds = false;

		for (std::size_t slot = 0; slot < bones.size(); ++slot) {
			const auto* bone_tc = slot < skeleton.bone_count
				? transforms.find(skeleton.bones[slot])
				: nullptr;

			const auto render_tc = bone_tc != nullptr
				? physics::interpolated_transform(*bone_tc, motions.find(skeleton.bones[slot]), vec3<displacement>{}, lag)
				: physics::transform_component{};

			d.bone_bindings.push_back({
				.inverse_bind = bones[slot].inverse_bind,
				.world = bone_tc != nullptr ? physics::transformation_matrix(render_tc) : mat4f(1.f),
				.valid = bone_tc != nullptr ? 1u : 0u,
			});

			if (bone_tc == nullptr) {
				continue;
			}

			const auto* bone_cc = collisions.find(skeleton.bones[slot]);
			if (bone_cc == nullptr) {
				continue;
			}

			const auto box = physics::world_aabb_of(render_tc, *bone_cc, nullptr);
			bounds.min = min(bounds.min, box.min);
			bounds.max = max(bounds.max, box.max);
			any_bounds = true;
		}

		if (any_bounds) {
			d.bounds.insert_or_assign(owners[i], bounds);
		}

		const auto meshes = model->meshes();
		for (std::uint32_t m = 0; m < meshes.size(); ++m) {
			d.instances.push_back({
				.owner = owners[i],
				.model = model,
				.mesh_index = m,
				.bone_offset = bone_offset,
				.vertex_offset = 0,
				.vertex_count = static_cast<std::uint32_t>(meshes[m].vertex_count()),
			});

			if (const auto target = d.targets.find(std::pair(owners[i], m)); target != d.targets.end()) {
				target->second.write_index ^= 1u;
			}
		}
	}

	return {};
}

auto gse::renderer::skin::deformed_slots_for(const deformed_target_map& targets, const id owner, const std::uint32_t mesh_index) -> deformed_slots {
	const auto it = targets.find(std::pair(owner, mesh_index));
	if (it == targets.end()) {
		return {};
	}

	const auto& target = it->second;
	const auto previous_index = target.write_index ^ 1u;

	if (!target.vertices[target.write_index].valid()) {
		return {};
	}

	return {
		.current = target.vertices[target.write_index].slot(),
		.previous = target.writes > 0 && target.vertices[previous_index].valid()
			? target.vertices[previous_index].slot()
			: gpu::bindless_slot{},
	};
}

auto gse::renderer::skin::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!d.initialized || d.instances.empty()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	if (d.bone_bindings.empty()) {
		co_return;
	}

	const auto required = d.bone_bindings.size() * sizeof(rig_bone_binding);
	if (d.bone_buffer_size < required) {
		for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
			d.bone_buffers[i] = gpu_s.device->create_buffer(
				{
					.size = required,
					.stride = sizeof(rig_bone_binding),
					.usage = gpu::buffer_flag::storage,
					.data = d.bone_bindings.data(),
					.bindless = true,
				}
			);
		}
		d.bone_buffer_size = required;
	}
	else {
		d.bone_buffers[frame_index].host_write(d.bone_bindings.data(), required);
	}

	const auto palette_required = d.bone_bindings.size() * sizeof(skin_matrix);
	if (d.palette_buffer_size < palette_required) {
		for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
			d.palette_buffers[i] = gpu_s.device->create_buffer(
				{
					.size = palette_required,
					.stride = sizeof(skin_matrix),
					.usage = gpu::buffer_flag::storage,
					.bindless = true,
					.writable = true,
				},
				"skin.palette"
			);
		}
		d.palette_buffer_size = palette_required;
	}

	const auto total_bones = static_cast<std::uint32_t>(d.bone_bindings.size());

	auto palette_rec = co_await gpu::pass<^^palette_pass>(pass_out)
		.pipeline(d.palette_pipeline);

	palette_rec.dispatch<palette_entry>(
		{
			.bone_count = total_bones,
		},
		{
			.bone_data = d.bone_buffers[frame_index].slot(),
			.palette_out = d.palette_buffers[frame_index].slot(),
		},
		vec3u{ (total_bones + 63) / 64, 1u, 1u }
	);

	auto rec = co_await gpu::pass<^^deform_pass>(pass_out)
		.pipeline(d.deform_pipeline)
		.after<^^palette_pass>();

	for (const auto& instance : d.instances) {
		const auto& mesh = instance.model->meshes()[instance.mesh_index];
		const auto& influences = instance.model->influence_buffers()[instance.mesh_index];

		auto& target = d.targets[std::pair(instance.owner, instance.mesh_index)];
		const auto bytes = instance.vertex_count * sizeof(vertex);
		if (target.capacity < bytes) {
			for (auto& buffer : target.vertices) {
				buffer = gpu_s.device->create_buffer(
					{
						.size = bytes,
						.stride = sizeof(vertex),
						.usage = gpu::buffer_flag::storage,
						.bindless = true,
						.writable = true,
					},
					"skin.deformed"
				);
			}
			target.capacity = bytes;
			target.write_index = 0;
			target.writes = 0;
		}

		rec.dispatch<deform_entry>(
			{
				.vertex_count = instance.vertex_count,
				.vertex_offset = instance.vertex_offset,
				.bone_offset = instance.bone_offset,
				.bone_count = static_cast<std::uint32_t>(instance.model->bones().size()),
			},
			{
				.palette_data = d.palette_buffers[frame_index].slot(),
				.bind_vertices = mesh.meshlet_gpu().vertex_storage.slot(),
				.influence_data = influences.slot(),
				.deformed_vertices = target.vertices[target.write_index].slot(),
			},
			vec3u{ (instance.vertex_count + 63) / 64, 1u, 1u }
		);

		++target.writes;
	}
}
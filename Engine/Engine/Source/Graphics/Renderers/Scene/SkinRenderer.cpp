module gse.graphics:skin_renderer_impl;

import std;

import :skin_renderer;
import :animation_components;
import :deformed_target;
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

	struct palette_bindings {
		shaders::ssbo_readonly<rig_bone_binding> bone_data;
		shaders::ssbo_readwrite<skin_matrix> palette_out;
	};

	using palette_entry = gpu::compute_entry<
		gpu::body_path<"Compute/skin_palette">,
		gpu::bindings<palette_bindings>,
		gpu::threads<64>,
		gpu::push_constant<palette_push_constants>
	>;

	struct [[= shaders::shader_struct]] deform_push_constants {
		std::uint32_t vertex_count;
		std::uint32_t vertex_offset;
		std::uint32_t bone_offset;
		std::uint32_t bone_count;
	};

	struct deform_bindings {
		shaders::ssbo_readonly<skin_matrix> palette_data;
		shaders::ssbo_readonly<shaders::forward::vertex> bind_vertices;
		shaders::ssbo_readonly<skin_influence> influence_data;
		shaders::ssbo_readwrite<shaders::forward::vertex> deformed_vertices;
	};

	using deform_entry = gpu::compute_entry<
		gpu::body_path<"Compute/skin_deform">,
		gpu::bindings<deform_bindings>,
		gpu::threads<64>,
		gpu::push_constant<deform_push_constants>
	>;
}

auto gse::renderer::skin::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.palette_pipeline, palette_entry::pod);
		batch.add(d.deform_pipeline, deform_entry::pod);
	}
	d.initialized = true;
	return {};
}

auto gse::renderer::skin::collect(context& ctx, data& d, const channel_read<physics::interpolation_state> interp_in, read<skeleton_instance_component> skeletons, read<physics::transform_component> transforms, read<physics::motion_component> motions, read<physics::collision_component> collisions) -> async::task<> {
	d.instances.clear();
	d.bone_bindings.clear();
	d.bounds.clear();

	if (!d.initialized) {
		return {};
	}

	physics::interpolation_state interpolation;
	if (const auto& published = interp_in.of<physics::interpolation_state>(); !published.empty()) {
		interpolation = published[0];
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
		if (bones.empty()) {
			continue;
		}
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
				? physics::render_transform(*bone_tc, motions.find(skeleton.bones[slot]), interpolation)
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
				target->second.begin_write();
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

	return it->second.slots();
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
			.bone_data = d.bone_buffers[frame_index],
			.palette_out = d.palette_buffers[frame_index],
		},
		gpu::group_count<palette_entry>(total_bones)
	);

	auto rec = co_await gpu::pass<^^deform_pass>(pass_out)
		.pipeline(d.deform_pipeline)
		.after<^^palette_pass>();

	for (const auto& instance : d.instances) {
		const auto& mesh = instance.model->meshes()[instance.mesh_index];
		const auto& influences = instance.model->influence_buffers()[instance.mesh_index];

		auto& target = d.targets[std::pair(instance.owner, instance.mesh_index)];
		target.reserve(*gpu_s.device, instance.vertex_count * sizeof(vertex), sizeof(vertex), "skin.deformed");

		rec.dispatch<deform_entry>(
			{
				.vertex_count = instance.vertex_count,
				.vertex_offset = instance.vertex_offset,
				.bone_offset = instance.bone_offset,
				.bone_count = static_cast<std::uint32_t>(instance.model->bones().size()),
			},
			{
				.palette_data = d.palette_buffers[frame_index],
				.bind_vertices = mesh.meshlet_gpu().vertex_storage,
				.influence_data = influences,
				.deformed_vertices = target.write_buffer(),
			},
			gpu::group_count<deform_entry>(instance.vertex_count)
		);

		target.finish_write();
	}
}
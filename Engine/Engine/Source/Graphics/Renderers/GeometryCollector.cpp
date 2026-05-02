module gse.graphics;

import std;

import :geometry_collector;
import :camera_system;
import :mesh;
import :skinned_mesh;
import :model;
import :skinned_model;
import :render_component;
import :animation_component;
import :material;
import :skeleton;
import :texture;

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
import gse.physics;


auto gse::renderer::geometry_collector::filter_render_queue(const render_data& data, const std::span<const id> exclude_ids) -> std::vector<render_queue_entry> {
	std::vector<render_queue_entry> result;
	result.reserve(data.render_queue.size());

	for (const auto& queue_entry : data.render_queue) {
		const bool excluded = std::ranges::any_of(exclude_ids, [&](const id& ex) {
			return ex == queue_entry.owner;
		});

		if (!excluded) {
			result.push_back(queue_entry.entry);
		}
	}

	return result;
}

auto gse::renderer::geometry_collector::system::resources::upload_skeleton_data(const skeleton& skel) const -> void {
	const auto joint_count = static_cast<std::size_t>(skel.joint_count());
	const auto joints = skel.joints();

	std::byte* buffer = skeleton_buffer.mapped();

	for (std::size_t i = 0; i < joint_count; ++i) {
		std::byte* offset = buffer + (i * joint_stride);

		const mat4f inverse_bind = joints[i].inverse_bind();
		const std::uint32_t parent_index = joints[i].parent_index();

		gse::memcpy(offset + joint_offsets.at("inverse_bind"), inverse_bind);
		gse::memcpy(offset + joint_offsets.at("parent_index"), parent_index);
	}
}

auto gse::renderer::geometry_collector::system::initialize(init_context& phase, resources& r, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& assets = phase.assets();
	r.ctx = &ctx;

	r.shader_handle = assets.get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	assets.instantly_load(r.shader_handle);

	const auto camera_ubo = r.shader_handle->uniform_block("CameraUBO");

	const auto instance_block = r.shader_handle->uniform_block("instanceData");
	r.instance_stride = instance_block.size;
	for (const auto& [name, member] : instance_block.members) {
		r.instance_offsets[name] = member.offset;
	}

	r.instance_model_matrix_offset   = r.instance_offsets.at("model_matrix");
	r.instance_normal_matrix_offset  = r.instance_offsets.at("normal_matrix");
	r.instance_skin_offset_offset    = r.instance_offsets.at("skin_offset");
	r.instance_joint_count_offset    = r.instance_offsets.at("joint_count");
	r.instance_material_index_offset = r.instance_offsets.at("material_index");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.ubo_allocations["CameraUBO"][i] = gpu::buffer::create(ctx.allocator(), {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		constexpr std::size_t skin_buffer_size = resources::max_skin_matrices * sizeof(mat4f);
		r.skin_buffer[i] = gpu::buffer::create(ctx.allocator(), {
			.size = skin_buffer_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
		const std::size_t instance_buffer_size = resources::max_instances * 2 * r.instance_stride;
		r.instance_buffer[i] = gpu::buffer::create(ctx.allocator(), {
			.size = instance_buffer_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src | gpu::buffer_flag::transfer_dst
		});

		constexpr std::size_t indirect_buffer_size = render_data::max_batches * sizeof(gpu::draw_indexed_indirect_command);
		r.normal_indirect_commands_buffer[i] = gpu::buffer::create(ctx.allocator(), {
			.size = indirect_buffer_size,
			.usage = gpu::buffer_flag::indirect | gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
		r.skinned_indirect_commands_buffer[i] = gpu::buffer::create(ctx.allocator(), {
			.size = indirect_buffer_size,
			.usage = gpu::buffer_flag::indirect | gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});

		constexpr std::size_t local_pose_size = resources::max_skin_matrices * sizeof(mat4f);
		r.local_pose_buffer[i] = gpu::buffer::create(ctx.allocator(), {
			.size = local_pose_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});

		constexpr std::size_t mapping_buffer_size = resources::max_instances * sizeof(physics_mapping_entry);
		r.physics_mapping_buffer[i] = gpu::buffer::create(ctx.allocator(), {
			.size = mapping_buffer_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
	}

	auto skin_compute = assets.get<shader>("Shaders/Compute/skin_compute");
	assets.instantly_load(skin_compute);

	const auto joint_block = skin_compute->uniform_block("skeletonData");
	r.joint_stride = joint_block.size;
	for (const auto& [name, member] : joint_block.members) {
		r.joint_offsets[name] = member.offset;
	}

	r.skeleton_buffer = gpu::buffer::create(ctx.allocator(), {
		.size = resources::max_joints * r.joint_stride,
		.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
	});
}

auto gse::renderer::geometry_collector::system::update(update_context& ctx, const resources& r, state& s) -> async::task<> {
	const auto* cam_state = ctx.try_state_of<camera::state>();
	const view_matrix view_matrix = cam_state ? cam_state->view_matrix : gse::view_matrix{};
	const projection_matrix proj_matrix = cam_state ? cam_state->projection_matrix : projection_matrix{};

	std::unordered_map<id, std::uint32_t> body_index_map;
	for (const auto& [entries] : ctx.read_channel<physics::gpu_body_index_map>()) {
		for (const auto& [eid, idx] : entries) {
			body_index_map[eid] = idx;
		}
	}

	auto [render, motion, collision, anim] = co_await ctx.acquire<
		write<render_component>,
		read<physics::motion_component>,
		read<physics::collision_component>,
		read<animation_component>
	>();

	{
		if (render.empty()) {
			co_return;
		}

		render_data data;
		data.view = view_matrix;
		data.proj = proj_matrix;

		auto& out = data.render_queue;
		auto& skinned_out = data.skinned_render_queue;

		auto& local_pose_staging = data.local_pose_staging;

		std::uint32_t skinned_instance_count = 0;

		out.reserve(render.size());

		{
			trace::scope_guard sg{trace_id<"geom_collect::collect">()};
			const auto render_size = render.size();

			bool motion_order_matches = render_size == motion.size();
			for (std::size_t i = 0; motion_order_matches && i < render_size; ++i) {
				motion_order_matches = render[i].owner_id() == motion[i].owner_id();
			}

			bool collision_order_matches = render_size == collision.size();
			for (std::size_t i = 0; collision_order_matches && i < render_size; ++i) {
				collision_order_matches = render[i].owner_id() == collision[i].owner_id();
			}

			struct component_lookup {
				const physics::motion_component* mc = nullptr;
				const physics::collision_component* cc = nullptr;
			};
			std::vector<component_lookup> lookups(render_size);

			task::parallel_invoke_range(0, render_size, [&](std::size_t i) {
				auto& component = render[i];
				if (!component.render) {
					return;
				}

				const auto eid = component.owner_id();
				const auto* mc = motion_order_matches ? std::addressof(motion[i]) : motion.find(eid);
				const auto* cc = collision_order_matches ? std::addressof(collision[i]) : collision.find(eid);
				if (mc == nullptr || cc == nullptr) {
					return;
				}

				lookups[i] = { mc, cc };

				for (auto& model_handle : component.model_instances) {
					if (!model_handle.handle().valid()) {
						continue;
					}
					model_handle.sync_structure();
					model_handle.sync_transform(*mc, *cc);
				}
			});

			for (std::size_t i = 0; i < render_size; ++i) {
				auto& component = render[i];
				if (!component.render) {
					continue;
				}

				const auto* mc = lookups[i].mc;
				const auto* cc = lookups[i].cc;
				if (mc == nullptr || cc == nullptr) {
					continue;
				}

				const auto eid = component.owner_id();
				const auto& world_aabb = cc->bounding_box.aabb();
				std::uint32_t body_index = owned_render_queue_entry::invalid_body_index;
				if (const auto it = body_index_map.find(eid); it != body_index_map.end()) {
					body_index = it->second;
				}

				for (auto& model_handle : component.model_instances) {
					if (!model_handle.handle().valid()) {
						continue;
					}
					for (const auto& entry : model_handle.render_queue_entries()) {
						out.push_back({
							.entry = entry,
							.owner = eid,
							.world_aabb_min = world_aabb.min,
							.world_aabb_max = world_aabb.max,
							.body_index = body_index
						});
					}
				}

				const auto* anim_comp = anim.find(eid);

				for (auto& skinned_model_handle : component.skinned_model_instances) {
					if (!skinned_model_handle.handle().valid()) {
						continue;
					}

					if (anim_comp != nullptr && !anim_comp->local_pose.empty()) {
						const std::uint32_t skin_offset = static_cast<std::uint32_t>(local_pose_staging.size());

						local_pose_staging.insert(local_pose_staging.end(), anim_comp->local_pose.begin(), anim_comp->local_pose.end());

						if (anim_comp->skeleton && (!s.current_skeleton.valid() || s.current_skeleton != anim_comp->skeleton)) {
							s.current_skeleton = anim_comp->skeleton;
							s.current_joint_count = static_cast<std::uint32_t>(anim_comp->skeleton->joint_count());
							r.upload_skeleton_data(*anim_comp->skeleton);
						}

						++skinned_instance_count;

						skinned_model_handle.update(*mc, *cc, skin_offset, s.current_joint_count);
						const auto entries = skinned_model_handle.render_queue_entries();
						skinned_out.append_range(entries);
					}
				}
			}
		}

		{
			trace::scope_guard sg{trace_id<"geom_collect::sort">()};
			std::ranges::sort(
				out,
				[](const owned_render_queue_entry& a, const owned_render_queue_entry& b) {
					const auto* ma = a.entry.model.resolve();
					const auto* mb = b.entry.model.resolve();

					if (ma != mb) {
						return ma < mb;
					}

					return a.entry.index < b.entry.index;
				}
			);

			std::ranges::sort(
				skinned_out,
				[](const skinned_render_queue_entry& a, const skinned_render_queue_entry& b) {
					const auto* ma = a.model.resolve();
					const auto* mb = b.model.resolve();

					if (ma != mb) {
						return ma < mb;
					}

					return a.index < b.index;
				}
			);
		}

		auto& instance_staging = data.instance_staging;
		auto& normal_batches = data.normal_batches;
		auto& skinned_batches = data.skinned_batches;

		instance_staging.reserve((out.size() + skinned_out.size()) * r.instance_stride);
		data.physics_mappings.reserve(out.size());

		auto material_index = [&data](const material* mat_ptr) -> std::uint32_t {
			if (auto it = data.material_palette_map.find(mat_ptr); it != data.material_palette_map.end()) {
				return it->second;
			}
			const auto index = static_cast<std::uint32_t>(data.material_palette_map.size());
			data.material_palette_map.insert({ mat_ptr, index });
			return index;
		};

		auto write_instance = [&instance_staging, &r](
			const mat4f& model_matrix,
			const mat4f& normal_matrix,
			const std::uint32_t skin_offset,
			const std::uint32_t joint_count,
			const std::uint32_t mat_idx
		) {
			const auto offset_value = instance_staging.size();
			instance_staging.resize(offset_value + r.instance_stride);
			std::byte* offset = instance_staging.data() + offset_value;

			memcpy(offset + r.instance_model_matrix_offset,   model_matrix);
			memcpy(offset + r.instance_normal_matrix_offset,  normal_matrix);
			memcpy(offset + r.instance_skin_offset_offset,    skin_offset);
			memcpy(offset + r.instance_joint_count_offset,    joint_count);
			memcpy(offset + r.instance_material_index_offset, mat_idx);
		};

		std::uint32_t global_instance_offset = 0;

		{
			trace::scope_guard sg{trace_id<"geom_collect::batch_normal">()};
		std::size_t batch_begin = 0;
		while (batch_begin < out.size()) {
			const auto& first = out[batch_begin].entry;
			const normal_batch_key key{
				.model_ptr = first.model.resolve(),
				.mesh_index = first.index
			};

			std::size_t batch_end = batch_begin + 1;
			while (batch_end < out.size()) {
				if (const auto& entry = out[batch_end].entry; entry.model.resolve() != key.model_ptr || entry.index != key.mesh_index) {
					break;
				}
				++batch_end;
			}

			const auto& mesh = key.model_ptr->meshes()[key.mesh_index];
			const auto instance_count = static_cast<std::uint32_t>(batch_end - batch_begin);
			const auto mat_idx = material_index(&mesh.material());

			vec3 world_aabb_min(meters(std::numeric_limits<float>::max()));
			vec3 world_aabb_max(meters(std::numeric_limits<float>::lowest()));

			for (std::size_t i = batch_begin; i < batch_end; ++i) {
				const auto& queue_entry = out[i];
				world_aabb_min = gse::min(world_aabb_min, queue_entry.world_aabb_min);
				world_aabb_max = gse::max(world_aabb_max, queue_entry.world_aabb_max);
			}

			normal_batches.push_back({
				.key = key,
				.first_instance = global_instance_offset,
				.instance_count = instance_count,
				.world_aabb_min = world_aabb_min,
				.world_aabb_max = world_aabb_max
			});

			const vec3 center_of_mass = key.model_ptr->center_of_mass();

			for (std::size_t i = batch_begin; i < batch_end; ++i) {
				const auto& queue_entry = out[i];
				const auto& entry = queue_entry.entry;
				const auto instance_index = global_instance_offset++;

				write_instance(entry.model_matrix, entry.normal_matrix, 0, 0, mat_idx);

				if (queue_entry.body_index != owned_render_queue_entry::invalid_body_index) {
					data.physics_mappings.push_back({
						.body_index = queue_entry.body_index,
						.instance_index = instance_index,
						.center_of_mass = center_of_mass
					});
				}
			}

			batch_begin = batch_end;
		}
		}

		{
			trace::scope_guard sg{trace_id<"geom_collect::batch_skinned">()};
		std::size_t batch_begin = 0;
		while (batch_begin < skinned_out.size()) {
			const auto& first = skinned_out[batch_begin];
			const skinned_batch_key key{
				.model_ptr = first.model.resolve(),
				.mesh_index = first.index
			};

			std::size_t batch_end = batch_begin + 1;
			while (batch_end < skinned_out.size()) {
				const auto& entry = skinned_out[batch_end];
				if (entry.model.resolve() != key.model_ptr || entry.index != key.mesh_index) {
					break;
				}
				++batch_end;
			}

			const auto& mesh = key.model_ptr->meshes()[key.mesh_index];
			const auto [local_aabb_min, local_aabb_max] = mesh.aabb();
			const auto instance_count = static_cast<std::uint32_t>(batch_end - batch_begin);
			const auto mat_idx = material_index(&mesh.material());

			vec3 world_aabb_min(meters(std::numeric_limits<float>::max()));
			vec3 world_aabb_max(meters(std::numeric_limits<float>::lowest()));

			for (std::size_t i = batch_begin; i < batch_end; ++i) {
				const auto& entry = skinned_out[i];
				const auto [inst_min, inst_max] = transform_aabb(local_aabb_min, local_aabb_max, entry.model_matrix);

				world_aabb_min = vec3<length>(
					std::min(world_aabb_min.x(), inst_min.x()),
					std::min(world_aabb_min.y(), inst_min.y()),
					std::min(world_aabb_min.z(), inst_min.z())
				);

				world_aabb_max = vec3<length>(
					std::max(world_aabb_max.x(), inst_max.x()),
					std::max(world_aabb_max.y(), inst_max.y()),
					std::max(world_aabb_max.z(), inst_max.z())
				);
			}

			skinned_batches.push_back({
				.key = key,
				.first_instance = global_instance_offset,
				.instance_count = instance_count,
				.world_aabb_min = world_aabb_min,
				.world_aabb_max = world_aabb_max
			});

			for (std::size_t i = batch_begin; i < batch_end; ++i) {
				const auto& entry = skinned_out[i];
				write_instance(entry.model_matrix, entry.normal_matrix, entry.skin_offset, entry.joint_count, mat_idx);
				++global_instance_offset;
			}

			batch_begin = batch_end;
		}
		}

		if (!local_pose_staging.empty() && s.current_joint_count > 0) {
			data.pending_compute_instance_count = skinned_instance_count;
		} else {
			data.pending_compute_instance_count = 0;
		}

		data.physics_mapping_count = static_cast<std::uint32_t>(data.physics_mappings.size());

		ctx.channels.push(std::move(data));
	}
}

auto gse::renderer::geometry_collector::system::frame(frame_context& ctx, const resources& r, const state& s) -> async::task<> {
	const auto& items = ctx.read_channel<render_data>();
	if (items.empty()) {
		ctx.notify_ready<state>();
		co_return;
	}

	const auto& data = items[0];
	const auto frame_index = r.ctx->graph().current_frame();

	const auto& cam_alloc = r.ubo_allocations.at("CameraUBO")[frame_index];
	r.shader_handle->set_uniform(cam_alloc.bytes(), "CameraUBO.view", data.view);
	r.shader_handle->set_uniform(cam_alloc.bytes(), "CameraUBO.proj", data.proj);

	if (!data.instance_staging.empty()) {
		gse::memcpy(r.instance_buffer[frame_index].mapped(), data.instance_staging);
	}

	if (!data.physics_mappings.empty()) {
		gse::memcpy(
			r.physics_mapping_buffer[frame_index].mapped(),
			data.physics_mappings.data(),
			data.physics_mappings.size() * sizeof(physics_mapping_entry)
		);
	}

	if (!data.normal_batches.empty()) {
		static_vector<gpu::draw_indexed_indirect_command, render_data::max_batches> normal_indirect_commands;

		for (const auto& batch : data.normal_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			normal_indirect_commands.push_back({
				.index_count = static_cast<std::uint32_t>(mesh.indices().size()),
				.instance_count = batch.instance_count,
				.first_index = 0,
				.vertex_offset = 0,
				.first_instance = batch.first_instance
			});
		}

		if (!normal_indirect_commands.empty()) {
			gse::memcpy(r.normal_indirect_commands_buffer[frame_index].mapped(), normal_indirect_commands);
		}
	}

	if (!data.skinned_batches.empty()) {
		static_vector<gpu::draw_indexed_indirect_command, render_data::max_batches> skinned_indirect_commands;

		for (const auto& batch : data.skinned_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			skinned_indirect_commands.push_back({
				.index_count = static_cast<std::uint32_t>(mesh.indices().size()),
				.instance_count = batch.instance_count,
				.first_index = 0,
				.vertex_offset = 0,
				.first_instance = batch.first_instance
			});
		}

		if (!skinned_indirect_commands.empty()) {
			gse::memcpy(r.skinned_indirect_commands_buffer[frame_index].mapped(), skinned_indirect_commands);
		}
	}

	if (!data.local_pose_staging.empty() && s.current_joint_count > 0) {
		gse::memcpy(r.local_pose_buffer[frame_index].mapped(), data.local_pose_staging);
	} else if (!data.skin_staging.empty()) {
		gse::memcpy(r.skin_buffer[frame_index].mapped(), data.skin_staging);
	}

	ctx.notify_ready<state>();
}


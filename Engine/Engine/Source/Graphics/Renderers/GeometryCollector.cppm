export module gse.graphics:geometry_collector;

import std;

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
import gse.utility;
import gse.platform;
import gse.physics;

export namespace gse::renderer {
	using frustum_planes = std::array<vec4f, 6>;

	auto transform_aabb(const vec3<length>& local_min, const vec3<length>& local_max, const mat4f& model_matrix) -> std::pair<vec3<length>, vec3<length>> {
		const std::array corners = {
			vec4<length>(local_min.x(), local_min.y(), local_min.z(), meters(1.0f)),
			vec4<length>(local_max.x(), local_min.y(), local_min.z(), meters(1.0f)),
			vec4<length>(local_min.x(), local_max.y(), local_min.z(), meters(1.0f)),
			vec4<length>(local_max.x(), local_max.y(), local_min.z(), meters(1.0f)),
			vec4<length>(local_min.x(), local_min.y(), local_max.z(), meters(1.0f)),
			vec4<length>(local_max.x(), local_min.y(), local_max.z(), meters(1.0f)),
			vec4<length>(local_min.x(), local_max.y(), local_max.z(), meters(1.0f)),
			vec4<length>(local_max.x(), local_max.y(), local_max.z(), meters(1.0f))
		};

		vec3 world_min(meters(std::numeric_limits<float>::max()));
		vec3 world_max(meters(std::numeric_limits<float>::lowest()));

		for (const auto& corner : corners) {
			const auto wc = model_matrix * corner;
			const vec3<length> world_pos(wc.x(), wc.y(), wc.z());
			world_min = min(world_min, world_pos);
			world_max = max(world_max, world_pos);
		}

		return { world_min, world_max };
	}

	auto extract_frustum_planes(const view_projection_matrix& vp) -> frustum_planes {
		frustum_planes planes;

		auto at = [&]<std::size_t C, std::size_t R>(std::integral_constant<std::size_t, C>, std::integral_constant<std::size_t, R>) {
			return internal::to_storage(vp.at<C, R>());
		};

		constexpr auto c0 = std::integral_constant<std::size_t, 0>{};
		constexpr auto c1 = std::integral_constant<std::size_t, 1>{};
		constexpr auto c2 = std::integral_constant<std::size_t, 2>{};
		constexpr auto c3 = std::integral_constant<std::size_t, 3>{};
		constexpr auto r0 = std::integral_constant<std::size_t, 0>{};
		constexpr auto r1 = std::integral_constant<std::size_t, 1>{};
		constexpr auto r2 = std::integral_constant<std::size_t, 2>{};
		constexpr auto r3 = std::integral_constant<std::size_t, 3>{};

		planes[0] = { at(c0,r3) + at(c0,r0), at(c1,r3) + at(c1,r0), at(c2,r3) + at(c2,r0), at(c3,r3) + at(c3,r0) };
		planes[1] = { at(c0,r3) - at(c0,r0), at(c1,r3) - at(c1,r0), at(c2,r3) - at(c2,r0), at(c3,r3) - at(c3,r0) };
		planes[2] = { at(c0,r3) + at(c0,r1), at(c1,r3) + at(c1,r1), at(c2,r3) + at(c2,r1), at(c3,r3) + at(c3,r1) };
		planes[3] = { at(c0,r3) - at(c0,r1), at(c1,r3) - at(c1,r1), at(c2,r3) - at(c2,r1), at(c3,r3) - at(c3,r1) };
		planes[4] = { at(c0,r3) + at(c0,r2), at(c1,r3) + at(c1,r2), at(c2,r3) + at(c2,r2), at(c3,r3) + at(c3,r2) };
		planes[5] = { at(c0,r3) - at(c0,r2), at(c1,r3) - at(c1,r2), at(c2,r3) - at(c2,r2), at(c3,r3) - at(c3,r2) };

		for (auto& plane : planes) {
			if (const float length = magnitude(plane); length > 0.0f) {
				plane = plane / length;
			}
		}

		return planes;
	}

	struct normal_batch_key {
		const model* model_ptr;
		std::size_t mesh_index;

		auto operator==(const normal_batch_key& other) const -> bool {
			return model_ptr == other.model_ptr && mesh_index == other.mesh_index;
		}
	};

	struct normal_batch_key_hash {
		auto operator()(const normal_batch_key& key) const -> std::size_t {
			return std::hash<const void*>{}(key.model_ptr) ^ std::hash<std::size_t>{}(key.mesh_index);
		}
	};

	struct normal_instance_batch {
		normal_batch_key key;
		std::uint32_t first_instance;
		std::uint32_t instance_count;
		vec3<length> world_aabb_min;
		vec3<length> world_aabb_max;
	};

	struct skinned_batch_key {
		const skinned_model* model_ptr;
		std::size_t mesh_index;

		auto operator==(const skinned_batch_key& other) const -> bool {
			return model_ptr == other.model_ptr && mesh_index == other.mesh_index;
		}
	};

	struct skinned_batch_key_hash {
		auto operator()(const skinned_batch_key& key) const -> std::size_t {
			return std::hash<const void*>{}(key.model_ptr) ^ std::hash<std::size_t>{}(key.mesh_index);
		}
	};

	struct skinned_instance_batch {
		skinned_batch_key key;
		std::uint32_t first_instance;
		std::uint32_t instance_count;
		vec3<length> world_aabb_min;
		vec3<length> world_aabb_max;
	};
}

export namespace gse::renderer::geometry_collector {
	struct physics_mapping_entry {
		std::uint32_t body_index;
		std::uint32_t instance_index;
		float center_of_mass[3];
		float _pad;
	};

	struct render_data {
		static constexpr std::size_t max_batches = 256;

		std::vector<render_queue_entry> render_queue;
		std::vector<id> render_queue_owners;
		std::vector<skinned_render_queue_entry> skinned_render_queue;
		static_vector<normal_instance_batch, max_batches>  normal_batches;
		static_vector<skinned_instance_batch, max_batches> skinned_batches;

		std::vector<std::byte> instance_staging;
		std::vector<mat4f> skin_staging;
		std::vector<mat4f> local_pose_staging;
		std::uint32_t pending_compute_instance_count = 0;

		std::vector<physics_mapping_entry> physics_mappings;
		std::uint32_t physics_mapping_count = 0;

		view_matrix view;
		projection_matrix proj;
	};

	struct state {
		gpu::context* ctx = nullptr;

		per_frame_resource<gpu::buffer> instance_buffer;

		std::uint32_t instance_stride = 0;
		std::unordered_map<std::string, std::uint32_t> instance_offsets;

		const skeleton* current_skeleton = nullptr;
		std::uint32_t current_joint_count = 0;
		std::uint32_t joint_stride = 0;
		std::unordered_map<std::string, std::uint32_t> joint_offsets;

		std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;

		static constexpr std::size_t max_instances = 4096;
		static constexpr std::size_t max_skin_matrices = 256 * 128;
		static constexpr std::size_t max_joints = 256;

		per_frame_resource<gpu::buffer> normal_indirect_commands_buffer;
		per_frame_resource<gpu::buffer> skinned_indirect_commands_buffer;
		gpu::buffer skeleton_buffer;
		per_frame_resource<gpu::buffer> local_pose_buffer;
		per_frame_resource<gpu::buffer> skin_buffer;

		resource::handle<shader> shader_handle;

		per_frame_resource<gpu::buffer> physics_mapping_buffer;

		state() = default;
	};

	auto upload_skeleton_data(
		const state& s,
		const skeleton& skel
	) -> void;

	auto filter_render_queue(
		const render_data& data,
		std::span<const id> exclude_ids
	) -> std::vector<render_queue_entry>;

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto prepare_render(const prepare_render_phase& phase, state& s) -> void;
	};
}

auto gse::renderer::geometry_collector::filter_render_queue(const render_data& data, const std::span<const id> exclude_ids) -> std::vector<render_queue_entry> {
	std::vector<render_queue_entry> result;
	result.reserve(data.render_queue.size());

	for (std::size_t i = 0; i < data.render_queue.size(); ++i) {
		const bool excluded = std::ranges::any_of(exclude_ids, [&](const id& ex) {
			return ex == data.render_queue_owners[i];
		});

		if (!excluded) {
			result.push_back(data.render_queue[i]);
		}
	}

	return result;
}

auto gse::renderer::geometry_collector::upload_skeleton_data(const state& s, const skeleton& skel) -> void {
	const auto joint_count = static_cast<std::size_t>(skel.joint_count());
	const auto joints = skel.joints();

	std::byte* buffer = s.skeleton_buffer.mapped();

	for (std::size_t i = 0; i < joint_count; ++i) {
		std::byte* offset = buffer + (i * s.joint_stride);

		const mat4f inverse_bind = joints[i].inverse_bind();
		const std::uint32_t parent_index = joints[i].parent_index();

		gse::memcpy(offset + s.joint_offsets.at("inverse_bind"), inverse_bind);
		gse::memcpy(offset + s.joint_offsets.at("parent_index"), parent_index);
	}
}

auto gse::renderer::geometry_collector::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	s.ctx = &ctx;

	s.shader_handle = ctx.get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	ctx.instantly_load(s.shader_handle);

	const auto camera_ubo = s.shader_handle->uniform_block("CameraUBO");

	const auto instance_block = s.shader_handle->uniform_block("instanceData");
	s.instance_stride = instance_block.size;
	for (const auto& [name, member] : instance_block.members) {
		s.instance_offsets[name] = member.offset;
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx.device_ref(), {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		constexpr std::size_t skin_buffer_size = state::max_skin_matrices * sizeof(mat4f);
		s.skin_buffer[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = skin_buffer_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
		const std::size_t instance_buffer_size = state::max_instances * 2 * s.instance_stride;
		s.instance_buffer[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = instance_buffer_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});

		constexpr std::size_t indirect_buffer_size = render_data::max_batches * sizeof(gpu::draw_indexed_indirect_command);
		s.normal_indirect_commands_buffer[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = indirect_buffer_size,
			.usage = gpu::buffer_flag::indirect | gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
		s.skinned_indirect_commands_buffer[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = indirect_buffer_size,
			.usage = gpu::buffer_flag::indirect | gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});

		constexpr std::size_t local_pose_size = state::max_skin_matrices * sizeof(mat4f);
		s.local_pose_buffer[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = local_pose_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});

		constexpr std::size_t mapping_buffer_size = state::max_instances * sizeof(physics_mapping_entry);
		s.physics_mapping_buffer[i] = gpu::create_buffer(ctx.device_ref(), {
			.size = mapping_buffer_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
	}

	auto skin_compute = ctx.get<shader>("Shaders/Compute/skin_compute");
	ctx.instantly_load(skin_compute);

	const auto joint_block = skin_compute->uniform_block("skeletonData");
	s.joint_stride = joint_block.size;
	for (const auto& [name, member] : joint_block.members) {
		s.joint_offsets[name] = member.offset;
	}

	s.skeleton_buffer = gpu::create_buffer(ctx.device_ref(), {
		.size = state::max_joints * s.joint_stride,
		.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
	});
}

auto gse::renderer::geometry_collector::system::update(update_phase& phase, state& s) -> void {
	if (phase.registry.view<render_component>().empty()) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const view_matrix view_matrix = cam_state ? cam_state->view_matrix : gse::view_matrix{};
	const projection_matrix proj_matrix = cam_state ? cam_state->projection_matrix : projection_matrix{};

	std::unordered_map<id, std::uint32_t> body_index_map;
	for (const auto& m : phase.read_channel<physics::gpu_body_index_map>()) {
		for (const auto& [eid, idx] : m.entries) {
			body_index_map[eid] = idx;
		}
	}

	phase.schedule([&s, &channels = phase.channels, view_matrix, proj_matrix, body_index_map = std::move(body_index_map)](
		chunk<render_component> render,
		chunk<const physics::motion_component> motion,
		chunk<const physics::collision_component> collision,
		chunk<const animation_component> anim
	) {
		render_data data;
		data.view = view_matrix;
		data.proj = proj_matrix;

		auto& out = data.render_queue;
		auto& owners_out = data.render_queue_owners;
		auto& skinned_out = data.skinned_render_queue;

		auto& skin_staging = data.skin_staging;
		auto& local_pose_staging = data.local_pose_staging;

		std::uint32_t skinned_instance_count = 0;

		for (auto& component : render) {
			if (!component.render) {
				continue;
			}

			const auto* mc = motion.find(component.owner_id());
			const auto* cc = collision.find(component.owner_id());

			for (auto& model_handle : component.model_instances) {
				if (!model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				model_handle.update(*mc, *cc);
				const auto entries = model_handle.render_queue_entries();
				out.append_range(entries);
				owners_out.resize(owners_out.size() + entries.size(), component.owner_id());
			}

			const auto* anim_comp = anim.find(component.owner_id());

			for (auto& skinned_model_handle : component.skinned_model_instances) {
				if (!skinned_model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				if (anim_comp != nullptr && !anim_comp->local_pose.empty()) {
					std::uint32_t skin_offset = 0;
					skin_offset = static_cast<std::uint32_t>(local_pose_staging.size());

					local_pose_staging.insert(local_pose_staging.end(), anim_comp->local_pose.begin(), anim_comp->local_pose.end());

					if (anim_comp->skeleton && (s.current_skeleton == nullptr || s.current_skeleton->id() != anim_comp->skeleton.id())) {
						s.current_skeleton = anim_comp->skeleton.resolve();
						s.current_joint_count = static_cast<std::uint32_t>(anim_comp->skeleton->joint_count());
						upload_skeleton_data(s, *anim_comp->skeleton);
					}

					++skinned_instance_count;

					skinned_model_handle.update(*mc, *cc, skin_offset, s.current_joint_count);
					const auto entries = skinned_model_handle.render_queue_entries();
					skinned_out.append_range(entries);
				}
			}
		}

		std::ranges::sort(
			out,
			[](const render_queue_entry& a, const render_queue_entry& b) {
				const auto* ma = a.model.resolve();
				const auto* mb = b.model.resolve();

				if (ma != mb) {
					return ma < mb;
				}

				return a.index < b.index;
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

	struct instance_data {
		mat4f model_matrix;
		mat4f normal_matrix;
		std::uint32_t skin_offset;
		std::uint32_t joint_count;
		id owner;
	};

	auto& instance_staging = data.instance_staging;
	auto& normal_batches = data.normal_batches;
	auto& skinned_batches = data.skinned_batches;

	std::uint32_t global_instance_offset = 0;

	if (!out.empty()) {
		std::unordered_map<normal_batch_key, std::vector<instance_data>, normal_batch_key_hash> normal_batch_map;

		for (std::size_t i = 0; i < out.size(); ++i) {
			const auto& entry = out[i];
			const normal_batch_key key{
				.model_ptr = entry.model.resolve(),
				.mesh_index = entry.index
			};

			instance_data inst{
				.model_matrix = entry.model_matrix,
				.normal_matrix = entry.normal_matrix,
				.skin_offset = 0,
				.joint_count = 0,
				.owner = owners_out[i]
			};

			normal_batch_map[key].push_back(inst);
		}

		for (auto& [key, instances] : normal_batch_map) {
			const auto& mesh = key.model_ptr->meshes()[key.mesh_index];
			const auto [local_aabb_min, local_aabb_max] = mesh.aabb();
			const std::uint32_t instance_count = static_cast<std::uint32_t>(instances.size());

			vec3 world_aabb_min(meters(std::numeric_limits<float>::max()));
			vec3 world_aabb_max(meters(std::numeric_limits<float>::lowest()));

			for (const auto& inst : instances) {
				const auto [inst_min, inst_max] = transform_aabb(local_aabb_min, local_aabb_max, inst.model_matrix);

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

			normal_instance_batch batch{
				.key = key,
				.first_instance = global_instance_offset,
				.instance_count = instance_count,
				.world_aabb_min = world_aabb_min,
				.world_aabb_max = world_aabb_max
			};

			const vec3 center_of_mass = key.model_ptr->center_of_mass();

			for (const auto& inst : instances) {
				std::byte* offset = instance_staging.data() + (global_instance_offset * s.instance_stride);
				instance_staging.resize(instance_staging.size() + s.instance_stride);
				offset = instance_staging.data() + (global_instance_offset * s.instance_stride);

				gse::memcpy(offset + s.instance_offsets["model_matrix"], inst.model_matrix);
				gse::memcpy(offset + s.instance_offsets["normal_matrix"], inst.normal_matrix);
				gse::memcpy(offset + s.instance_offsets["skin_offset"], inst.skin_offset);
				gse::memcpy(offset + s.instance_offsets["joint_count"], inst.joint_count);

				if (const auto it = body_index_map.find(inst.owner); it != body_index_map.end()) {
					data.physics_mappings.push_back({
						.body_index = it->second,
						.instance_index = global_instance_offset,
						.center_of_mass = {
							center_of_mass.x().as<meters>(),
							center_of_mass.y().as<meters>(),
							center_of_mass.z().as<meters>()
						},
						._pad = 0.f
					});
				}

				global_instance_offset++;
			}
			normal_batches.push_back(std::move(batch));
		}
	}

	if (!skinned_out.empty()) {
		std::unordered_map<skinned_batch_key, std::vector<instance_data>, skinned_batch_key_hash> skinned_batch_map;

		for (const auto& entry : skinned_out) {
			const skinned_batch_key key{
				.model_ptr = entry.model.resolve(),
				.mesh_index = entry.index
			};

			instance_data inst{
				.model_matrix = entry.model_matrix,
				.normal_matrix = entry.normal_matrix,
				.skin_offset = entry.skin_offset,
				.joint_count = entry.joint_count
			};

			skinned_batch_map[key].push_back(inst);
		}

		for (auto& [key, instances] : skinned_batch_map) {
			const auto& mesh = key.model_ptr->meshes()[key.mesh_index];
			const auto [local_aabb_min, local_aabb_max] = mesh.aabb();
			const std::uint32_t instance_count = static_cast<std::uint32_t>(instances.size());

			vec3 world_aabb_min(meters(std::numeric_limits<float>::max()));
			vec3 world_aabb_max(meters(std::numeric_limits<float>::lowest()));

			for (const auto& inst : instances) {
				const auto [inst_min, inst_max] = transform_aabb(local_aabb_min, local_aabb_max, inst.model_matrix);

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

			skinned_instance_batch batch{
				.key = key,
				.first_instance = global_instance_offset,
				.instance_count = instance_count,
				.world_aabb_min = world_aabb_min,
				.world_aabb_max = world_aabb_max
			};

			for (const auto& [model_matrix, normal_matrix, skin_offset, joint_count, owner] : instances) {
				std::byte* offset = instance_staging.data() + (global_instance_offset * s.instance_stride);
				instance_staging.resize(instance_staging.size() + s.instance_stride);
				offset = instance_staging.data() + (global_instance_offset * s.instance_stride);

				gse::memcpy(offset + s.instance_offsets["model_matrix"], model_matrix);
				gse::memcpy(offset + s.instance_offsets["normal_matrix"], normal_matrix);
				gse::memcpy(offset + s.instance_offsets["skin_offset"], skin_offset);
				gse::memcpy(offset + s.instance_offsets["joint_count"], joint_count);

				global_instance_offset++;
			}

			skinned_batches.push_back(std::move(batch));
		}
	}

	if (!local_pose_staging.empty() && s.current_joint_count > 0) {
		data.pending_compute_instance_count = skinned_instance_count;
	} else {
		data.pending_compute_instance_count = 0;
	}

	data.physics_mapping_count = static_cast<std::uint32_t>(data.physics_mappings.size());

		channels.push(std::move(data));
	});
}

auto gse::renderer::geometry_collector::system::prepare_render(const prepare_render_phase& phase, state& s) -> void {
	const auto& items = phase.read_channel<render_data>();
	if (items.empty()) {
		return;
	}

	const auto& data = items[0];
	const auto frame_index = s.ctx->graph().current_frame();

	const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index];
	s.shader_handle->set_uniform("CameraUBO.view", data.view, cam_alloc);
	s.shader_handle->set_uniform("CameraUBO.proj", data.proj, cam_alloc);

	if (!data.instance_staging.empty()) {
		gse::memcpy(s.instance_buffer[frame_index].mapped(), data.instance_staging);
	}

	if (!data.physics_mappings.empty()) {
		gse::memcpy(
			s.physics_mapping_buffer[frame_index].mapped(),
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
			gse::memcpy(s.normal_indirect_commands_buffer[frame_index].mapped(), normal_indirect_commands);
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
			gse::memcpy(s.skinned_indirect_commands_buffer[frame_index].mapped(), skinned_indirect_commands);
		}
	}

	if (!data.local_pose_staging.empty() && s.current_joint_count > 0) {
		gse::memcpy(s.local_pose_buffer[frame_index].mapped(), data.local_pose_staging);
	} else if (!data.skin_staging.empty()) {
		gse::memcpy(s.skin_buffer[frame_index].mapped(), data.skin_staging);
	}
}

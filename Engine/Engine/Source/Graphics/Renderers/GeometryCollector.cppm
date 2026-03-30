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

		vec3<length> world_min(meters(std::numeric_limits<float>::max()));
		vec3<length> world_max(meters(std::numeric_limits<float>::lowest()));

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
	struct render_data {
		std::uint32_t frame_index = 0;
		std::vector<render_queue_entry> render_queue;
		std::vector<id> render_queue_owners;
		std::vector<skinned_render_queue_entry> skinned_render_queue;
		std::vector<normal_instance_batch> normal_batches;
		std::vector<skinned_instance_batch> skinned_batches;
		std::uint32_t pending_compute_instance_count = 0;
	};

	struct state {
		gpu::context* ctx = nullptr;

		per_frame_resource<vulkan::buffer_resource> instance_buffer;
		per_frame_resource<std::vector<std::byte>> instance_staging;
		std::uint32_t instance_stride = 0;
		std::unordered_map<std::string, std::uint32_t> instance_offsets;

		per_frame_resource<std::vector<mat4f>> skin_staging;
		per_frame_resource<std::vector<mat4f>> local_pose_staging;
		const skeleton* current_skeleton = nullptr;
		std::uint32_t current_joint_count = 0;
		std::uint32_t joint_stride = 0;
		std::unordered_map<std::string, std::uint32_t> joint_offsets;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> ubo_allocations;

		static constexpr std::size_t max_instances = 4096;
		static constexpr std::size_t max_skin_matrices = 256 * 128;
		static constexpr std::size_t max_joints = 256;
		static constexpr std::size_t max_batches = 256;

		per_frame_resource<vulkan::buffer_resource> normal_indirect_commands_buffer;
		per_frame_resource<vulkan::buffer_resource> skinned_indirect_commands_buffer;
		vulkan::buffer_resource skeleton_buffer;
		per_frame_resource<vulkan::buffer_resource> local_pose_buffer;
		per_frame_resource<vulkan::buffer_resource> skin_buffer;

		resource::handle<shader> shader_handle;

		explicit state(gpu::context& c) : ctx(std::addressof(c)) {}
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

	std::byte* buffer = s.skeleton_buffer.allocation.mapped();

	for (std::size_t i = 0; i < joint_count; ++i) {
		std::byte* offset = buffer + (i * s.joint_stride);

		const mat4f inverse_bind = joints[i].inverse_bind();
		const std::uint32_t parent_index = joints[i].parent_index();

		gse::memcpy(offset + s.joint_offsets.at("inverse_bind"), inverse_bind);
		gse::memcpy(offset + s.joint_offsets.at("parent_index"), parent_index);
	}
}

auto gse::renderer::geometry_collector::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& config = s.ctx->config();

	s.shader_handle = s.ctx->get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	s.ctx->instantly_load(s.shader_handle);

	const auto camera_ubo = s.shader_handle->uniform_block("CameraUBO");

	const auto instance_block = s.shader_handle->uniform_block("instanceData");
	s.instance_stride = instance_block.size;
	for (const auto& [name, member] : instance_block.members) {
		s.instance_offsets[name] = member.offset;
	}

	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = config.allocator().create_buffer({
			.size = camera_ubo.size,
			.usage = vk::BufferUsageFlagBits::eUniformBuffer,
			.sharingMode = vk::SharingMode::eExclusive
		});

		constexpr vk::DeviceSize skin_buffer_size = state::max_skin_matrices * sizeof(mat4f);
		s.skin_buffer[i] = config.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = skin_buffer_size,
				.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
			}
		);
		s.skin_staging[i].reserve(state::max_skin_matrices);

		const vk::DeviceSize instance_buffer_size = state::max_instances * 2 * s.instance_stride;
		s.instance_buffer[i] = config.allocator().create_buffer({
			.size = instance_buffer_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		s.instance_staging[i].reserve(instance_buffer_size);

		constexpr vk::DeviceSize indirect_buffer_size = state::max_batches * sizeof(vk::DrawIndexedIndirectCommand);
		s.normal_indirect_commands_buffer[i] = config.allocator().create_buffer({
			.size = indirect_buffer_size,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		s.skinned_indirect_commands_buffer[i] = config.allocator().create_buffer({
			.size = indirect_buffer_size,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});

		constexpr vk::DeviceSize local_pose_size = state::max_skin_matrices * sizeof(mat4f);
		s.local_pose_buffer[i] = config.allocator().create_buffer({
			.size = local_pose_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		s.local_pose_staging[i].reserve(state::max_skin_matrices);
	}

	auto skin_compute = s.ctx->get<shader>("Shaders/Compute/skin_compute");
	s.ctx->instantly_load(skin_compute);

	const auto joint_block = skin_compute->uniform_block("skeletonData");
	s.joint_stride = joint_block.size;
	for (const auto& [name, member] : joint_block.members) {
		s.joint_offsets[name] = member.offset;
	}

	s.skeleton_buffer = config.allocator().create_buffer({
		.size = state::max_joints * s.joint_stride,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
	});
}

auto gse::renderer::geometry_collector::system::update(update_phase& phase, state& s) -> void {
	if (phase.registry.view<render_component>().empty()) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const view_matrix view_matrix = cam_state ? cam_state->view_matrix : gse::view_matrix{};
	const projection_matrix proj_matrix = cam_state ? cam_state->projection_matrix : projection_matrix{};

	phase.schedule([&s, &channels = phase.channels, view_matrix, proj_matrix](
		chunk<render_component> render,
		chunk<const physics::motion_component> motion,
		chunk<const physics::collision_component> collision,
		chunk<const animation_component> anim
	) {
		const auto frame_index = s.ctx->graph().current_frame();

		const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index].allocation;
		s.shader_handle->set_uniform("CameraUBO.view", view_matrix, cam_alloc);
		s.shader_handle->set_uniform("CameraUBO.proj", proj_matrix, cam_alloc);

		render_data data;
		data.frame_index = frame_index;

		auto& out = data.render_queue;
		auto& owners_out = data.render_queue_owners;
		auto& skinned_out = data.skinned_render_queue;

		auto& skin_staging = s.skin_staging[frame_index];
		skin_staging.clear();

		auto& local_pose_staging = s.local_pose_staging[frame_index];
		local_pose_staging.clear();

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
	};

	auto& instance_staging = s.instance_staging[frame_index];
	auto& normal_batches = data.normal_batches;
	auto& skinned_batches = data.skinned_batches;

	instance_staging.clear();

	std::uint32_t global_instance_offset = 0;

	if (!out.empty()) {
		std::unordered_map<normal_batch_key, std::vector<instance_data>, normal_batch_key_hash> normal_batch_map;

		for (const auto& entry : out) {
			const normal_batch_key key{
				.model_ptr = entry.model.resolve(),
				.mesh_index = entry.index
			};

			instance_data inst{
				.model_matrix = entry.model_matrix,
				.normal_matrix = entry.normal_matrix,
				.skin_offset = 0,
				.joint_count = 0
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

			for (const auto& [model_matrix, normal_matrix, skin_offset, joint_count] : instances) {
				std::byte* offset = instance_staging.data() + (global_instance_offset * s.instance_stride);
				instance_staging.resize(instance_staging.size() + s.instance_stride);
				offset = instance_staging.data() + (global_instance_offset * s.instance_stride);

				gse::memcpy(offset + s.instance_offsets["model_matrix"], model_matrix);
				gse::memcpy(offset + s.instance_offsets["normal_matrix"], normal_matrix);
				gse::memcpy(offset + s.instance_offsets["skin_offset"], skin_offset);
				gse::memcpy(offset + s.instance_offsets["joint_count"], joint_count);

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

			for (const auto& [model_matrix, normal_matrix, skin_offset, joint_count] : instances) {
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

	if (!instance_staging.empty()) {
		gse::memcpy(s.instance_buffer[frame_index].allocation.mapped(), instance_staging);
	}

	if (!normal_batches.empty()) {
		std::vector<vk::DrawIndexedIndirectCommand> normal_indirect_commands;
		normal_indirect_commands.reserve(normal_batches.size());

		for (const auto& batch : normal_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			vk::DrawIndexedIndirectCommand cmd{
				.indexCount = static_cast<std::uint32_t>(mesh.indices().size()),
				.instanceCount = batch.instance_count,
				.firstIndex = 0,
				.vertexOffset = 0,
				.firstInstance = batch.first_instance
			};

			normal_indirect_commands.push_back(cmd);
		}

		if (!normal_indirect_commands.empty()) {
			gse::memcpy(s.normal_indirect_commands_buffer[frame_index].allocation.mapped(), normal_indirect_commands);
		}
	}

	if (!skinned_batches.empty()) {
		std::vector<vk::DrawIndexedIndirectCommand> skinned_indirect_commands;
		skinned_indirect_commands.reserve(skinned_batches.size());

		for (const auto& batch : skinned_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			vk::DrawIndexedIndirectCommand cmd{
				.indexCount = static_cast<std::uint32_t>(mesh.indices().size()),
				.instanceCount = batch.instance_count,
				.firstIndex = 0,
				.vertexOffset = 0,
				.firstInstance = batch.first_instance
			};

			skinned_indirect_commands.push_back(cmd);
		}

		if (!skinned_indirect_commands.empty()) {
			gse::memcpy(s.skinned_indirect_commands_buffer[frame_index].allocation.mapped(), skinned_indirect_commands);
		}
	}

	if (!local_pose_staging.empty() && s.current_joint_count > 0) {
		gse::memcpy(s.local_pose_buffer[frame_index].allocation.mapped(), local_pose_staging);
		data.pending_compute_instance_count = skinned_instance_count;
	} else if (!skin_staging.empty()) {
		gse::memcpy(s.skin_buffer[frame_index].allocation.mapped(), skin_staging);
		data.pending_compute_instance_count = 0;
	} else {
		data.pending_compute_instance_count = 0;
	}

		channels.push(std::move(data));
	});
}

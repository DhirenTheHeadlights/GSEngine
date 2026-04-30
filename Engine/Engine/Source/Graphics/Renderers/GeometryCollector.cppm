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
		vec3<length> center_of_mass;
	};

	struct owned_render_queue_entry {
		static constexpr std::uint32_t invalid_body_index = std::numeric_limits<std::uint32_t>::max();

		render_queue_entry entry;
		id owner;
		vec3<position> world_aabb_min;
		vec3<position> world_aabb_max;
		std::uint32_t body_index = invalid_body_index;
	};

	struct render_data {
		static constexpr std::size_t max_batches = 256;

		std::vector<owned_render_queue_entry> render_queue;
		std::vector<skinned_render_queue_entry> skinned_render_queue;
		static_vector<normal_instance_batch, max_batches>  normal_batches;
		static_vector<skinned_instance_batch, max_batches> skinned_batches;

		std::vector<std::byte> instance_staging;
		std::vector<mat4f> skin_staging;
		std::vector<mat4f> local_pose_staging;
		std::uint32_t pending_compute_instance_count = 0;

		std::vector<physics_mapping_entry> physics_mappings;
		std::uint32_t physics_mapping_count = 0;

		flat_map<const material*, std::uint32_t> material_palette_map;

		view_matrix view;
		projection_matrix proj;
	};

	struct state {
		resource::handle<skeleton> current_skeleton;
		std::uint32_t current_joint_count = 0;
	};

	auto filter_render_queue(
		const render_data& data,
		std::span<const id> exclude_ids
	) -> std::vector<render_queue_entry>;

	struct system {
		struct resources {
			gpu::context* ctx = nullptr;

			per_frame_resource<gpu::buffer> instance_buffer;

			std::uint32_t instance_stride = 0;
			std::unordered_map<std::string, std::uint32_t> instance_offsets;

			std::uint32_t instance_model_matrix_offset = 0;
			std::uint32_t instance_normal_matrix_offset = 0;
			std::uint32_t instance_skin_offset_offset = 0;
			std::uint32_t instance_joint_count_offset = 0;
			std::uint32_t instance_material_index_offset = 0;

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

			auto upload_skeleton_data(const skeleton& skel) const -> void;
		};

		static auto initialize(init_context& phase, resources& r, state& s) -> void;
		static auto update(update_context& ctx, const resources& r, state& s) -> async::task<>;
		static auto frame(frame_context& ctx, const resources& r, const state& s) -> async::task<>;
	};
}


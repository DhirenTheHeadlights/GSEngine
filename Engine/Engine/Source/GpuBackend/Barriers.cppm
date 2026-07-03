export module gse.gpu_backend:barriers;

import std;

import :core;
import :enums;
import :sync;
import :buffer;
import :image;

export namespace gse::gpu {
	enum class resource_state : std::uint8_t {
		undefined,
		common,
		color_target,
		depth_write,
		depth_read,
		sampled,
		storage_read,
		storage_write,
		storage_read_write,
		copy_src,
		copy_dst,
		indirect,
		present,
		acceleration_structure_read,
		acceleration_structure_build,
	};

	[[nodiscard]] auto state_of(
		access_flags access
	) -> resource_state;

	struct buffer_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		gpu::handle<gpu::buffer> buffer;
		device_size offset = 0;
		device_size size = 0;
	};

	struct image_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		resource_state prev_state = resource_state::undefined;
		resource_state next_state = resource_state::undefined;
		bool discard_contents = false;
		gpu::handle<gpu::image> image;
		image_aspect_flags aspects;
		std::uint32_t base_mip_level = 0;
		std::uint32_t level_count = 1;
		std::uint32_t base_array_layer = 0;
		std::uint32_t layer_count = 1;
	};

	struct dependency_info {
		std::span<const memory_barrier> memory_barriers;
		std::span<const buffer_barrier> buffer_barriers;
		std::span<const image_barrier> image_barriers;
	};
}

auto gse::gpu::state_of(const access_flags access) -> resource_state {
	if (access.test(access_flag::depth_stencil_attachment_write)) {
		return resource_state::depth_write;
	}
	if (access.test(access_flag::depth_stencil_attachment_read)) {
		return resource_state::depth_read;
	}
	if (access.test(access_flag::color_attachment_write) || access.test(access_flag::color_attachment_read)) {
		return resource_state::color_target;
	}
	const bool storage_write = access.test(access_flag::shader_storage_write) || access.test(access_flag::shader_write);
	const bool storage_read = access.test(access_flag::shader_storage_read);
	if (storage_write) {
		return storage_read ? resource_state::storage_read_write : resource_state::storage_write;
	}
	if (storage_read) {
		return resource_state::storage_read;
	}
	if (access.test(access_flag::transfer_write)) {
		return resource_state::copy_dst;
	}
	if (access.test(access_flag::transfer_read)) {
		return resource_state::copy_src;
	}
	if (access.test(access_flag::acceleration_structure_write)) {
		return resource_state::acceleration_structure_build;
	}
	if (access.test(access_flag::acceleration_structure_read)) {
		return resource_state::acceleration_structure_read;
	}
	if (access.test(access_flag::indirect_command_read)) {
		return resource_state::indirect;
	}
	if (access.test(access_flag::shader_sampled_read) || access.test(access_flag::shader_read) || access.test(access_flag::uniform_read)) {
		return resource_state::sampled;
	}
	return resource_state::common;
}

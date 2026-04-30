export module gse.gpu:acceleration_structure;

import std;
import vulkan;

import :context;
import :device;
import :gpu_task;
import :handles;
import :transient_api;
import :types;
import :vulkan_acceleration_structure;
import :vulkan_buffer;
import :vulkan_commands;
import :vulkan_device;
import :render_graph;

import gse.core;
import gse.concurrency;
import gse.math;

export namespace gse::gpu {
	struct blas_geometry_desc {
		const vulkan::buffer* vertex_buffer = nullptr;
		std::uint32_t vertex_count = 0;
		std::uint32_t vertex_stride = 0;
		const vulkan::buffer* index_buffer = nullptr;
		std::uint32_t index_count = 0;
	};

	struct tlas_instance_desc {
		mat4f transform = mat4(1.0f);
		std::uint32_t custom_index = 0;
		std::uint8_t mask = 0xFF;
		std::uint32_t sbt_offset = 0;
		bool cull_disable = true;
		std::uint64_t blas_address = 0;
	};

	auto build_blas(
		context& ctx,
		const blas_geometry_desc& desc
	) -> vulkan::blas;

	auto build_tlas(
		context& ctx,
		std::uint32_t max_instances
	) -> vulkan::tlas;

	auto rebuild_tlas(
		context& ctx,
		vulkan::tlas& t,
		std::span<const tlas_instance_desc> instances,
		vulkan::recording_context& rec
	) -> void;

	auto write_tlas_instances(
		vulkan::tlas& t,
		std::span<const tlas_instance_desc> instances
	) -> void;

	auto build_tlas_in_place(
		context& ctx,
		vulkan::tlas& t,
		std::uint32_t instance_count,
		vulkan::recording_context& rec
	) -> void;
}

namespace gse {
	auto to_vk_instance(
		const gpu::tlas_instance_desc& inst
	) -> vk::AccelerationStructureInstanceKHR;

	auto build_blas_async(
		gpu::device& dev,
		gpu::acceleration_structure_handle as_handle,
		gpu::acceleration_structure_geometry geometry,
		std::uint32_t prim_count,
		gpu::device_size scratch_size,
		gpu::device_size scratch_alignment
	) -> async::task<>;
}


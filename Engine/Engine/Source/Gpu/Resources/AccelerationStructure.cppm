export module gse.gpu:acceleration_structure;

import std;

import :device;
import :gpu_task;
import :render_graph;

import gse.gpu_backend;
import gse.core;
import gse.concurrency;

export namespace gse::gpu {
	struct blas_geometry_desc {
		const buffer* vertex_buffer = nullptr;
		std::uint32_t vertex_count = 0;
		std::uint32_t vertex_stride = 0;
		const buffer* index_buffer = nullptr;
		std::uint32_t index_count = 0;
	};

	auto build_blas(
		gpu::device& device,
		const blas_geometry_desc& desc
	) -> blas;

	auto build_tlas(
		gpu::device& device,
		std::uint32_t max_instances
	) -> tlas;

	auto rebuild_tlas(
		gpu::device& device,
		tlas& t,
		std::span<const tlas_instance_desc> instances,
		gpu::recording_context& rec
	) -> void;

	auto write_tlas_instances(
		tlas& t,
		std::span<const tlas_instance_desc> instances
	) -> void;

	auto build_tlas_in_place(
		gpu::device& device,
		tlas& t,
		std::uint32_t instance_count,
		gpu::recording_context& rec
	) -> void;
}

namespace gse {
	auto build_blas_async(
		gpu::device& dev,
		gpu::acceleration_structure as_handle,
		gpu::acceleration_structure_geometry geometry,
		std::uint32_t prim_count,
		gpu::device_size scratch_size,
		gpu::device_size scratch_alignment
	) -> async::task<>;

	auto build_tlas_initial_empty_async(
		gpu::device& dev,
		gpu::acceleration_structure as_handle,
		gpu::device_address instance_addr,
		gpu::device_address scratch_addr
	) -> async::task<>;
}

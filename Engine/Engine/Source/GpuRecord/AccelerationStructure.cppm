export module gse.gpu_record:acceleration_structure;

import std;

import gse.gpu;
import :recording_context;

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

	[[nodiscard]] auto make_blas_geometry(
		const blas_geometry_desc& desc
	) -> acceleration_structure_geometry;

	auto build_blas_in_place(
		gpu::device& device,
		acceleration_structure dst,
		const acceleration_structure_geometry& geometry,
		std::uint32_t prim_count,
		const buffer& scratch,
		gpu::recording_context& rec
	) -> void;

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

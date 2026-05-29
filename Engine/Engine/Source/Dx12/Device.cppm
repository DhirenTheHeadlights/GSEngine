export module gse.dx12:device;

import std;

import gse.gpu_types;
import gse.core;
import gse.math;

import :resources;
import :sync;
import :accel;
import :bindless;

export namespace gse::dx12 {
	class device final : public non_copyable {
	public:
		device() = default;

		~device() override = default;

		device(
			device&&
		) noexcept = default;

		auto operator=(
			device&&
		) noexcept -> device& = default;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

		[[nodiscard]] auto surface_format() const -> gpu::image_format;

		[[nodiscard]] auto queue_family(
			gpu::queue_type queue
		) const -> std::uint32_t;

		[[nodiscard]] auto create_buffer(
			const gpu::buffer_desc& info,
			const void* data = nullptr,
			std::string_view tag = "",
			const std::source_location& loc = std::source_location::current()
		) -> buffer;

		[[nodiscard]] auto create_image(
			const gpu::image_desc& desc,
			std::string_view tag = ""
		) -> image;

		[[nodiscard]] auto create_sampler(
			const gpu::sampler_desc& desc
		) -> sampler;

		[[nodiscard]] auto create_image_view(
			gpu::image_handle img,
			const gpu::image_view_create_info& info
		) const -> gpu::image_view_handle;

		[[nodiscard]] auto buffer_device_address(
			gpu::buffer_handle buf
		) const -> gpu::device_address;

		auto destroy_buffer(
			gpu::buffer_handle buf
		) const -> void;

		auto destroy_image(
			gpu::image_handle img
		) const -> void;

		auto destroy_image_view(
			gpu::image_view_handle view
		) const -> void;

		[[nodiscard]] auto create_buffer_unbound(
			const gpu::buffer_desc& info
		) const -> std::pair<gpu::buffer_handle, gpu::memory_requirements>;

		[[nodiscard]] auto create_image_unbound(
			const gpu::image_create_info& info
		) const -> std::pair<gpu::image_handle, gpu::memory_requirements>;

		auto bind_buffer_memory(
			gpu::buffer_handle buf,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		auto bind_image_memory(
			gpu::image_handle img,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		[[nodiscard]] auto allocate_aliased_memory(
			gpu::device_size size,
			std::uint32_t memory_type_index
		) const -> gpu::device_memory;

		auto free_aliased_memory(
			gpu::device_memory mem
		) const -> void;

		[[nodiscard]] auto find_memory_type_index(
			std::uint32_t type_bits,
			gpu::memory_property_flags required
		) const -> std::uint32_t;

		auto host_upload_image_layers(
			gpu::image_handle img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		[[nodiscard]] auto create_sync(
			std::uint32_t image_count,
			std::uint32_t frames_in_flight = 2
		) const -> sync;

		[[nodiscard]] auto create_timeline_semaphore(
			std::uint64_t initial_value
		) const -> semaphore;

		[[nodiscard]] auto create_timestamp_query_pool(
			std::uint32_t capacity,
			std::string_view label = {}
		) const -> query_pool;

		[[nodiscard]] auto create_pipeline_stats_query_pool(
			std::uint32_t capacity,
			gpu::pipeline_statistic_flags statistics,
			std::string_view label = {}
		) const -> query_pool;

		[[nodiscard]] auto create_bindless_heaps(
			std::uint32_t texture_capacity = 2048,
			std::uint32_t image_capacity = 65536,
			std::uint32_t buffer_capacity = 16384,
			std::uint32_t sampler_capacity = 512
		) const -> std::unique_ptr<bindless_heaps>;

		[[nodiscard]] auto create_blas(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> blas;

		[[nodiscard]] auto create_tlas(
			std::uint32_t max_instances
		) -> tlas;

		[[nodiscard]] auto query_blas_build_sizes(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) const -> gpu::acceleration_structure_build_sizes;

		[[nodiscard]] auto acceleration_structure_scratch_alignment() const -> gpu::device_size;

		[[nodiscard]] auto wait_for_fence(
			gpu::fence_handle f,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::result;

		auto reset_fence(
			gpu::fence_handle f
		) const -> void;
	};
}

auto gse::dx12::device::wait_idle() const -> void {
}

auto gse::dx12::device::timestamp_period() const -> float {
	return 1.0f;
}

auto gse::dx12::device::surface_format() const -> gpu::image_format {
	return gpu::image_format::b8g8r8a8_unorm;
}

auto gse::dx12::device::queue_family(const gpu::queue_type) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::create_buffer(const gpu::buffer_desc&, const void*, const std::string_view, const std::source_location&) -> buffer {
	return {};
}

auto gse::dx12::device::create_image(const gpu::image_desc&, const std::string_view) -> image {
	return {};
}

auto gse::dx12::device::create_sampler(const gpu::sampler_desc&) -> sampler {
	return {};
}

auto gse::dx12::device::create_image_view(const gpu::image_handle, const gpu::image_view_create_info&) const -> gpu::image_view_handle {
	return {};
}

auto gse::dx12::device::buffer_device_address(const gpu::buffer_handle) const -> gpu::device_address {
	return 0;
}

auto gse::dx12::device::destroy_buffer(const gpu::buffer_handle) const -> void {
}

auto gse::dx12::device::destroy_image(const gpu::image_handle) const -> void {
}

auto gse::dx12::device::destroy_image_view(const gpu::image_view_handle) const -> void {
}

auto gse::dx12::device::create_buffer_unbound(const gpu::buffer_desc&) const -> std::pair<gpu::buffer_handle, gpu::memory_requirements> {
	return {};
}

auto gse::dx12::device::create_image_unbound(const gpu::image_create_info&) const -> std::pair<gpu::image_handle, gpu::memory_requirements> {
	return {};
}

auto gse::dx12::device::bind_buffer_memory(const gpu::buffer_handle, const gpu::device_memory, const gpu::device_size) const -> void {
}

auto gse::dx12::device::bind_image_memory(const gpu::image_handle, const gpu::device_memory, const gpu::device_size) const -> void {
}

auto gse::dx12::device::allocate_aliased_memory(const gpu::device_size, const std::uint32_t) const -> gpu::device_memory {
	return {};
}

auto gse::dx12::device::free_aliased_memory(const gpu::device_memory) const -> void {
}

auto gse::dx12::device::find_memory_type_index(const std::uint32_t, const gpu::memory_property_flags) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::host_upload_image_layers(const gpu::image_handle, const std::span<const void* const>, const vec2u) const -> void {
}

auto gse::dx12::device::create_sync(const std::uint32_t, const std::uint32_t) const -> sync {
	return {};
}

auto gse::dx12::device::create_timeline_semaphore(const std::uint64_t) const -> semaphore {
	return {};
}

auto gse::dx12::device::create_timestamp_query_pool(const std::uint32_t, const std::string_view) const -> query_pool {
	return {};
}

auto gse::dx12::device::create_pipeline_stats_query_pool(const std::uint32_t, const gpu::pipeline_statistic_flags, const std::string_view) const -> query_pool {
	return {};
}

auto gse::dx12::device::create_bindless_heaps(const std::uint32_t, const std::uint32_t, const std::uint32_t, const std::uint32_t) const -> std::unique_ptr<bindless_heaps> {
	return std::make_unique<bindless_heaps>();
}

auto gse::dx12::device::create_blas(const gpu::acceleration_structure_geometry&, const std::uint32_t) -> blas {
	return {};
}

auto gse::dx12::device::create_tlas(const std::uint32_t) -> tlas {
	return {};
}

auto gse::dx12::device::query_blas_build_sizes(const gpu::acceleration_structure_geometry&, const std::uint32_t) const -> gpu::acceleration_structure_build_sizes {
	return {};
}

auto gse::dx12::device::acceleration_structure_scratch_alignment() const -> gpu::device_size {
	return 256;
}

auto gse::dx12::device::wait_for_fence(const gpu::fence_handle, const std::uint64_t) const -> gpu::result {
	return gpu::result::success;
}

auto gse::dx12::device::reset_fence(const gpu::fence_handle) const -> void {
}

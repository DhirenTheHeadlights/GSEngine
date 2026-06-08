export module gse.dx12:device;

import std;

import gse.gpu_backend;
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

		~device() = default;

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
		) -> gpu::buffer;

		[[nodiscard]] auto create_image(
			const gpu::image_desc& desc,
			std::string_view tag = ""
		) -> gpu::image;

		[[nodiscard]] auto create_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::handle<gpu::sampler>;

		[[nodiscard]] auto create_image_view(
			gpu::handle<gpu::image> img,
			const gpu::image_view_create_info& info
		) const -> gpu::handle<gpu::image_view>;

		[[nodiscard]] auto buffer_device_address(
			gpu::handle<gpu::buffer> buf
		) const -> gpu::device_address;

		auto destroy_buffer(
			gpu::handle<gpu::buffer> buf
		) const -> void;

		auto destroy_image(
			gpu::handle<gpu::image> img
		) const -> void;

		auto destroy_image_view(
			gpu::handle<gpu::image_view> view
		) const -> void;

		[[nodiscard]] auto create_buffer_unbound(
			const gpu::buffer_desc& info
		) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements>;

		[[nodiscard]] auto create_image_unbound(
			const gpu::image_create_info& info
		) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements>;

		auto bind_buffer_memory(
			gpu::handle<gpu::buffer> buf,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		auto bind_image_memory(
			gpu::handle<gpu::image> img,
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
			gpu::handle<gpu::image> img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		[[nodiscard]] auto create_sync(
			std::uint32_t image_count,
			std::uint32_t frames_in_flight = 2
		) const -> sync;

		[[nodiscard]] auto create_timeline_semaphore(
			std::uint64_t initial_value
		) const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto create_timestamp_query_pool(
			std::uint32_t capacity,
			std::string_view label = {}
		) const -> gpu::handle<gpu::query_pool>;

		[[nodiscard]] auto create_pipeline_stats_query_pool(
			std::uint32_t capacity,
			gpu::pipeline_statistic_flags statistics,
			std::string_view label = {}
		) const -> gpu::handle<gpu::query_pool>;

		[[nodiscard]] auto create_bindless_heaps(
			std::uint32_t texture_capacity = 2048,
			std::uint32_t image_capacity = 65536,
			std::uint32_t buffer_capacity = 16384,
			std::uint32_t sampler_capacity = 512
		) const -> std::unique_ptr<bindless_heaps>;

		[[nodiscard]] auto create_blas(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> gpu::blas;

		[[nodiscard]] auto create_tlas(
			std::uint32_t max_instances
		) -> gpu::tlas;

		[[nodiscard]] auto query_blas_build_sizes(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) const -> gpu::acceleration_structure_build_sizes;

		[[nodiscard]] auto acceleration_structure_scratch_alignment() const -> gpu::device_size;

		[[nodiscard]] auto wait_for_fence(
			gpu::handle<gpu::fence> f,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::result;

		auto reset_fence(
			gpu::handle<gpu::fence> f
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

auto gse::dx12::device::create_buffer(const gpu::buffer_desc&, const void*, const std::string_view, const std::source_location&) -> gpu::buffer {
	return {};
}

auto gse::dx12::device::create_image(const gpu::image_desc&, const std::string_view) -> gpu::image {
	return {};
}

auto gse::dx12::device::create_sampler(const gpu::sampler_desc&) -> gpu::handle<gpu::sampler> {
	return {};
}

auto gse::dx12::device::create_image_view(const gpu::handle<gpu::image>, const gpu::image_view_create_info&) const -> gpu::handle<gpu::image_view> {
	return {};
}

auto gse::dx12::device::buffer_device_address(const gpu::handle<gpu::buffer>) const -> gpu::device_address {
	return 0;
}

auto gse::dx12::device::destroy_buffer(const gpu::handle<gpu::buffer>) const -> void {
}

auto gse::dx12::device::destroy_image(const gpu::handle<gpu::image>) const -> void {
}

auto gse::dx12::device::destroy_image_view(const gpu::handle<gpu::image_view>) const -> void {
}

auto gse::dx12::device::create_buffer_unbound(const gpu::buffer_desc&) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements> {
	return {};
}

auto gse::dx12::device::create_image_unbound(const gpu::image_create_info&) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements> {
	return {};
}

auto gse::dx12::device::bind_buffer_memory(const gpu::handle<gpu::buffer>, const gpu::device_memory, const gpu::device_size) const -> void {
}

auto gse::dx12::device::bind_image_memory(const gpu::handle<gpu::image>, const gpu::device_memory, const gpu::device_size) const -> void {
}

auto gse::dx12::device::allocate_aliased_memory(const gpu::device_size, const std::uint32_t) const -> gpu::device_memory {
	return {};
}

auto gse::dx12::device::free_aliased_memory(const gpu::device_memory) const -> void {
}

auto gse::dx12::device::find_memory_type_index(const std::uint32_t, const gpu::memory_property_flags) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::host_upload_image_layers(const gpu::handle<gpu::image>, const std::span<const void* const>, const vec2u) const -> void {
}

auto gse::dx12::device::create_sync(const std::uint32_t, const std::uint32_t) const -> sync {
	return {};
}

auto gse::dx12::device::create_timeline_semaphore(const std::uint64_t) const -> gpu::handle<gpu::semaphore> {
	return {};
}

auto gse::dx12::device::create_timestamp_query_pool(const std::uint32_t, const std::string_view) const -> gpu::handle<gpu::query_pool> {
	return {};
}

auto gse::dx12::device::create_pipeline_stats_query_pool(const std::uint32_t, const gpu::pipeline_statistic_flags, const std::string_view) const -> gpu::handle<gpu::query_pool> {
	return {};
}

auto gse::dx12::device::create_bindless_heaps(const std::uint32_t, const std::uint32_t, const std::uint32_t, const std::uint32_t) const -> std::unique_ptr<bindless_heaps> {
	return std::make_unique<bindless_heaps>();
}

auto gse::dx12::device::create_blas(const gpu::acceleration_structure_geometry&, const std::uint32_t) -> gpu::blas {
	return {};
}

auto gse::dx12::device::create_tlas(const std::uint32_t) -> gpu::tlas {
	return {};
}

auto gse::dx12::device::query_blas_build_sizes(const gpu::acceleration_structure_geometry&, const std::uint32_t) const -> gpu::acceleration_structure_build_sizes {
	return {};
}

auto gse::dx12::device::acceleration_structure_scratch_alignment() const -> gpu::device_size {
	return 256;
}

auto gse::dx12::device::wait_for_fence(const gpu::handle<gpu::fence>, const std::uint64_t) const -> gpu::result {
	return gpu::result::success;
}

auto gse::dx12::device::reset_fence(const gpu::handle<gpu::fence>) const -> void {
}

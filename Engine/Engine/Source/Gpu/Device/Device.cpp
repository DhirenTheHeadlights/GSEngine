module gse.gpu:device_impl;

import std;

import :device;
import :sync_token;
import :video_backend;
import :video_encoder;

import gse.vulkan;

import gse.os;
import gse.log;
import gse.concurrency;
import gse.meta;

namespace gse::gpu {
	struct device_backend {
		vulkan::aftermath aftermath;
		vulkan::instance instance;
		vulkan::device device_config;
		vulkan::queue queue;
		vulkan::command command;
		vulkan::worker_command_pools worker_pools;
	};
}

auto gse::gpu::device::create(const shared_view<window::data> win, const bool validation_layers_enabled, gpu::device_settings& device_cfg) -> std::unique_ptr<device> {
	auto aftermath_tracker = vulkan::aftermath::create({});

	auto instance = vulkan::instance::create(vulkan::instance::required_window_extensions(), validation_layers_enabled);
	instance.create_surface(win);

	auto creation = vulkan::device::create(instance, device_cfg, aftermath_tracker);
	std::array<std::uint32_t, gpu::queue_type_count> queue_families{};
	queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)] = creation.families.graphics_family.value();
	queue_families[static_cast<std::size_t>(gpu::queue_type::compute)] = creation.families.compute_family.value();

	auto command = vulkan::command::create(creation.device, queue_families);

	auto worker_pools = vulkan::worker_command_pools::create(creation.device, queue_families, task::thread_count());

	const auto surface_format = vulkan::pick_surface_format(creation.device, instance);

	auto dev = std::unique_ptr<device>(new device(
		std::make_unique<device_backend>(
			std::move(aftermath_tracker),
			std::move(instance),
			std::move(creation.device),
			std::move(creation.queue),
			std::move(command),
			std::move(worker_pools)
		),
		surface_format,
		creation.video_encode_enabled
	));

	dev->m_backend->device_config.init_bindless();

	dev->m_transient = transient_executor<device>::create(
		*dev,
		dev->m_backend->device_config.queue_family(queue_type::graphics),
		dev->m_backend->device_config.queue_family(queue_type::compute),
		task::thread_count()
	);

	return dev;
}

gse::gpu::device::device(std::unique_ptr<device_backend> backend, image_format surface_format, bool video_encode_enabled)
	: m_backend(std::move(backend)), m_surface_format(surface_format), m_video_encode_enabled(video_encode_enabled) {
	constexpr std::size_t slot_count = pass_marker_ring_size * 4;
	constexpr std::size_t buffer_size = slot_count * sizeof(std::uint32_t);
	const std::array<std::uint32_t, slot_count> zeros{};

	for (std::size_t di = 0; di < pass_marker_domain_count; ++di) {
		auto& ring = m_pass_marker_rings[di];
		ring.checkpoint_buffer = m_backend->device_config.create_buffer(
			gpu::buffer_desc{
				.size = buffer_size,
				.usage = gpu::buffer_flag::transfer_dst,
				.data = zeros.data(),
			},
			std::format(
				"device.pass_checkpoint.{}",
				static_cast<pass_marker_domain>(di)
			)
		);
		ring.checkpoint_slots = std::bit_cast<const std::uint32_t*>(ring.checkpoint_buffer.host_read().data());
	}
}

gse::gpu::device::~device() {
	log::println(log::category::runtime, "Destroying Device");
}

auto gse::gpu::device::handle() const -> gpu::device_handle {
	return m_backend->device_config.device_handle();
}

auto gse::gpu::device::surface_format() const -> image_format {
	return m_surface_format;
}

auto gse::gpu::device::queue_family(const gpu::queue_type queue) const -> std::uint32_t {
	return m_backend->device_config.queue_family(queue);
}

auto gse::gpu::device::wait_idle() const -> void {
	m_backend->device_config.wait_idle();
}

auto gse::gpu::device::timestamp_period() const -> float {
	return m_backend->device_config.timestamp_period();
}

auto gse::gpu::device::begin_pass_marker(const gpu::command_buffer_handle cmd, const pass_marker_domain domain, const pass_marker marker) -> pass_marker_handle {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(domain)];
	const auto seq = ring.seq.fetch_add(1, std::memory_order_relaxed);
	ring.entries[seq % pass_marker_ring_size] = marker;

	if (ring.checkpoint_buffer.valid()) {
		const auto slot = seq % pass_marker_ring_size;
		const auto offset = slot * 4 * sizeof(std::uint32_t);
		vulkan::commands(cmd)
			.fill_buffer(
				ring.checkpoint_buffer.handle(),
				offset,
				sizeof(std::uint32_t),
				static_cast<std::uint32_t>(seq)
			);
	}

	return pass_marker_handle{
		.seq = seq,
		.domain = domain
	};
}

auto gse::gpu::device::checkpoint_pass_marker(const gpu::command_buffer_handle cmd, const pass_marker_handle handle) -> void {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + sizeof(std::uint32_t);
	vulkan::commands(cmd)
		.fill_buffer(
			ring.checkpoint_buffer.handle(),
			offset,
			sizeof(std::uint32_t),
			static_cast<std::uint32_t>(handle.seq)
		);
}

auto gse::gpu::device::post_renderpass_pass_marker(const gpu::command_buffer_handle cmd, const pass_marker_handle handle) -> void {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 2 * sizeof(std::uint32_t);
	vulkan::commands(cmd)
		.fill_buffer(
			ring.checkpoint_buffer.handle(),
			offset,
			sizeof(std::uint32_t),
			static_cast<std::uint32_t>(handle.seq)
		);
}

auto gse::gpu::device::end_pass_marker(const gpu::command_buffer_handle cmd, const pass_marker_handle handle) -> void {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 3 * sizeof(std::uint32_t);
	vulkan::commands(cmd)
		.fill_buffer(
			ring.checkpoint_buffer.handle(),
			offset,
			sizeof(std::uint32_t),
			static_cast<std::uint32_t>(handle.seq)
		);
}

auto gse::gpu::device::report_device_lost(const std::string_view operation) -> void {
	if (bool expected = false; !m_device_lost_reported.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
		return;
	}

	const auto aftermath_wait = make_scope_exit([this] {
		m_backend->aftermath.wait_for_crash_dump();
	});

	log::println(log::level::error, log::category::vulkan, "Vulkan device lost during {}", operation);

	for (std::size_t di = 0; di < pass_marker_domain_count; ++di) {
		auto& ring = m_pass_marker_rings[di];
		const auto domain = static_cast<pass_marker_domain>(di);
		const auto seq_next = ring.seq.load(std::memory_order_relaxed);
		const auto total = seq_next > 0 ? seq_next - 1 : 0;
		if (total == 0) {
			continue;
		}

		const auto count = std::min<std::uint64_t>(total, pass_marker_ring_size);
		const auto first_seq = seq_next - count;
		log::println(
			log::level::error,
			log::category::vulkan,
			"Last {} pass markers for {} (oldest first, status from GPU checkpoint):",
			count,
			domain
		);

		std::uint64_t last_gpu_inflight = 0;
		std::string_view last_gpu_inflight_phase = {};
		bool any_gpu_inflight = false;

		for (std::uint64_t i = 0; i < count; ++i) {
			const auto seq = first_seq + i;
			const auto slot = seq % pass_marker_ring_size;
			const auto& m = ring.entries[slot];

			std::string_view status = "queued";
			if (ring.checkpoint_slots) {
				const auto begin_seq = ring.checkpoint_slots[slot * 4];
				const auto post_barrier_seq = ring.checkpoint_slots[slot * 4 + 1];
				const auto post_renderpass_seq = ring.checkpoint_slots[slot * 4 + 2];
				const auto end_seq = ring.checkpoint_slots[slot * 4 + 3];
				const auto seq_low = static_cast<std::uint32_t>(seq);

				const bool begin_done = begin_seq == seq_low;
				const bool post_barrier_done = post_barrier_seq == seq_low;
				const bool post_renderpass_done = post_renderpass_seq == seq_low;
				const bool end_done = end_seq == seq_low;

				if (begin_done && post_barrier_done && post_renderpass_done && end_done) {
					status = "completed";
				}
				else if (begin_done && post_barrier_done && post_renderpass_done) {
					status = "HUNG in tail (renderpass done, end marker not written)";
					last_gpu_inflight = std::max(last_gpu_inflight, seq);
					last_gpu_inflight_phase = "tail";
					any_gpu_inflight = true;
				}
				else if (begin_done && post_barrier_done) {
					status = "HUNG in renderpass body (begin+barrier done, renderpass did not finish)";
					last_gpu_inflight = std::max(last_gpu_inflight, seq);
					last_gpu_inflight_phase = "renderpass body";
					any_gpu_inflight = true;
				}
				else if (begin_done) {
					status = "HUNG in inter-pass barrier (begin reached, no post-barrier)";
					last_gpu_inflight = std::max(last_gpu_inflight, seq);
					last_gpu_inflight_phase = "inter-pass barrier";
					any_gpu_inflight = true;
				}
				else {
					status = "not started (CPU recorded, GPU never entered)";
				}
			}

			log::println(
				log::level::error,
				log::category::vulkan,
				"  seq={} frame={} pass_index={} pass_type={} [{}]",
				seq,
				m.frame_counter,
				m.pass_index,
				m.pass_type.tag(),
				status
			);
		}

		if (any_gpu_inflight) {
			const auto& m = ring.entries[last_gpu_inflight % pass_marker_ring_size];
			log::println(
				log::level::error,
				log::category::vulkan,
				"{} GPU hung in {} of: seq={} frame={} pass_index={} pass_type={}",
				domain,
				last_gpu_inflight_phase,
				last_gpu_inflight,
				m.frame_counter,
				m.pass_index,
				m.pass_type.tag()
			);
		}
	}

	if (!m_backend->device_config.fault_enabled()) {
		log::println(log::level::warning, log::category::vulkan, "VK_EXT_device_fault is unavailable on this device");
		return;
	}

	device_fault_counts counts{};
	if (const auto result = m_backend->device_config.query_fault_counts(counts); result != gpu::result::success) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"Failed to query device fault counts: {}",
			static_cast<int>(result)
		);
		return;
	}

	if (!m_backend->device_config.vendor_binary_fault_enabled()) {
		counts.vendor_binary_size = 0;
	}

	device_fault_info fault_info{};
	if (const auto result = m_backend->device_config.query_fault_info(counts, fault_info); result != gpu::result::success) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"Failed to query device fault info: {}",
			static_cast<int>(result)
		);
		return;
	}

	log::println(
		log::level::error,
		log::category::vulkan,
		"Device fault description: {}",
		fault_info.description.empty() ? std::string_view("(no description)") : std::string_view(fault_info.description)
	);

	for (std::size_t i = 0; i < fault_info.address_infos.size(); ++i) {
		const auto& [address_type, reported_address, address_precision] = fault_info.address_infos[i];
		log::println(
			log::level::error,
			log::category::vulkan,
			"Fault address {}: type={}, reported=0x{:x}, precision=0x{:x}",
			i,
			address_type,
			reported_address,
			address_precision
		);
	}

	for (std::size_t i = 0; i < fault_info.vendor_infos.size(); ++i) {
		const auto& [description, vendor_fault_code, vendor_fault_data] = fault_info.vendor_infos[i];
		log::println(
			log::level::error,
			log::category::vulkan,
			"Vendor fault {}: '{}' code=0x{:x} data=0x{:x}",
			i,
			description.empty() ? std::string_view("(no description)") : std::string_view(description),
			vendor_fault_code,
			vendor_fault_data
		);
	}

	if (!fault_info.vendor_binary.empty()) {
		log::println(
			log::level::error,
			log::category::vulkan,
			"Device fault vendor binary size: {} bytes",
			fault_info.vendor_binary.size()
		);
	}
}

auto gse::gpu::device::frame_command_buffer(const queue_type queue, const std::uint32_t frame_index) const -> gpu::command_buffer_handle {
	return m_backend->command.frame_command_buffer(queue, frame_index);
}

auto gse::gpu::device::submit(const queue_type queue, const submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> void {
	m_backend->queue.submit(queue, info, signal_fence);
}

auto gse::gpu::device::present(const present_info& info) -> result {
	return m_backend->queue.present(info);
}

auto gse::gpu::device::wait_for_fence(const gpu::handle<gpu::fence> f, const std::uint64_t timeout_ns) const -> result {
	return m_backend->device_config.wait_for_fence(f, timeout_ns);
}

auto gse::gpu::device::reset_fence(const gpu::handle<gpu::fence> f) const -> void {
	m_backend->device_config.reset_fence(f);
}

auto gse::gpu::device::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	m_backend->worker_pools.reset_frame(frame_index);
}

auto gse::gpu::device::acquire_worker_command_buffer(const queue_type queue, const std::size_t worker_index, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	return m_backend->worker_pools.acquire_secondary(queue, worker_index, frame_index);
}

auto gse::gpu::device::make_video_encoder(const vec2u extent) -> std::optional<video_encoder> {
	if (!m_video_encode_enabled) {
		return std::nullopt;
	}
	const auto caps = vulkan::video_encoder::probe(m_backend->device_config, m_backend->queue);
	if (!caps.available) {
		return std::nullopt;
	}
	return video_encoder(std::make_unique<video_encoder_backend>(vulkan::video_encoder::create(m_backend->device_config, m_backend->queue, extent, caps)));
}

auto gse::gpu::device::create_image_unbound(const image_create_info& info) const -> std::pair<gpu::handle<gpu::image>, memory_requirements> {
	return m_backend->device_config.create_image_unbound(info);
}

auto gse::gpu::device::create_buffer_unbound(const buffer_desc& info) const -> std::pair<gpu::handle<gpu::buffer>, memory_requirements> {
	return m_backend->device_config.create_buffer_unbound(info);
}

auto gse::gpu::device::bind_image_memory(const gpu::handle<gpu::image> img, const device_memory mem, const device_size offset) const -> void {
	m_backend->device_config.bind_image_memory(img, mem, offset);
}

auto gse::gpu::device::bind_buffer_memory(const gpu::handle<gpu::buffer> buf, const device_memory mem, const device_size offset) const -> void {
	m_backend->device_config.bind_buffer_memory(buf, mem, offset);
}

auto gse::gpu::device::create_image_view(const gpu::handle<gpu::image> img, const image_view_create_info& info) const -> gpu::handle<gpu::image_view> {
	return m_backend->device_config.create_image_view(img, info);
}

auto gse::gpu::device::allocate_aliased_memory(const device_size size, const std::uint32_t memory_type_index) const -> device_memory {
	return m_backend->device_config.allocate_aliased_memory(size, memory_type_index);
}

auto gse::gpu::device::free_aliased_memory(const device_memory mem) const -> void {
	m_backend->device_config.free_aliased_memory(mem);
}

auto gse::gpu::device::find_memory_type_index(const std::uint32_t type_bits, const memory_property_flags required) const -> std::uint32_t {
	return m_backend->device_config.find_memory_type_index(type_bits, required);
}

auto gse::gpu::device::make_aliased_image(const gpu::handle<gpu::image> img_handle, const gpu::handle<gpu::image_view> view_handle, const image_format format, const vec3u extent, const image_view_create_info& view_info, const std::string_view tag) -> std::unique_ptr<image> {
	return std::make_unique<image>(
		img_handle,
		view_handle,
		static_cast<image_format_value>(format),
		extent,
		view_info
	);
}

auto gse::gpu::device::make_aliased_buffer(const gpu::handle<gpu::buffer> buf_handle, const device_size size, const std::string_view tag) -> std::unique_ptr<buffer> {
	return std::make_unique<buffer>(
		buf_handle,
		size,
		0,
		nullptr
	);
}

auto gse::gpu::device::transient() -> transient_executor<device>& {
	return *m_transient;
}

auto gse::gpu::device::begin_one_time_commands(const gpu::command_buffer_handle cmd) -> void {
	m_backend->device_config.begin_one_time_commands(cmd);
}

auto gse::gpu::device::end_commands(const gpu::command_buffer_handle cmd) -> void {
	m_backend->device_config.end_commands(cmd);
}

auto gse::gpu::device::create_transient_command_pool(const std::uint32_t family) -> gpu::transient_pool_handle {
	return m_backend->device_config.create_transient_command_pool(family);
}

auto gse::gpu::device::allocate_transient_primary(const gpu::transient_pool_handle pool) -> gpu::command_buffer_handle {
	return m_backend->device_config.allocate_transient_primary(pool);
}

auto gse::gpu::device::transient_pool_try_reset(const gpu::transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	m_backend->device_config.transient_pool_try_reset(pool, queue_progress);
}

auto gse::gpu::device::transient_pool_mark_in_use(const gpu::transient_pool_handle pool, const std::uint64_t value) -> void {
	m_backend->device_config.transient_pool_mark_in_use(pool, value);
}

auto gse::gpu::device::transient_pool_reset_all(const gpu::transient_pool_handle pool) -> void {
	m_backend->device_config.transient_pool_reset_all(pool);
}

auto gse::gpu::device::video_encode_enabled() const -> bool {
	return m_video_encode_enabled;
}

auto gse::gpu::device::create_shader_program(const shader_program_create_info& info) -> shader_program {
	return m_backend->device_config.create_shader_program(info);
}



auto gse::gpu::device::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	return m_backend->device_config.create_timeline_semaphore(initial_value);
}

auto gse::gpu::device::create_semaphore() -> gpu::handle<gpu::semaphore> {
	return m_backend->device_config.create_semaphore();
}

auto gse::gpu::device::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	return m_backend->device_config.create_fence(signaled);
}

auto gse::gpu::device::retire(const gpu::handle<gpu::semaphore> semaphore) -> void {
	m_backend->device_config.retire(semaphore);
}

auto gse::gpu::device::retire(const gpu::handle<gpu::fence> fence) -> void {
	m_backend->device_config.retire(fence);
}

auto gse::gpu::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	return m_backend->device_config.semaphore_counter_value(semaphore);
}

auto gse::gpu::device::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	m_backend->device_config.wait_semaphore(semaphore, value);
}

auto gse::gpu::device::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return m_backend->device_config.create_timestamp_query_pool(capacity, label);
}

auto gse::gpu::device::create_pipeline_stats_query_pool(const std::uint32_t capacity, const pipeline_statistic_flags statistics, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return m_backend->device_config.create_pipeline_stats_query_pool(capacity, statistics, label);
}

auto gse::gpu::device::query_pool_results(const gpu::handle<gpu::query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<query_status, std::vector<std::uint64_t>> {
	return m_backend->device_config.query_pool_results(pool, first_query, query_count, stride);
}

auto gse::gpu::device::create_swapchain(const vec2i framebuffer_size, const present_mode mode, const gpu::swap_chain_handle old_handle) -> swap_chain_info {
	return m_backend->device_config.create_swap_chain(framebuffer_size, mode, old_handle);
}

auto gse::gpu::device::acquire_swapchain_image(const gpu::swap_chain_handle swapchain, const gpu::handle<gpu::semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	return m_backend->device_config.acquire_next_image(swapchain, wait_semaphore, timeout_ns);
}

auto gse::gpu::device::wait_swapchain_release_fences(const gpu::swap_chain_handle swapchain) const -> void {
	m_backend->device_config.wait_swapchain_release_fences(swapchain);
}

auto gse::gpu::device::reset_swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> void {
	m_backend->device_config.reset_swapchain_release_fence(swapchain, image_index);
}

auto gse::gpu::device::swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> gpu::handle<gpu::fence> {
	return m_backend->device_config.swapchain_release_fence(swapchain, image_index);
}

auto gse::gpu::device::swapchain_past_presentation_timing(const gpu::swap_chain_handle swapchain) const -> std::vector<past_present_timing> {
	return m_backend->device_config.swapchain_past_presentation_timing(swapchain);
}

auto gse::gpu::device::create_blas(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> blas {
	return m_backend->device_config.create_blas(geometry, prim_count);
}

auto gse::gpu::device::create_tlas(const std::uint32_t max_instances) -> tlas {
	return m_backend->device_config.create_tlas(max_instances);
}

auto gse::gpu::device::query_blas_build_sizes(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> acceleration_structure_build_sizes {
	return m_backend->device_config.query_blas_build_sizes(geometry, prim_count);
}

auto gse::gpu::device::acceleration_structure_scratch_alignment() const -> device_size {
	return m_backend->device_config.acceleration_structure_scratch_alignment();
}

auto gse::gpu::device::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	m_backend->device_config.host_upload_image_layers(img, layer_pointers, extent);
}

auto gse::gpu::device::create_buffer(const buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> buffer {
	return m_backend->device_config.create_buffer(desc, tag, loc);
}

auto gse::gpu::device::create_image(const image_desc& desc, const std::string_view tag) -> image {
	return m_backend->device_config.create_image(desc, tag);
}

auto gse::gpu::device::allocate_buffer_slot() -> gpu::bindless_handle {
	return m_backend->device_config.allocate_buffer_slot();
}

auto gse::gpu::device::allocate_image_slot() -> gpu::bindless_handle {
	return m_backend->device_config.allocate_image_slot();
}

auto gse::gpu::device::write_storage_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	m_backend->device_config.write_storage_buffer(slot, address, size);
}

auto gse::gpu::device::write_uniform_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	m_backend->device_config.write_uniform_buffer(slot, address, size);
}

auto gse::gpu::device::write_acceleration_structure(const gpu::bindless_slot slot, const gpu::device_address as_address) -> void {
	m_backend->device_config.write_acceleration_structure(slot, as_address);
}

auto gse::gpu::device::write_sampled_image(const gpu::bindless_slot slot, const image& img) -> void {
	m_backend->device_config.write_sampled_image(slot, img);
}

auto gse::gpu::device::register_sampler(const sampler_desc& desc) -> gpu::bindless_handle {
	return m_backend->device_config.register_sampler(desc);
}

auto gse::gpu::device::register_texture(const image& img, const sampler_desc& desc) -> gpu::bindless_handle {
	return m_backend->device_config.register_texture(img, desc);
}

auto gse::gpu::device::bindless_layout() const -> gpu::bindless_layout {
	return m_backend->device_config.bindless_layout();
}

auto gse::gpu::device::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return m_backend->device_config.bindless_resource_heap_binding();
}

auto gse::gpu::device::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return m_backend->device_config.bindless_sampler_heap_binding();
}

auto gse::gpu::device::create_sampler(const sampler_desc& desc) -> gpu::handle<gpu::sampler> {
	return m_backend->device_config.create_sampler(desc);
}

auto gse::gpu::device::collect_garbage() -> void {
	m_backend->device_config.collect_garbage();
}

auto gse::gpu::device::upload_image_2d(image& img, const void* pixel_data) -> sync_token {
	const auto extent3 = img.extent();
	const vec2u extent2{ extent3.x(), extent3.y() };
	const void* ptrs[] = { pixel_data };
	host_upload_image_layers(img.handle(), ptrs, extent2);
	return {};
}

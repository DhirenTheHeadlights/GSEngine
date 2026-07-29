module gse.gpu:device_impl;

import std;

import :device;
import :video_backend;
import :video_encoder;
import :command_dispatch;
import :device_vulkan_backend;
import :device_dx12_backend;
import :backend_state;
import :image;

import gse.gpu_backend;

import gse.os;
import gse.ecs;
import gse.log;
import gse.concurrency;
import gse.meta;
import gse.math;
import gse.core;
import gse.assert;

namespace gse::gpu {
	template <typename B>
	auto device_backend_delete(void* self) -> void {
		delete static_cast<B*>(self);
	}

	consteval {
		std::meta::define_aggregate(^^gpu_dispatch, meta::dispatch_specs(meta::pointer_receiver<vulkan_device_backend>::self_type(), ^^vulkan_device_backend, meta::dispatch_no_exclude));
	}

	template <typename B>
	constexpr gpu_dispatch device_dispatch_for = meta::build_dispatch<gpu_dispatch, vulkan_device_backend, meta::pointer_receiver<B>>();
}

auto gse::gpu::device::create(const std::optional<shared_view<window::data>> win, const bool validation_layers_enabled, backend_kind& backend, gpu::device_settings& device_cfg) -> std::unique_ptr<device> {
	if (backend == backend_kind::vulkan) {
		auto created = create_vulkan_device_backend(win, validation_layers_enabled, device_cfg);
		if (created) {
			active_backend = backend_kind::vulkan;

			std::unique_ptr<void, void (*)(void*)> backend(
				created->backend.release(),
				&device_backend_delete<vulkan_device_backend>
			);

			auto dev = std::unique_ptr<device>(new device(
				std::move(backend),
				&device_dispatch_for<vulkan_device_backend>,
				created->commands,
				created->surface_format,
				created->video_encode_enabled
			));

			dev->m_transient = transient_executor<device>::create(
				*dev,
				dev->queue_family(queue_type::graphics),
				dev->queue_family(queue_type::compute),
				task::thread_count()
			);

			return dev;
		}

		log::println(log::category::render, "vulkan device unavailable; falling back to dx12 backend");
		log::flush();
		backend = backend_kind::dx12;
	}

	active_backend = backend_kind::dx12;
	auto created = create_dx12_device_backend(win, validation_layers_enabled, device_cfg);

	std::unique_ptr<void, void (*)(void*)> backend_ptr(
		created.backend.release(),
		&device_backend_delete<dx12_device_backend>
	);

	auto dev = std::unique_ptr<device>(new device(
		std::move(backend_ptr),
		&device_dispatch_for<dx12_device_backend>,
		created.commands,
		created.surface_format,
		created.video_encode_enabled
	));

	dev->m_transient = transient_executor<device>::create(
		*dev,
		dev->queue_family(queue_type::graphics),
		dev->queue_family(queue_type::compute),
		task::thread_count()
	);

	return dev;
}

gse::gpu::device::device(std::unique_ptr<void, void (*)(void*)> backend, const gpu_dispatch* dispatch, const command_dispatch* commands, image_format surface_format, bool video_encode_enabled)
	: m_backend(std::move(backend)), m_vt(dispatch), m_command_dispatch(commands), m_surface_format(surface_format), m_video_encode_enabled(video_encode_enabled) {
	constexpr std::size_t slot_count = pass_marker_ring_size * 4;
	constexpr std::size_t buffer_size = slot_count * sizeof(std::uint32_t);
	const std::array<std::uint32_t, slot_count> zeros{};

	for (std::size_t di = 0; di < pass_marker_domain_count; ++di) {
		auto& ring = m_pass_marker_rings[di];
		ring.checkpoint_buffer = m_vt->create_buffer(
			m_backend.get(),
			gpu::buffer_desc{
				.size = buffer_size,
				.usage = gpu::buffer_flag::transfer_dst,
				.data = zeros.data(),
			},
			std::format(
				"device.pass_checkpoint.{}",
				static_cast<pass_marker_domain>(di)
			),
			std::source_location::current()
		);
		ring.checkpoint_slots = std::bit_cast<const std::uint32_t*>(ring.checkpoint_buffer.host_read().data());
	}
}

gse::gpu::device::~device() {
	log::println(log::category::runtime, "Destroying Device");
}

auto gse::gpu::device::handle() const -> gpu::device_handle {
	return m_vt->handle(m_backend.get());
}

auto gse::gpu::device::surface_format() const -> image_format {
	return m_surface_format;
}

auto gse::gpu::device::queue_family(const gpu::queue_type queue) const -> std::uint32_t {
	return m_vt->queue_family(m_backend.get(), queue);
}

auto gse::gpu::device::wait_idle() const -> void {
	m_vt->wait_idle(m_backend.get());
}

auto gse::gpu::device::timestamp_period() const -> float {
	return m_vt->timestamp_period(m_backend.get());
}

auto gse::gpu::device::begin_pass_marker(const gpu::command_buffer_handle cmd, const pass_marker_domain domain, const pass_marker marker, const std::string_view name) -> pass_marker_handle {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(domain)];
	const auto seq = ring.seq.fetch_add(1, std::memory_order_relaxed);
	ring.entries[seq % pass_marker_ring_size] = marker;

	if (ring.checkpoint_buffer.valid()) {
		const auto slot = seq % pass_marker_ring_size;
		const auto offset = slot * 4 * sizeof(std::uint32_t);
		m_vt->record_buffer_fill_u32(
			m_backend.get(),
			cmd,
			ring.checkpoint_buffer.handle(),
			offset,
			static_cast<std::uint32_t>(seq)
		);
	}

	begin_debug_event(cmd, name);

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
	m_vt->record_buffer_fill_u32(
		m_backend.get(),
		cmd,
		ring.checkpoint_buffer.handle(),
		offset,
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
	m_vt->record_buffer_fill_u32(
		m_backend.get(),
		cmd,
		ring.checkpoint_buffer.handle(),
		offset,
		static_cast<std::uint32_t>(handle.seq)
	);
}

auto gse::gpu::device::end_pass_marker(const gpu::command_buffer_handle cmd, const pass_marker_handle handle) -> void {
	end_debug_event(cmd);

	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 3 * sizeof(std::uint32_t);
	m_vt->record_buffer_fill_u32(
		m_backend.get(),
		cmd,
		ring.checkpoint_buffer.handle(),
		offset,
		static_cast<std::uint32_t>(handle.seq)
	);
}

auto gse::gpu::device::report_device_lost(const std::string_view operation) -> void {
	if (bool expected = false; !m_device_lost_reported.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
		return;
	}

	const auto aftermath_wait = make_scope_exit([this] {
		m_vt->wait_for_crash_dump(m_backend.get());
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
			"Last {} pass markers for {} (record order, NOT GPU execution order; status from GPU checkpoint):",
			count,
			domain
		);

		std::vector<std::uint64_t> in_flight;

		for (std::uint64_t i = 0; i < count; ++i) {
			const auto seq = first_seq + i;
			const auto slot = seq % pass_marker_ring_size;
			const auto& m = ring.entries[slot];

			std::string_view status = "queued";
			if (ring.checkpoint_slots) {
				const auto begin_seq = ring.checkpoint_slots[slot * 4];
				const auto post_renderpass_seq = ring.checkpoint_slots[slot * 4 + 2];
				const auto end_seq = ring.checkpoint_slots[slot * 4 + 3];
				const auto seq_low = static_cast<std::uint32_t>(seq);

				const bool begin_done = begin_seq == seq_low;
				const bool post_renderpass_done = post_renderpass_seq == seq_low;
				const bool end_done = end_seq == seq_low;

				if (end_done) {
					status = "completed";
				}
				else if (begin_done && post_renderpass_done) {
					status = "IN FLIGHT: body done, end marker not written";
					in_flight.push_back(seq);
				}
				else if (begin_done) {
					status = "IN FLIGHT: begin reached, body did not finish";
					in_flight.push_back(seq);
				}
				else if (post_renderpass_done) {
					status = "IN FLIGHT: body marker set without begin (markers unordered)";
					in_flight.push_back(seq);
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

		if (!in_flight.empty()) {
			log::println(
				log::level::error,
				log::category::vulkan,
				"{} had {} pass(es) in flight at device loss; the hang is one of these (record seq is NOT execution order, so any is a candidate):",
				domain,
				in_flight.size()
			);
			for (const auto seq : in_flight) {
				const auto& m = ring.entries[seq % pass_marker_ring_size];
				log::println(
					log::level::error,
					log::category::vulkan,
					"  -> seq={} frame={} pass_index={} pass_type={}",
					seq,
					m.frame_counter,
					m.pass_index,
					m.pass_type.tag()
				);
			}
		}
	}

	if (!m_vt->fault_enabled(m_backend.get())) {
		log::println(log::level::warning, log::category::vulkan, "VK_EXT_device_fault is unavailable on this device");
		return;
	}

	device_fault_counts counts{};
	if (const auto result = m_vt->query_fault_counts(m_backend.get(), counts); result != gpu::result::success) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"Failed to query device fault counts: {}",
			static_cast<int>(result)
		);
		return;
	}

	if (!m_vt->vendor_binary_fault_enabled(m_backend.get())) {
		counts.vendor_binary_size = 0;
	}

	device_fault_info fault_info{};
	if (const auto result = m_vt->query_fault_info(m_backend.get(), counts, fault_info); result != gpu::result::success) {
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
		const auto& address = fault_info.address_infos[i];
		log::println(
			log::level::error,
			log::category::vulkan,
			"Fault address {}: type={}({}), reported=0x{:x}, precision=0x{:x}",
			i,
			address.address_type,
			address.address_type_name.empty() ? std::string_view("unknown") : std::string_view(address.address_type_name),
			address.reported_address,
			address.address_precision
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
	return m_vt->frame_command_buffer(m_backend.get(), queue, frame_index);
}

auto gse::gpu::device::submit(const queue_type queue, const submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> void {
	m_vt->submit(m_backend.get(), queue, info, signal_fence);
}

auto gse::gpu::device::present(const present_info& info) -> result {
	return m_vt->present(m_backend.get(), info);
}

auto gse::gpu::device::wait_for_fence(const gpu::handle<gpu::fence> f, const std::uint64_t timeout_ns) const -> result {
	return m_vt->wait_for_fence(m_backend.get(), f, timeout_ns);
}

auto gse::gpu::device::reset_fence(const gpu::handle<gpu::fence> f) const -> void {
	m_vt->reset_fence(m_backend.get(), f);
}

auto gse::gpu::device::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	m_vt->reset_worker_command_pools(m_backend.get(), frame_index);
}

auto gse::gpu::device::acquire_worker_command_buffer(const queue_type queue, const std::size_t worker_index, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	return m_vt->acquire_worker_command_buffer(m_backend.get(), queue, worker_index, frame_index);
}

auto gse::gpu::device::make_video_encoder(const vec2u extent) -> std::optional<video_encoder> {
	if (!m_video_encode_enabled) {
		return std::nullopt;
	}
	auto backend = m_vt->make_video_encoder_backend(m_backend.get(), extent);
	if (!backend) {
		return std::nullopt;
	}
	return video_encoder(std::move(backend));
}

auto gse::gpu::device::create_image_unbound(const image_create_info& info) const -> std::pair<gpu::handle<gpu::image>, memory_requirements> {
	return m_vt->create_image_unbound(m_backend.get(), info);
}

auto gse::gpu::device::create_buffer_unbound(const buffer_desc& info) const -> std::pair<gpu::handle<gpu::buffer>, memory_requirements> {
	return m_vt->create_buffer_unbound(m_backend.get(), info);
}

auto gse::gpu::device::create_shared_surface(const shared_surface_desc& desc) const -> std::expected<shared_surface, std::string> {
	return m_vt->create_shared_surface(m_backend.get(), desc);
}

auto gse::gpu::device::import_shared_surface(const shared_surface_desc& desc, void* handle) const -> std::expected<shared_surface, std::string> {
	return m_vt->import_shared_surface(m_backend.get(), desc, handle);
}

auto gse::gpu::device::destroy_shared_surface(const shared_surface& surface) const -> void {
	m_vt->destroy_shared_surface(m_backend.get(), surface);
}

auto gse::gpu::device::bind_image_memory(const gpu::handle<gpu::image> img, const device_memory mem, const device_size offset) const -> void {
	m_vt->bind_image_memory(m_backend.get(), img, mem, offset);
}

auto gse::gpu::device::bind_buffer_memory(const gpu::handle<gpu::buffer> buf, const device_memory mem, const device_size offset) const -> void {
	m_vt->bind_buffer_memory(m_backend.get(), buf, mem, offset);
}

auto gse::gpu::device::buffer_slot(const gpu::handle<gpu::buffer> buffer) const -> gpu::bindless_slot {
	return m_vt->buffer_slot(m_backend.get(), buffer);
}

auto gse::gpu::device::buffer_address(const gpu::handle<gpu::buffer> buffer) const -> gpu::device_address {
	return m_vt->buffer_address(m_backend.get(), buffer);
}

auto gse::gpu::device::buffer_size(const gpu::handle<gpu::buffer> buffer) const -> gpu::device_size {
	return m_vt->buffer_size(m_backend.get(), buffer);
}

auto gse::gpu::device::buffer_mapped(const gpu::handle<gpu::buffer> buffer) const -> std::byte* {
	return m_vt->buffer_mapped(m_backend.get(), buffer);
}

auto gse::gpu::device::image_sampled_slot(const gpu::handle<gpu::image> image) const -> gpu::bindless_slot {
	return m_vt->image_sampled_slot(m_backend.get(), image);
}

auto gse::gpu::device::image_storage_slot(const gpu::handle<gpu::image> image) const -> gpu::bindless_slot {
	return m_vt->image_storage_slot(m_backend.get(), image);
}

auto gse::gpu::device::image_format_of(const gpu::handle<gpu::image> image) const -> gpu::image_format_value {
	return m_vt->image_format_of(m_backend.get(), image);
}

auto gse::gpu::device::image_extent(const gpu::handle<gpu::image> image) const -> vec3u {
	return m_vt->image_extent(m_backend.get(), image);
}

auto gse::gpu::device::image_view_of(const gpu::handle<gpu::image> image) const -> gpu::handle<gpu::image_view> {
	return m_vt->image_view_of(m_backend.get(), image);
}

auto gse::gpu::device::create_image_view(const gpu::handle<gpu::image> img, const image_view_create_info& info) const -> gpu::handle<gpu::image_view> {
	return m_vt->create_image_view(m_backend.get(), img, info);
}

auto gse::gpu::device::allocate_aliased_memory(const device_size size, const std::uint32_t memory_type_index) const -> device_memory {
	return m_vt->allocate_aliased_memory(m_backend.get(), size, memory_type_index);
}

auto gse::gpu::device::free_aliased_memory(const device_memory mem) const -> void {
	m_vt->free_aliased_memory(m_backend.get(), mem);
}

auto gse::gpu::device::find_memory_type_index(const std::uint32_t type_bits, const memory_property_flags required) const -> std::uint32_t {
	return m_vt->find_memory_type_index(m_backend.get(), type_bits, required);
}

auto gse::gpu::device::make_aliased_image(const gpu::handle<gpu::image> img_handle, const gpu::handle<gpu::image_view> view_handle, const image_format format, const vec3u extent, const image_view_create_info& view_info) -> std::unique_ptr<image> {
	return std::make_unique<image>(
		img_handle,
		view_handle,
		static_cast<image_format_value>(format),
		extent,
		view_info
	);
}

auto gse::gpu::device::make_aliased_buffer(const gpu::handle<gpu::buffer> buf_handle, const device_size size) -> std::unique_ptr<buffer> {
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

auto gse::gpu::device::recorder(const gpu::command_buffer_handle cmd) const -> pass_recorder {
	return pass_recorder(cmd, m_command_dispatch);
}

auto gse::gpu::device::begin_one_time_commands(const gpu::command_buffer_handle cmd) -> void {
	m_vt->begin_one_time_commands(m_backend.get(), cmd);
}

auto gse::gpu::device::end_commands(const gpu::command_buffer_handle cmd) -> void {
	m_vt->end_commands(m_backend.get(), cmd);
}

auto gse::gpu::device::cmd_reset(const gpu::command_buffer_handle cmd) -> void {
	m_vt->cmd_reset(m_backend.get(), cmd);
}

auto gse::gpu::device::cmd_begin(const gpu::command_buffer_handle cmd) -> void {
	m_vt->cmd_begin(m_backend.get(), cmd);
}

auto gse::gpu::device::cmd_end(const gpu::command_buffer_handle cmd) -> void {
	m_vt->cmd_end(m_backend.get(), cmd);
}

auto gse::gpu::device::cmd_pipeline_barrier(const gpu::command_buffer_handle cmd, const dependency_info& dep) -> void {
	m_vt->cmd_pipeline_barrier(m_backend.get(), cmd, dep);
}

auto gse::gpu::device::cmd_release_swapchain_to_present(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::image> img, const pipeline_stage_flags src_stages, const access_flags src_access) -> void {
	m_vt->cmd_release_swapchain_to_present(m_backend.get(), cmd, img, src_stages, src_access);
}

auto gse::gpu::device::begin_debug_event(const gpu::command_buffer_handle cmd, const std::string_view label) -> void {
	m_vt->begin_debug_event(m_backend.get(), cmd, label);
}

auto gse::gpu::device::end_debug_event(const gpu::command_buffer_handle cmd) -> void {
	m_vt->end_debug_event(m_backend.get(), cmd);
}

auto gse::gpu::device::create_transient_command_pool(const std::uint32_t family) -> gpu::transient_pool_handle {
	return m_vt->create_transient_command_pool(m_backend.get(), family);
}

auto gse::gpu::device::allocate_transient_primary(const gpu::transient_pool_handle pool) -> gpu::command_buffer_handle {
	return m_vt->allocate_transient_primary(m_backend.get(), pool);
}

auto gse::gpu::device::transient_pool_try_reset(const gpu::transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	m_vt->transient_pool_try_reset(m_backend.get(), pool, queue_progress);
}

auto gse::gpu::device::transient_pool_mark_in_use(const gpu::transient_pool_handle pool, const std::uint64_t value) -> void {
	m_vt->transient_pool_mark_in_use(m_backend.get(), pool, value);
}

auto gse::gpu::device::transient_pool_reset_all(const gpu::transient_pool_handle pool) -> void {
	m_vt->transient_pool_reset_all(m_backend.get(), pool);
}

auto gse::gpu::device::video_encode_enabled() const -> bool {
	return m_video_encode_enabled;
}

auto gse::gpu::device::create_shader_program(const shader_program_create_info& info) -> shader_program {
	return m_vt->create_shader_program(m_backend.get(), info);
}

auto gse::gpu::device::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	return m_vt->create_timeline_semaphore(m_backend.get(), initial_value);
}

auto gse::gpu::device::create_exportable_semaphore() -> gpu::handle<gpu::semaphore> {
	return m_vt->create_exportable_semaphore(m_backend.get());
}

auto gse::gpu::device::export_semaphore_handle(const gpu::handle<gpu::semaphore> semaphore) const -> std::expected<void*, std::string> {
	return m_vt->export_semaphore_handle(m_backend.get(), semaphore);
}

auto gse::gpu::device::import_semaphore_handle(void* handle) -> std::expected<gpu::handle<gpu::semaphore>, std::string> {
	return m_vt->import_semaphore_handle(m_backend.get(), handle);
}

auto gse::gpu::device::create_semaphore() -> gpu::handle<gpu::semaphore> {
	return m_vt->create_semaphore(m_backend.get());
}

auto gse::gpu::device::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	return m_vt->create_fence(m_backend.get(), signaled);
}

auto gse::gpu::device::retire(const gpu::handle<gpu::semaphore> semaphore) -> void {
	m_vt->retire_semaphore(m_backend.get(), semaphore);
}

auto gse::gpu::device::retire(const gpu::handle<gpu::fence> fence) -> void {
	m_vt->retire_fence(m_backend.get(), fence);
}

auto gse::gpu::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	return m_vt->semaphore_counter_value(m_backend.get(), semaphore);
}

auto gse::gpu::device::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	m_vt->wait_semaphore(m_backend.get(), semaphore, value);
}

auto gse::gpu::device::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return m_vt->create_timestamp_query_pool(m_backend.get(), capacity, label);
}

auto gse::gpu::device::create_pipeline_stats_query_pool(const std::uint32_t capacity, const pipeline_statistic_flags statistics, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return m_vt->create_pipeline_stats_query_pool(m_backend.get(), capacity, statistics, label);
}

auto gse::gpu::device::query_pool_results(const gpu::handle<gpu::query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<query_status, std::vector<std::uint64_t>> {
	return m_vt->query_pool_results(m_backend.get(), pool, first_query, query_count, stride);
}

auto gse::gpu::device::create_swapchain(const vec2i framebuffer_size, const present_mode mode, const gpu::swap_chain_handle old_handle) -> swap_chain_info {
	return m_vt->create_swapchain(m_backend.get(), framebuffer_size, mode, old_handle);
}

auto gse::gpu::device::acquire_swapchain_image(const gpu::swap_chain_handle swapchain, const gpu::handle<gpu::semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	return m_vt->acquire_swapchain_image(m_backend.get(), swapchain, wait_semaphore, timeout_ns);
}

auto gse::gpu::device::wait_swapchain_release_fences(const gpu::swap_chain_handle swapchain) const -> void {
	m_vt->wait_swapchain_release_fences(m_backend.get(), swapchain);
}

auto gse::gpu::device::reset_swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> void {
	m_vt->reset_swapchain_release_fence(m_backend.get(), swapchain, image_index);
}

auto gse::gpu::device::swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> gpu::handle<gpu::fence> {
	return m_vt->swapchain_release_fence(m_backend.get(), swapchain, image_index);
}

auto gse::gpu::device::swapchain_past_presentation_timing(const gpu::swap_chain_handle swapchain) const -> std::vector<past_present_timing> {
	return m_vt->swapchain_past_presentation_timing(m_backend.get(), swapchain);
}

auto gse::gpu::device::create_blas(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> blas {
	return m_vt->create_blas(m_backend.get(), geometry, prim_count);
}

auto gse::gpu::device::create_tlas(const std::uint32_t max_instances) -> tlas {
	return m_vt->create_tlas(m_backend.get(), max_instances);
}

auto gse::gpu::device::query_blas_build_sizes(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> acceleration_structure_build_sizes {
	return m_vt->query_blas_build_sizes(m_backend.get(), geometry, prim_count);
}

auto gse::gpu::device::acceleration_structure_scratch_alignment() const -> device_size {
	return m_vt->acceleration_structure_scratch_alignment(m_backend.get());
}

auto gse::gpu::device::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	m_vt->host_upload_image_layers(m_backend.get(), img, layer_pointers, extent);
}

auto gse::gpu::device::create_buffer(const buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> buffer {
	assert(
		!(desc.bindless && desc.usage.test(buffer_flag::uniform)),
		"bindless buffers must be usage=storage, not uniform: every shader binding reads the descriptor heap as a StructuredBuffer, " 
		"so a uniform-usage bindless buffer writes a descriptor no shader can read (garbage matrices -> NaN -> GPU hang). Drop buffer_flag::uniform."
	);
	auto buf = m_vt->create_buffer(m_backend.get(), desc, tag, loc);
	if (desc.bindless) {
		set_slot_resource(buf.slot().index, resource_ref{
			.ptr = std::bit_cast<const void*>(buf.handle()),
			.type = resource_type::buffer,
			.buffer_size = buf.size(),
		});
	}
	return buf;
}

auto gse::gpu::device::create_image(const image_desc& desc, const std::string_view tag) -> image {
	auto img = m_vt->create_image(m_backend.get(), desc, tag);
	if (desc.bindless) {
		const resource_ref ref{
			.ptr = std::bit_cast<const void*>(img.handle()),
			.type = resource_type::image,
			.aspects = image_aspect_for(img.format()),
		};
		set_slot_resource(img.storage_slot().index, ref);
		set_slot_resource(img.sampled_slot().index, ref);
	}
	return img;
}

auto gse::gpu::device::set_slot_resource(const std::uint32_t slot_index, const resource_ref& ref) -> void {
	if (slot_index == bindless_slot::invalid_index) {
		return;
	}
	if (slot_index >= m_slot_resources.size()) {
		m_slot_resources.resize(slot_index + 1);
	}
	m_slot_resources[slot_index] = ref;
}

auto gse::gpu::device::resource_for_slot(const std::uint32_t slot_index) const -> resource_ref {
	return slot_index < m_slot_resources.size() ? m_slot_resources[slot_index] : resource_ref{};
}

auto gse::gpu::device::allocate_buffer_slot() -> gpu::bindless_handle {
	return m_vt->allocate_buffer_slot(m_backend.get());
}

auto gse::gpu::device::allocate_image_slot() -> gpu::bindless_handle {
	return m_vt->allocate_image_slot(m_backend.get());
}

auto gse::gpu::device::allocate_acceleration_structure_slot() -> gpu::bindless_handle {
	return m_vt->allocate_acceleration_structure_slot(m_backend.get());
}

auto gse::gpu::device::write_storage_buffer(const gpu::bindless_slot slot, const buffer& buf, const gpu::device_size size) -> void {
	set_slot_resource(slot.index, resource_ref{
		.ptr = std::bit_cast<const void*>(buf.handle()),
		.type = resource_type::buffer,
		.buffer_size = size,
	});
	m_vt->write_storage_buffer(m_backend.get(), slot, buf.device_address(), size);
}

auto gse::gpu::device::write_acceleration_structure(const gpu::bindless_slot slot, const gpu::device_address as_address) -> void {
	m_vt->write_acceleration_structure(m_backend.get(), slot, as_address);
}

auto gse::gpu::device::write_sampled_image(const gpu::bindless_slot slot, const image& img) -> void {
	set_slot_resource(slot.index, resource_ref{
		.ptr = std::bit_cast<const void*>(img.handle()),
		.type = resource_type::image,
		.aspects = image_aspect_for(img.format()),
	});
	m_vt->write_sampled_image(m_backend.get(), slot, img);
}

auto gse::gpu::device::register_sampler(const sampler_desc& desc) -> gpu::bindless_handle {
	return m_vt->register_sampler(m_backend.get(), desc);
}

auto gse::gpu::device::register_texture(const image& img, const sampler_desc& desc) -> gpu::bindless_handle {
	return m_vt->register_texture(m_backend.get(), img, desc);
}

auto gse::gpu::device::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return m_vt->bindless_resource_heap_binding(m_backend.get());
}

auto gse::gpu::device::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return m_vt->bindless_sampler_heap_binding(m_backend.get());
}

auto gse::gpu::device::create_sampler(const sampler_desc& desc) -> gpu::handle<gpu::sampler> {
	return m_vt->create_sampler(m_backend.get(), desc);
}

auto gse::gpu::device::collect_garbage() -> void {
	m_vt->collect_garbage(m_backend.get());
}

auto gse::gpu::device::upload_image_2d(image& img, const void* pixel_data) -> void {
	const auto extent3 = img.extent();
	const vec2u extent2{ extent3.x(), extent3.y() };
	const void* ptrs[] = { pixel_data };
	host_upload_image_layers(img.handle(), ptrs, extent2);
}

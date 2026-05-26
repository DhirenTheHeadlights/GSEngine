module gse.gpu;

import std;

import :aliases;
import :device;

import gse.vulkan;

import gse.os;
import gse.log;
import gse.concurrency;
import gse.meta;

auto gse::gpu::device::create(const window::data& win, const bool validation_layers_enabled, gpu::device_settings& device_cfg) -> std::unique_ptr<device> {
	auto aftermath_tracker = vulkan::aftermath::create({});

	auto instance = vulkan::instance::create(
		window::vulkan_instance_extensions(),
		validation_layers_enabled
	);
	instance.create_surface(win);

	auto creation = vulkan::device::create(instance, device_cfg, aftermath_tracker);
	std::array<std::uint32_t, gpu::queue_type_count> queue_families{};
	queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)] = creation.families.graphics_family.value();
	queue_families[static_cast<std::size_t>(gpu::queue_type::compute)] = creation.families.compute_family.value();

	auto command = vulkan::command::create(creation.device, queue_families);

	auto worker_pools = vulkan::worker_command_pools::create(
		creation.device,
		queue_families,
		task::thread_count()
	);

	const auto surface_format = vulkan::pick_surface_format(creation.device, instance);

	auto dev = std::unique_ptr<device>(new device(
		std::move(aftermath_tracker),
		std::move(instance),
		std::move(creation.device),
		std::move(creation.queue),
		std::move(command),
		std::move(worker_pools),
		surface_format,
		creation.video_encode_enabled
	));

	dev->m_transient = transient_executor::create(
		dev->m_device_config,
		dev->m_device_config.queue_family(queue_type::graphics),
		dev->m_device_config.queue_family(queue_type::compute),
		task::thread_count()
	);

	return dev;
}

gse::gpu::device::device(vulkan::aftermath&& aftermath_tracker, vulkan::instance&& instance, vulkan::device&& device, vulkan::queue&& queue, vulkan::command&& command, vulkan::worker_command_pools&& worker_pools, image_format surface_format, bool video_encode_enabled)
	: m_aftermath(std::move(aftermath_tracker)), m_instance(std::move(instance)), m_device_config(std::move(device)), m_queue(std::move(queue)), m_command(std::move(command)), m_worker_pools(std::move(worker_pools)), m_surface_format(surface_format), m_video_encode_enabled(video_encode_enabled) {
	constexpr std::size_t slot_count = pass_marker_ring_size * 4;
	constexpr std::size_t buffer_size = slot_count * sizeof(std::uint32_t);
	const std::array<std::uint32_t, slot_count> zeros{};

	for (std::size_t di = 0; di < pass_marker_domain_count; ++di) {
		auto& ring = m_pass_marker_rings[di];
		ring.checkpoint_buffer = m_device_config.create_buffer(
			gpu::buffer_create_info{
				.size = buffer_size,
				.usage = gpu::buffer_flag::transfer_dst,
			},
			zeros.data(),
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

auto gse::gpu::device::handle() const -> gpu::handle<vulkan::device> {
	return m_device_config.device_handle();
}

auto gse::gpu::device::surface_format() const -> image_format {
	return m_surface_format;
}

auto gse::gpu::device::queue_family(const gpu::queue_type queue) const -> std::uint32_t {
	return m_device_config.queue_family(queue);
}

auto gse::gpu::device::wait_idle() const -> void {
	m_device_config.wait_idle();
}

auto gse::gpu::device::timestamp_period() const -> float {
	return m_device_config.timestamp_period();
}

auto gse::gpu::device::begin_pass_marker(const gpu::handle<command_buffer> cmd, const pass_marker_domain domain, const pass_marker marker) -> pass_marker_handle {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(domain)];
	const auto seq = ring.seq.fetch_add(1, std::memory_order_relaxed);
	ring.entries[seq % pass_marker_ring_size] = marker;

	if (ring.checkpoint_buffer.valid()) {
		const auto slot = seq % pass_marker_ring_size;
		const auto offset = slot * 4 * sizeof(std::uint32_t);
		commands(cmd).fill_buffer(
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

auto gse::gpu::device::checkpoint_pass_marker(const gpu::handle<command_buffer> cmd, const pass_marker_handle handle) -> void {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + sizeof(std::uint32_t);
	commands(cmd).fill_buffer(
		ring.checkpoint_buffer.handle(),
		offset,
		sizeof(std::uint32_t),
		static_cast<std::uint32_t>(handle.seq)
	);
}

auto gse::gpu::device::post_renderpass_pass_marker(const gpu::handle<command_buffer> cmd, const pass_marker_handle handle) -> void {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 2 * sizeof(std::uint32_t);
	commands(cmd).fill_buffer(
		ring.checkpoint_buffer.handle(),
		offset,
		sizeof(std::uint32_t),
		static_cast<std::uint32_t>(handle.seq)
	);
}

auto gse::gpu::device::end_pass_marker(const gpu::handle<command_buffer> cmd, const pass_marker_handle handle) -> void {
	auto& ring = m_pass_marker_rings[static_cast<std::size_t>(handle.domain)];
	if (!ring.checkpoint_buffer.valid()) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 3 * sizeof(std::uint32_t);
	commands(cmd).fill_buffer(
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
		m_aftermath.wait_for_crash_dump();
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

	if (!m_device_config.fault_enabled()) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"VK_EXT_device_fault is unavailable on this device"
		);
		return;
	}

	device_fault_counts counts{};
	if (const auto result = m_device_config.query_fault_counts(counts); result != gpu::result::success) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"Failed to query device fault counts: {}",
			static_cast<int>(result)
		);
		return;
	}

	if (!m_device_config.vendor_binary_fault_enabled()) {
		counts.vendor_binary_size = 0;
	}

	device_fault_info fault_info{};
	if (const auto result = m_device_config.query_fault_info(counts, fault_info); result != gpu::result::success) {
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

auto gse::gpu::device::frame_command_buffer(const queue_type queue, const std::uint32_t frame_index) const -> gpu::handle<command_buffer> {
	return m_command.frame_command_buffer(queue, frame_index);
}

auto gse::gpu::device::submit(const queue_type queue, const submit_info& info, const gpu::handle<fence> signal_fence) -> void {
	m_queue.submit(queue, info, signal_fence);
}

auto gse::gpu::device::present(const present_info& info) -> result {
	return m_queue.present(info);
}

auto gse::gpu::device::wait_for_fence(const gpu::handle<fence> f, const std::uint64_t timeout_ns) const -> result {
	return vulkan::wait_for_fence(m_device_config, f, timeout_ns);
}

auto gse::gpu::device::reset_fence(const gpu::handle<fence> f) const -> void {
	vulkan::reset_fence(m_device_config, f);
}

auto gse::gpu::device::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	m_worker_pools.reset_frame(frame_index);
}

auto gse::gpu::device::acquire_worker_command_buffer(const queue_type queue, const std::size_t worker_index, const std::uint32_t frame_index) -> gpu::handle<command_buffer> {
	return m_worker_pools.acquire_secondary(queue, worker_index, frame_index);
}

auto gse::gpu::device::make_video_encoder(const vec2u extent) -> std::optional<video_encoder> {
	if (!m_video_encode_enabled) {
		return std::nullopt;
	}
	const auto caps = video_encoder::probe(m_device_config, m_queue);
	if (!caps.available) {
		return std::nullopt;
	}
	return video_encoder::create(m_device_config, m_queue, extent, caps);
}

auto gse::gpu::device::create_image_unbound(const image_create_info& info) const -> std::pair<gpu::handle<image>, memory_requirements> {
	return m_device_config.create_image_unbound(info);
}

auto gse::gpu::device::create_buffer_unbound(const buffer_create_info& info) const -> std::pair<gpu::handle<buffer>, memory_requirements> {
	return m_device_config.create_buffer_unbound(info);
}

auto gse::gpu::device::bind_image_memory(const gpu::handle<image> img, const device_memory_handle mem, const device_size offset) const -> void {
	m_device_config.bind_image_memory(img, mem, offset);
}

auto gse::gpu::device::bind_buffer_memory(const gpu::handle<buffer> buf, const device_memory_handle mem, const device_size offset) const -> void {
	m_device_config.bind_buffer_memory(buf, mem, offset);
}

auto gse::gpu::device::create_image_view(const gpu::handle<image> img, const image_view_create_info& info) const -> gpu::handle<image_view> {
	return m_device_config.create_image_view(img, info);
}

auto gse::gpu::device::allocate_aliased_memory(const device_size size, const std::uint32_t memory_type_index) const -> device_memory_handle {
	return m_device_config.allocate_aliased_memory(size, memory_type_index);
}

auto gse::gpu::device::free_aliased_memory(const device_memory_handle mem) const -> void {
	m_device_config.free_aliased_memory(mem);
}

auto gse::gpu::device::find_memory_type_index(const std::uint32_t type_bits, const memory_property_flags required) const -> std::uint32_t {
	return m_device_config.find_memory_type_index(type_bits, required);
}

auto gse::gpu::device::make_aliased_image(const gpu::handle<image> img_handle, const gpu::handle<image_view> view_handle, const image_format format, const vec3u extent, const image_view_create_info& view_info, const std::string_view tag) -> std::unique_ptr<image> {
	return std::make_unique<image>(
		img_handle,
		view_handle,
		static_cast<image_format_value>(format),
		extent,
		view_info,
		vulkan::basic_allocation<vulkan::device>{
			0, 0, 0, nullptr, nullptr, nullptr,
			std::addressof(m_device_config),
			vulkan::allocation_debug_info{ .tag = std::string(tag) },
		}
	);
}

auto gse::gpu::device::make_aliased_buffer(const gpu::handle<buffer> buf_handle, const device_size size, const std::string_view tag) -> std::unique_ptr<buffer> {
	return std::make_unique<buffer>(
		buf_handle,
		vulkan::basic_allocation<vulkan::device>{
			0, 0, 0, nullptr, nullptr, nullptr,
			std::addressof(m_device_config),
			vulkan::allocation_debug_info{ .tag = std::string(tag) },
		},
		size
	);
}

auto gse::gpu::device::transient() -> transient_executor& {
	return *m_transient;
}

auto gse::gpu::device::video_encode_enabled() const -> bool {
	return m_video_encode_enabled;
}

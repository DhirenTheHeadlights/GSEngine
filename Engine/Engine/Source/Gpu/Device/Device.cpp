module gse.gpu;

import std;

import :aliases;
import :device;
import :descriptor_heap;
import :types;

import :vulkan_buffer;
import :vulkan_command_pools;
import :vulkan_commands;
import :vulkan_device;
import :vulkan_instance;
import :vulkan_queues;
import :vulkan_swapchain;

import gse.os;
import gse.log;
import gse.concurrency;

auto gse::gpu::device::create(const window::state& win, const bool validation_layers_enabled, vulkan::device::settings& device_cfg) -> std::unique_ptr<device> {
	auto instance = vulkan::instance::create(window::vulkan_instance_extensions(), validation_layers_enabled);
	vulkan::create_surface(win, instance);

	auto creation = vulkan::device::create(instance, device_cfg);
	auto command = vulkan::command::create(creation.device, creation.families.graphics_family.value());

	auto worker_pools = vulkan::worker_command_pools::create(
		creation.device,
		command.graphics_family_index(),
		task::thread_count()
	);

	auto transient = transient_executor::create(
		creation.device,
		creation.queue.graphics_family_index(),
		creation.queue.compute_family_index(),
		task::thread_count()
	);

	auto desc_heap = std::make_unique<gpu::descriptor_heap>(
		creation.device,
		creation.desc_buf_props
	);

	const auto surface_format = vulkan::pick_surface_format(creation.device, instance);

	return std::unique_ptr<device>(new device(
		std::move(instance),
		std::move(creation.device),
		std::move(creation.queue),
		std::move(command),
		std::move(worker_pools),
		std::move(transient),
		std::move(desc_heap),
		std::move(creation.desc_buf_props),
		surface_format,
		creation.video_encode_enabled
	));
}

gse::gpu::device::device(vulkan::instance&& instance, vulkan::device&& device, vulkan::queue&& queue, vulkan::command&& command, vulkan::worker_command_pools&& worker_pools, transient_executor&& transient, std::unique_ptr<gpu::descriptor_heap> descriptor_heap, descriptor_buffer_properties desc_buf_props, image_format surface_format, bool video_encode_enabled)
	: m_instance(std::move(instance)),
	  m_device_config(std::move(device)),
	  m_queue(std::move(queue)),
	  m_command(std::move(command)),
	  m_worker_pools(std::move(worker_pools)),
	  m_transient(std::move(transient)),
	  m_descriptor_heap(std::move(descriptor_heap)),
	  m_descriptor_buffer_props(std::move(desc_buf_props)),
	  m_surface_format(surface_format),
	  m_video_encode_enabled(video_encode_enabled) {
	constexpr std::size_t slot_count = pass_marker_ring_size * 4;
	constexpr std::size_t buffer_size = slot_count * sizeof(std::uint32_t);
	const std::array<std::uint32_t, slot_count> zeros{};

	m_pass_checkpoint_buffer = m_device_config.create_buffer(
		gpu::buffer_create_info{
			.size = buffer_size,
			.usage = gpu::buffer_flag::transfer_dst,
		},
		zeros.data(),
		"device.pass_checkpoint"
	);
	m_pass_checkpoint_slots = std::bit_cast<std::uint32_t*>(m_pass_checkpoint_buffer.mapped());
}

gse::gpu::device::~device() {
	log::println(log::category::runtime, "Destroying Device");
}

auto gse::gpu::device::allocator() -> vulkan::device& {
	return m_device_config;
}

auto gse::gpu::device::allocator() const -> const vulkan::device& {
	return m_device_config;
}

auto gse::gpu::device::surface_format() const -> image_format {
	return m_surface_format;
}

auto gse::gpu::device::compute_queue_family() const -> std::uint32_t {
	return m_queue.compute_family_index();
}

auto gse::gpu::device::wait_idle() const -> void {
	m_device_config.wait_idle();
}

auto gse::gpu::device::timestamp_period() const -> float {
	return m_device_config.timestamp_period();
}

auto gse::gpu::device::begin_pass_marker(const handle<command_buffer> cmd, const pass_marker marker) -> pass_marker_handle {
	const auto seq = m_pass_marker_seq.fetch_add(1, std::memory_order_relaxed);
	m_pass_markers[seq % pass_marker_ring_size] = marker;

	if (m_pass_checkpoint_buffer) {
		const auto slot = seq % pass_marker_ring_size;
		const auto offset = slot * 4 * sizeof(std::uint32_t);
		vulkan::commands(cmd).fill_buffer(m_pass_checkpoint_buffer.handle(), offset, sizeof(std::uint32_t), static_cast<std::uint32_t>(seq));
	}

	return pass_marker_handle{ .seq = seq };
}

auto gse::gpu::device::checkpoint_pass_marker(const handle<command_buffer> cmd, const pass_marker_handle handle) -> void {
	if (!m_pass_checkpoint_buffer) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + sizeof(std::uint32_t);
	vulkan::commands(cmd).fill_buffer(m_pass_checkpoint_buffer.handle(), offset, sizeof(std::uint32_t), static_cast<std::uint32_t>(handle.seq));
}

auto gse::gpu::device::post_renderpass_pass_marker(const handle<command_buffer> cmd, const pass_marker_handle handle) -> void {
	if (!m_pass_checkpoint_buffer) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 2 * sizeof(std::uint32_t);
	vulkan::commands(cmd).fill_buffer(m_pass_checkpoint_buffer.handle(), offset, sizeof(std::uint32_t), static_cast<std::uint32_t>(handle.seq));
}

auto gse::gpu::device::end_pass_marker(const handle<command_buffer> cmd, const pass_marker_handle handle) -> void {
	if (!m_pass_checkpoint_buffer) {
		return;
	}

	const auto slot = handle.seq % pass_marker_ring_size;
	const auto offset = slot * 4 * sizeof(std::uint32_t) + 3 * sizeof(std::uint32_t);
	vulkan::commands(cmd).fill_buffer(m_pass_checkpoint_buffer.handle(), offset, sizeof(std::uint32_t), static_cast<std::uint32_t>(handle.seq));
}

auto gse::gpu::device::report_device_lost(const std::string_view operation) -> void {
	if (bool expected = false; !m_device_lost_reported.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
		return;
	}

	log::println(log::level::error, log::category::vulkan, "Vulkan device lost during {}", operation);

	const auto seq_next = m_pass_marker_seq.load(std::memory_order_relaxed);
	const auto total = seq_next > 0 ? seq_next - 1 : 0;
	if (total > 0) {
		const auto count = std::min<std::uint64_t>(total, pass_marker_ring_size);
		const auto first_seq = seq_next - count;
		log::println(log::level::error, log::category::vulkan, "Last {} pass markers (oldest first, status from GPU checkpoint):", count);

		std::uint64_t last_gpu_inflight = 0;
		std::string_view last_gpu_inflight_phase = {};
		bool any_gpu_inflight = false;

		for (std::uint64_t i = 0; i < count; ++i) {
			const auto seq = first_seq + i;
			const auto slot = seq % pass_marker_ring_size;
			const auto& m = m_pass_markers[slot];

			std::string_view status = "queued";
			if (m_pass_checkpoint_slots) {
				const auto begin_seq = m_pass_checkpoint_slots[slot * 4];
				const auto post_barrier_seq = m_pass_checkpoint_slots[slot * 4 + 1];
				const auto post_renderpass_seq = m_pass_checkpoint_slots[slot * 4 + 2];
				const auto end_seq = m_pass_checkpoint_slots[slot * 4 + 3];
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
			const auto& m = m_pass_markers[last_gpu_inflight % pass_marker_ring_size];
			log::println(
				log::level::error,
				log::category::vulkan,
				"GPU hung in {} of: seq={} frame={} pass_index={} pass_type={}",
				last_gpu_inflight_phase,
				last_gpu_inflight,
				m.frame_counter,
				m.pass_index,
				m.pass_type.tag()
			);
		}
	}

	if (!m_device_config.fault_enabled()) {
		log::println(log::level::warning, log::category::vulkan, "VK_EXT_device_fault is unavailable on this device");
		return;
	}

	device_fault_counts counts{};
	if (const auto result = m_device_config.query_fault_counts(counts); result != gpu::result::success) {
		log::println(log::level::warning, log::category::vulkan, "Failed to query device fault counts: {}", static_cast<int>(result));
		return;
	}

	if (!m_device_config.vendor_binary_fault_enabled()) {
		counts.vendor_binary_size = 0;
	}

	device_fault_info fault_info{};
	if (const auto result = m_device_config.query_fault_info(counts, fault_info); result != gpu::result::success) {
		log::println(log::level::warning, log::category::vulkan, "Failed to query device fault info: {}", static_cast<int>(result));
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
		log::println(log::level::error, log::category::vulkan, "Device fault vendor binary size: {} bytes", fault_info.vendor_binary.size());
	}
}

auto gse::gpu::device::vulkan_queue() -> vulkan::queue& {
	return m_queue;
}

auto gse::gpu::device::vulkan_command() -> vulkan::command& {
	return m_command;
}

auto gse::gpu::device::worker_command_pools() -> vulkan::worker_command_pools& {
	return m_worker_pools;
}

auto gse::gpu::device::transient() -> transient_executor& {
	return m_transient;
}

auto gse::gpu::device::descriptor_buffer_props() const -> const descriptor_buffer_properties& {
	return m_descriptor_buffer_props;
}

auto gse::gpu::device::video_encode_enabled() const -> bool {
	return m_video_encode_enabled;
}

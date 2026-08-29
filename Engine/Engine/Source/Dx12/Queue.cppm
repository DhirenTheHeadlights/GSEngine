export module gse.dx12:queue;

import std;
import gse.gpu_backend;
import gse.directx;
import gse.log;

import :device;

export namespace gse::dx12 {
	class queue {
	public:
		auto bind(
			device* owner
		) -> void;

		auto submit(
			gpu::queue_type queue_type,
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence
		) -> void;

		[[nodiscard]] auto wait_for_fence(
			gpu::handle<gpu::fence> f
		) const -> gpu::result;

		auto reset_fence(
			gpu::handle<gpu::fence> f
		) const -> void;

	private:
		device* m_owner = nullptr;
	};
}

auto gse::dx12::queue::bind(device* owner) -> void {
	m_owner = owner;
}

auto gse::dx12::queue::submit(const gpu::queue_type queue_type, const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> void {
	auto* target_queue = m_owner->command_queue(queue_type);
	for (const auto& w : info.wait_semaphores) {
		if (auto* sp = std::bit_cast<sync_point*>(w.semaphore); sp && sp->fence) {
			const auto value = sp->is_timeline ? w.value : sp->value;
			m_owner->record_queue_op(queue_op_kind::wait, queue_type, sp->fence.get(), value, 0);
			target_queue->Wait(sp->fence.get(), value);
		}
	}
	std::vector<directx::ID3D12CommandList*> lists;
	for (const auto& cb : info.command_buffers) {
		if (auto* list = std::bit_cast<directx::ID3D12CommandList*>(cb.command_buffer)) {
			lists.push_back(list);
		}
	}
	if (!lists.empty()) {
		m_owner->record_queue_op(queue_op_kind::execute, queue_type, nullptr, 0, static_cast<std::uint32_t>(lists.size()));
		target_queue->ExecuteCommandLists(static_cast<std::uint32_t>(lists.size()), lists.data());
		if (m_owner->validation_enabled()) {
			directx::drain_debug_messages(m_owner->raw_device(), [](void*, const char* message) {
				log::println(log::level::warning, log::category::dx12_validation, "{}", message);
			}, nullptr);
		}
		if (const auto r = m_owner->raw_device()->GetDeviceRemovedReason(); r != 0) {
			log::println(log::level::error, log::category::dx12, "post-ExecuteCommandLists removed=0x{:08x} lists={}", static_cast<std::uint32_t>(r), lists.size());
			m_owner->dump_dred_once();
		}
	}
	for (const auto& s : info.signal_semaphores) {
		if (auto* sp = std::bit_cast<sync_point*>(s.semaphore); sp && sp->fence) {
			if (sp->is_timeline) {
				m_owner->record_queue_op(queue_op_kind::signal, queue_type, sp->fence.get(), s.value, 0);
				target_queue->Signal(sp->fence.get(), s.value);
				sp->value = s.value;
			}
			else {
				const auto value = ++sp->value;
				m_owner->record_queue_op(queue_op_kind::signal, queue_type, sp->fence.get(), value, 0);
				target_queue->Signal(sp->fence.get(), value);
			}
		}
	}
	if (auto* sp = std::bit_cast<sync_point*>(signal_fence); sp && sp->fence) {
		const auto value = ++sp->value;
		m_owner->record_queue_op(queue_op_kind::signal, queue_type, sp->fence.get(), value, 0);
		target_queue->Signal(sp->fence.get(), value);
	}
}

auto gse::dx12::queue::wait_for_fence(const gpu::handle<gpu::fence> f) const -> gpu::result {
	if (auto* sp = std::bit_cast<sync_point*>(f); sp && sp->fence && sp->value != 0) {
		m_owner->record_queue_op(queue_op_kind::cpu_wait, gpu::queue_type::graphics, sp->fence.get(), sp->value, 0);
		directx::wait_fence(sp->fence.get(), sp->value, m_owner->idle_event());
	}
	if (const auto r = m_owner->raw_device()->GetDeviceRemovedReason(); r != 0) {
		log::println(log::level::error, log::category::dx12, "wait_for_fence removed=0x{:08x}", static_cast<std::uint32_t>(r));
		m_owner->dump_dred_once();
		return gpu::result::error_device_lost;
	}
	return gpu::result::success;
}

auto gse::dx12::queue::reset_fence(gpu::handle<gpu::fence>) const -> void {}

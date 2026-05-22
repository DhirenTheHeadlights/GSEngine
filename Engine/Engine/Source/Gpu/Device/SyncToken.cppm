export module gse.gpu:sync_token;

import std;

import :transient_queue;
import :vulkan_device;

export namespace gse::gpu {
	class sync_token {
	public:
		sync_token() = default;

		sync_token(
			transient_queue* queue,
			std::uint64_t value
		);

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto ready() const -> bool;

		auto wait(
			const vulkan::device& device
		) const -> void;

		[[nodiscard]] auto operator co_await() const noexcept;

	private:
		struct awaiter {
			transient_queue* m_queue;
			std::uint64_t m_value;

			auto await_ready() const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> caller
			) -> bool;

			auto await_resume() const noexcept -> void;
		};

		transient_queue* m_queue = nullptr;
		std::uint64_t m_value = 0;
	};
}

gse::gpu::sync_token::sync_token(transient_queue* queue, const std::uint64_t value) : m_queue(queue), m_value(value) {
}

auto gse::gpu::sync_token::valid() const -> bool {
	return m_queue != nullptr;
}

auto gse::gpu::sync_token::ready() const -> bool {
	if (!m_queue) {
		return true;
	}
	return m_queue->reached(m_value);
}

auto gse::gpu::sync_token::wait(const vulkan::device& device) const -> void {
	if (!m_queue) {
		return;
	}
	m_queue->wait_until(device, m_value);
}

auto gse::gpu::sync_token::operator co_await() const noexcept {
	return awaiter{ m_queue, m_value };
}

auto gse::gpu::sync_token::awaiter::await_ready() const noexcept -> bool {
	if (!m_queue) {
		return true;
	}
	return m_queue->reached(m_value);
}

auto gse::gpu::sync_token::awaiter::await_suspend(std::coroutine_handle<> caller) -> bool {
	if (!m_queue) {
		return false;
	}
	if (m_queue->reached(m_value)) {
		return false;
	}
	m_queue->park(m_value, caller);
	return true;
}

auto gse::gpu::sync_token::awaiter::await_resume() const noexcept -> void {
}

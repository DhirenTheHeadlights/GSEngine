export module gse.concurrency:mpsc_ring_buffer;

import std;

export namespace gse {
	template <typename T, std::size_t Capacity>
	class mpsc_ring_buffer {
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
	public:
		auto push(
			const T& value
		) -> bool;

		auto push(
			T&& value
		) -> bool;

		auto pop(
			T& out
		) -> bool;
	private:
		static constexpr auto index(
			std::size_t i
		) -> std::size_t;

		std::array<T, Capacity> m_data{};
		alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_head{ 0 };
		alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_tail{ 0 };
	};
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::push(const T& value) -> bool {
	std::size_t head_old;
	for (;;) {
		head_old = m_head.load(std::memory_order_relaxed);
		if (const auto tail = m_tail.load(std::memory_order_acquire); head_old - tail >= Capacity) {
			return false;
		}
		if (m_head.compare_exchange_weak(
				head_old,
				head_old + 1,
				std::memory_order_acq_rel,
				std::memory_order_relaxed
			)) {
			break;
		}
	}
	m_data[index(head_old)] = value;
	return true;
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::push(T&& value) -> bool {
	std::size_t head_old;
	for (;;) {
		head_old = m_head.load(std::memory_order_relaxed);
		if (const auto tail = m_tail.load(std::memory_order_acquire); head_old - tail >= Capacity) {
			return false;
		}
		if (m_head.compare_exchange_weak(
				head_old,
				head_old + 1,
				std::memory_order_acq_rel,
				std::memory_order_relaxed
			)) {
			break;
		}
	}
	m_data[index(head_old)] = std::move(value);
	return true;
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::pop(T& out) -> bool {
	const auto tail = m_tail.load(std::memory_order_relaxed);
	if (const auto head = m_head.load(std::memory_order_acquire); tail == head) {
		return false;
	}
	out = std::move(m_data[index(tail)]);
	m_tail.store(tail + 1, std::memory_order_release);
	return true;
}

template <typename T, std::size_t Capacity>
constexpr auto gse::mpsc_ring_buffer<T, Capacity>::index(const std::size_t i) -> std::size_t {
	return i & (Capacity - 1);
}


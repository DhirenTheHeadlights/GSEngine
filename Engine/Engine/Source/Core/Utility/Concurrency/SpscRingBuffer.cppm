export module gse.utility:spsc_ring_buffer;

import std;

export namespace gse {
	template <typename T, std::size_t Capacity>
	class spsc_ring_buffer {
	public:
		auto push(
			const T& value
		) -> bool;

		auto pop(
			T& out
		) -> bool;
	private:
		static constexpr auto increment(
			std::size_t i
		) -> std::size_t;

		std::array<T, Capacity> m_data{};
		std::atomic<std::size_t> m_head{ 0 };
		std::atomic<std::size_t> m_tail{ 0 };
	};
}

template <typename T, std::size_t Capacity>
auto gse::spsc_ring_buffer<T, Capacity>::push(const T& value) -> bool {
	const auto head = m_head.load(std::memory_order_relaxed);
	const auto next = increment(head);
	if (next == m_tail.load(std::memory_order_acquire)) {
		return false;
	}
	m_data[head] = value;
	m_head.store(next, std::memory_order_release);
	return true;
}

template <typename T, std::size_t Capacity>
auto gse::spsc_ring_buffer<T, Capacity>::pop(T& out) -> bool {
	const auto tail = m_tail.load(std::memory_order_relaxed);
	if (tail == m_head.load(std::memory_order_acquire)) {
		return false;
	}
	out = std::move(m_data[tail]);
	m_tail.store(increment(tail), std::memory_order_release);
	return true;
}

template <typename T, std::size_t Capacity>
constexpr auto gse::spsc_ring_buffer<T, Capacity>::increment(const std::size_t i) -> std::size_t {
	return (i + 1) % Capacity;
}


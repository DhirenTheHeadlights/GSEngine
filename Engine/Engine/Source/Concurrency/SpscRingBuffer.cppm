export module gse.concurrency:spsc_ring_buffer;

import std;

export namespace gse {
	template <typename T, std::size_t Capacity>
	class spsc_ring_buffer {
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
	public:
		auto push(
			const T& value
		) -> bool;

		auto pop(
			T& out
		) -> bool;
	private:
		static constexpr auto mask(
			std::size_t i
		) -> std::size_t;

		std::array<T, Capacity> m_data{};
		alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_head{ 0 };
		alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> m_tail{ 0 };
	};
}

template <typename T, std::size_t Capacity>
auto gse::spsc_ring_buffer<T, Capacity>::push(const T& value) -> bool {
	const auto head = m_head.load(std::memory_order_relaxed);
	const auto next = mask(head + 1);
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
	m_tail.store(mask(tail + 1), std::memory_order_release);
	return true;
}

template <typename T, std::size_t Capacity>
constexpr auto gse::spsc_ring_buffer<T, Capacity>::mask(const std::size_t i) -> std::size_t {
	return i & (Capacity - 1);
}


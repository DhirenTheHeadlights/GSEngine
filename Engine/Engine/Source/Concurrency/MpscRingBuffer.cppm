export module gse.concurrency:mpsc_ring_buffer;

import std;

export namespace gse {
	template <typename T, std::size_t Capacity>
	class mpsc_ring_buffer {
		static_assert(
			(Capacity & (Capacity - 1)) == 0,
			"Capacity must be a power of two"
		);

	public:
		mpsc_ring_buffer();

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
		struct slot {
			T value{};
			std::atomic<std::size_t> sequence{ 0 };
		};

		auto claim() -> std::optional<std::size_t>;

		static constexpr auto index(
			std::size_t i
		) -> std::size_t;

		static constexpr std::size_t cache_line_size = 64;

		std::array<slot, Capacity> m_slots;
		alignas(cache_line_size) std::atomic<std::size_t> m_head{ 0 };
		alignas(cache_line_size) std::size_t m_tail = 0;
	};
}

template <typename T, std::size_t Capacity>
gse::mpsc_ring_buffer<T, Capacity>::mpsc_ring_buffer() {
	for (std::size_t i = 0; i < Capacity; ++i) {
		m_slots[i].sequence.store(i, std::memory_order_relaxed);
	}
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::claim() -> std::optional<std::size_t> {
	auto pos = m_head.load(std::memory_order_relaxed);
	for (;;) {
		const auto sequence = m_slots[index(pos)].sequence.load(std::memory_order_acquire);
		const auto lag = static_cast<std::ptrdiff_t>(sequence) - static_cast<std::ptrdiff_t>(pos);
		if (lag == 0) {
			if (m_head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				return pos;
			}
		}
		else if (lag < 0) {
			return std::nullopt;
		}
		else {
			pos = m_head.load(std::memory_order_relaxed);
		}
	}
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::push(const T& value) -> bool {
	const auto pos = claim();
	if (!pos) {
		return false;
	}
	auto& s = m_slots[index(*pos)];
	s.value = value;
	s.sequence.store(*pos + 1, std::memory_order_release);
	return true;
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::push(T&& value) -> bool {
	const auto pos = claim();
	if (!pos) {
		return false;
	}
	auto& s = m_slots[index(*pos)];
	s.value = std::move(value);
	s.sequence.store(*pos + 1, std::memory_order_release);
	return true;
}

template <typename T, std::size_t Capacity>
auto gse::mpsc_ring_buffer<T, Capacity>::pop(T& out) -> bool {
	auto& s = m_slots[index(m_tail)];
	if (s.sequence.load(std::memory_order_acquire) != m_tail + 1) {
		return false;
	}
	out = std::move(s.value);
	s.sequence.store(m_tail + Capacity, std::memory_order_release);
	++m_tail;
	return true;
}

template <typename T, std::size_t Capacity>
constexpr auto gse::mpsc_ring_buffer<T, Capacity>::index(const std::size_t i) -> std::size_t {
	return i & (Capacity - 1);
}

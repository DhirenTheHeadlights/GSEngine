export module gse.containers:n_buffer;

import std;

export namespace gse {
	template <typename T, std::size_t N>
	class n_buffer {
	public:
		static_assert(N >= 2, "n_buffer requires at least 2 buffers");

		using value_type = T;

		n_buffer() = default;

		explicit n_buffer(
			const value_type& initial_value
		);

		auto operator=(
			const value_type& v
		) -> n_buffer&;

		auto write(
		) -> value_type&;

		auto publish(
		) noexcept -> void;

		[[nodiscard]] auto read(
		) const -> const value_type&;

		auto take_ready(
		) noexcept -> value_type;

		auto flip(
		) noexcept -> void;

		auto buffer(
			this auto&& self
		) -> decltype(auto);

	private:
		auto next_write_index(
			std::size_t current
		) const noexcept -> std::size_t;

		std::array<value_type, N> m_buffers{};
		std::size_t m_write_index{ 0 };
		std::atomic<std::size_t> m_ready_index{ N - 1 };
	};

	template <typename T>
	using double_buffer = n_buffer<T, 2>;

	template <typename T>
	using triple_buffer = n_buffer<T, 3>;
}

template <typename T, std::size_t N>
gse::n_buffer<T, N>::n_buffer(const value_type& initial_value) {
	for (auto& buf : m_buffers) {
		buf = initial_value;
	}
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::operator=(const value_type& v) -> n_buffer& {
	for (auto& buf : m_buffers) {
		buf = v;
	}
	std::atomic_thread_fence(std::memory_order_release);
	m_ready_index.store(N - 1, std::memory_order_release);
	m_write_index = 0;
	return *this;
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::write() -> value_type& {
	return m_buffers[m_write_index];
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::publish() noexcept -> void {
	const auto wrote = m_write_index;
	m_ready_index.store(wrote, std::memory_order_release);
	m_write_index = next_write_index(wrote);
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::read() const -> const value_type& {
	return m_buffers[m_ready_index.load(std::memory_order_acquire)];
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::take_ready() noexcept -> value_type {
	return std::move(m_buffers[m_ready_index.load(std::memory_order_acquire)]);
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::flip() noexcept -> void {
	publish();
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::buffer(this auto&& self) -> decltype(auto) {
	return std::span{ self.m_buffers };
}

template <typename T, std::size_t N>
auto gse::n_buffer<T, N>::next_write_index(const std::size_t current) const noexcept -> std::size_t {
	auto next = (current + 1) % N;
	if (const auto ready = m_ready_index.load(std::memory_order_relaxed); next == ready && N > 2) {
		next = (next + 1) % N;
	}
	return next;
}

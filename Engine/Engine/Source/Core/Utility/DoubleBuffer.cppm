export module gse.utility:double_buffer;

import std;

export namespace gse {
    template <typename T>
    class double_buffer {
    public:
        using value_type = T;

        double_buffer() = default;

        double_buffer(
            const value_type& initial_value
        );

        auto operator=(const value_type& v) -> double_buffer&;

        auto write(
        ) -> value_type&;

        auto read(
        ) -> const value_type&;

        auto flip(
        ) noexcept -> void;

        auto buffer(
            this auto&& self
        ) -> decltype(auto);
    private:
        std::array<value_type, 2> m_buffer{};
        std::atomic<std::uint8_t> m_read{0};
        std::uint8_t m_write{1};
    };
}

template <typename T>
gse::double_buffer<T>::double_buffer(const value_type& initial_value) : m_buffer{ initial_value, initial_value } {}

template <typename T>
auto gse::double_buffer<T>::operator=(const value_type& v) -> double_buffer& {
	m_buffer[0] = v;
    m_buffer[1] = v;
    std::atomic_thread_fence(std::memory_order_release);
    m_read.store(0, std::memory_order_release);
    m_write = 1;
    return *this;
}

template <typename T>
auto gse::double_buffer<T>::write() -> value_type& {
    return m_buffer[m_write];
}

template <typename T>
auto gse::double_buffer<T>::read() -> const value_type& {
    return m_buffer[m_read.load(std::memory_order_acquire)];
}

template <typename T>
auto gse::double_buffer<T>::flip() noexcept -> void {
    std::atomic_thread_fence(std::memory_order_release);
    m_read.store(m_write, std::memory_order_release);
    m_write ^= 1;
}

template <typename T>
auto gse::double_buffer<T>::buffer(this auto&& self) -> decltype(auto) {
    return self.m_buffer;
}

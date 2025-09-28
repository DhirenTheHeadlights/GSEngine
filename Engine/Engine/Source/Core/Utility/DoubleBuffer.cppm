export module gse.utility:double_buffer;

import std;

export namespace gse {
	template <typename T>
	class double_buffer {
	public:
		using value_type = T;

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
		std::size_t m_read = 0;
		std::size_t m_write = 0;
	};
}

template <typename T>
auto gse::double_buffer<T>::write() -> value_type& {
	return m_buffer[m_write];
}

template <typename T>
auto gse::double_buffer<T>::read() -> const value_type& {
	return m_buffer[m_read];
}

template <typename T>
auto gse::double_buffer<T>::flip() noexcept -> void {
	std::swap(m_read, m_write);
}

template <typename T>
auto gse::double_buffer<T>::buffer(this auto&& self) -> decltype(auto) {
	return self.m_buffer;
}


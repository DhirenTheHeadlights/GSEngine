export module gse.utility:flags;

import std;

export namespace gse {
	template <typename E>
		requires std::is_enum_v<E>
	class flags {
	public:
		using enum_type = E;
		using underlying_type = std::underlying_type_t<E>;

		constexpr flags() = default;
		constexpr flags(E e);

		constexpr auto set(
			E e
		) -> flags&;

		constexpr auto clear(
			E e
		) -> flags&;

		constexpr auto test(
			E e
		) const -> bool;

		constexpr auto operator|(
			flags rhs
		) const -> flags;

		constexpr auto operator&(
			flags rhs
		) const -> flags;

		constexpr auto operator|=(
			flags rhs
		) -> flags&;

		constexpr auto operator&=(
			flags rhs
		) -> flags&;

		constexpr auto operator|(
			E e
		) const -> flags;

		constexpr auto operator&(
			E e
		) const -> flags;

		constexpr explicit operator bool(
		) const;

		constexpr auto bits(
		) const -> underlying_type;

		static constexpr auto from_bits(
			underlying_type raw
		) -> flags;
	private:
		constexpr explicit flags(underlying_type bits);

		underlying_type m_bits = 0;
	};

}

template <typename E>
	requires std::is_enum_v<E>
constexpr gse::flags<E>::flags(E e) : m_bits(static_cast<underlying_type>(e)) {}

template <typename E>
	requires std::is_enum_v<E>
constexpr gse::flags<E>::flags(underlying_type bits) : m_bits(bits) {}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::set(E e) -> flags& {
	m_bits |= static_cast<underlying_type>(e);
	return *this;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::clear(E e) -> flags& {
	m_bits &= ~static_cast<underlying_type>(e);
	return *this;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::test(E e) const -> bool {
	return (m_bits & static_cast<underlying_type>(e)) != 0;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::operator|(flags rhs) const -> flags {
	return flags{ static_cast<underlying_type>(m_bits | rhs.m_bits) };
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::operator&(flags rhs) const -> flags {
	return flags{ static_cast<underlying_type>(m_bits & rhs.m_bits) };
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::operator|=(flags rhs) -> flags& {
	m_bits |= rhs.m_bits;
	return *this;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::operator&=(flags rhs) -> flags& {
	m_bits &= rhs.m_bits;
	return *this;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::operator|(E e) const -> flags {
	return *this | flags{ e };
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::operator&(E e) const -> flags {
	return *this & flags{ e };
}

template <typename E>
	requires std::is_enum_v<E>
constexpr gse::flags<E>::operator bool() const {
	return m_bits != 0;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::bits() const -> underlying_type {
	return m_bits;
}

template <typename E>
	requires std::is_enum_v<E>
constexpr auto gse::flags<E>::from_bits(underlying_type raw) -> flags {
	return flags{ raw };
}


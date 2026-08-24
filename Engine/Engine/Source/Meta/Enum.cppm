export module gse.meta:enums;

import std;

export namespace gse {

	template <typename E>
	requires std::is_enum_v<E>
	constexpr auto enum_to_string(
		E value
	) -> std::string_view;

	template <typename E>
	requires std::is_enum_v<E>
	constexpr auto enum_from_string(
		std::string_view name,
		E& out
	) -> bool;

	template <typename E>
	requires std::is_enum_v<E>
	constexpr auto enum_values() -> std::span<const E>;
}

namespace gse {
	template <typename E>
	requires std::is_enum_v<E>
	consteval auto enum_value_list() -> std::vector<E>;
}

template <typename E>
requires std::is_enum_v<E>
consteval auto gse::enum_value_list() -> std::vector<E> {
	std::vector<E> out;
	template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
		out.push_back([:v:]);
	}
	return out;
}

template <typename E>
requires std::is_enum_v<E>
constexpr auto gse::enum_values() -> std::span<const E> {
	return std::define_static_array(enum_value_list<E>());
}

template <typename E>
requires std::is_enum_v<E>
constexpr auto gse::enum_to_string(const E value) -> std::string_view {
	template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
		if ([:v:] == value) {
			return std::meta::identifier_of(v);
		}
	}
	return "<unknown>";
}

template <typename E>
requires std::is_enum_v<E>
constexpr auto gse::enum_from_string(const std::string_view name, E& out) -> bool {
	template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
		if (std::meta::identifier_of(v) == name) {
			out = [:v:];
			return true;
		}
	}
	return false;
}

export template <typename E>
requires std::is_enum_v<E>
struct std::formatter<E> : std::formatter<std::string_view> {
	auto format(E value, auto& ctx) const {
		return std::formatter<std::string_view>::format(gse::enum_to_string(value), ctx);
	}
};

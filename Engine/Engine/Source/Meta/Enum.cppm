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
	auto enum_from_ordinal(
		std::string_view text,
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

template <typename E>
requires std::is_enum_v<E>
auto gse::enum_from_ordinal(const std::string_view text, E& out) -> bool {
	std::underlying_type_t<E> ordinal{};
	const auto* first = text.data();
	const auto* last = first + text.size();
	const auto result = std::from_chars(first, last, ordinal);
	if (result.ec != std::errc{} || result.ptr != last) {
		return false;
	}
	template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
		if (static_cast<std::underlying_type_t<E>>([:v:]) == ordinal) {
			out = [:v:];
			return true;
		}
	}
	return false;
}

export template <typename E>
requires std::is_enum_v<E>
struct std::formatter<E> : formatter<string_view> {
	auto format(E value, auto& ctx) const {
		return formatter<string_view>::format(gse::enum_to_string(value), ctx);
	}
};
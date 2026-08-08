export module gse.meta:parse;

import std;

import :enums;

export namespace gse {
	template <typename T>
	struct parser;

	template <typename T>
	auto parse(
		std::string_view raw,
		T& out
	) -> bool;
}

template <>
struct gse::parser<bool> {
	static auto parse(
		std::string_view raw,
		bool& out
	) -> bool;
};

template <>
struct gse::parser<std::string> {
	static auto parse(
		std::string_view raw,
		std::string& out
	) -> bool;
};

export template <typename T>
requires std::is_arithmetic_v<T> && (!std::same_as<T, bool>)
struct gse::parser<T> {
	static auto parse(
		std::string_view raw,
		T& out
	) -> bool;
};

export template <typename E>
requires std::is_enum_v<E>
struct gse::parser<E> {
	static auto parse(
		std::string_view raw,
		E& out
	) -> bool;
};

template <typename T>
auto gse::parse(const std::string_view raw, T& out) -> bool {
	return parser<T>::parse(raw, out);
}

auto gse::parser<bool>::parse(const std::string_view raw, bool& out) -> bool {
	if (raw == "true") {
		out = true;
		return true;
	}
	if (raw == "false") {
		out = false;
		return true;
	}
	return false;
}

auto gse::parser<std::string>::parse(const std::string_view raw, std::string& out) -> bool {
	out = std::string(raw);
	return true;
}

template <typename T>
requires std::is_arithmetic_v<T> && (!std::same_as<T, bool>)
auto gse::parser<T>::parse(const std::string_view raw, T& out) -> bool {
	if constexpr (std::is_floating_point_v<T>) {
		try {
			out = static_cast<T>(std::stod(std::string(raw)));
			return true;
		}
		catch (...) {
			return false;
		}
	}
	else {
		T value{};
		const auto* first = raw.data();
		const auto* last = first + raw.size();
		if (std::from_chars(first, last, value).ec != std::errc{}) {
			return false;
		}
		out = value;
		return true;
	}
}

template <typename E>
requires std::is_enum_v<E>
auto gse::parser<E>::parse(const std::string_view raw, E& out) -> bool {
	return enum_from_string(raw, out);
}

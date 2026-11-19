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
requires requires(std::string_view raw, T& out) {
	{ gse::parser<T>::parse(raw, out) } -> std::same_as<bool>;
}
struct gse::parser<std::vector<T>> {
	static auto parse(
		std::string_view raw,
		std::vector<T>& out
	) -> bool;
};

template <>
struct gse::parser<std::filesystem::path> {
	static auto parse(
		std::string_view raw,
		std::filesystem::path& out
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
requires requires(std::string_view raw, T& out) {
	{ gse::parser<T>::parse(raw, out) } -> std::same_as<bool>;
}
auto gse::parser<std::vector<T>>::parse(const std::string_view raw, std::vector<T>& out) -> bool {
	T element{};
	if (!parser<T>::parse(raw, element)) {
		return false;
	}
	out.push_back(std::move(element));
	return true;
}

auto gse::parser<std::filesystem::path>::parse(const std::string_view raw, std::filesystem::path& out) -> bool {
	out = std::filesystem::path(raw);
	return true;
}

template <typename T>
requires std::is_arithmetic_v<T> && (!std::same_as<T, bool>)
auto gse::parser<T>::parse(const std::string_view raw, T& out) -> bool {
	if constexpr (std::is_floating_point_v<T>) {
		const std::size_t begin = raw.find_first_not_of(" \t\n\r\f\v");
		if (begin == std::string_view::npos) {
			return false;
		}
		std::string_view text = raw.substr(begin);
		const bool negative = text.starts_with('-');
		if (negative || text.starts_with('+')) {
			text.remove_prefix(1);
		}
		if (text.starts_with('-') || text.starts_with('+')) {
			return false;
		}
		const bool hexadecimal = text.starts_with("0x") || text.starts_with("0X");
		if (hexadecimal) {
			text.remove_prefix(2);
		}
		T value{};
		const auto* first = text.data();
		const auto* last = first + text.size();
		const std::chars_format format = hexadecimal ? std::chars_format::hex : std::chars_format::general;
		if (std::from_chars(first, last, value, format).ec != std::errc{}) {
			return false;
		}
		out = negative ? -value : value;
		return true;
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
	return enum_from_string(raw, out) || enum_from_ordinal(raw, out);
}

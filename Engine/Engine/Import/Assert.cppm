export module gse.assert;

import std;

export namespace gse {
	template <typename... Args>
	struct fmt_loc {
		std::format_string<Args...> fmt;
		std::source_location loc;

		template <typename T>
		requires std::convertible_to<const T&, std::string_view>
		consteval fmt_loc(const T& s, std::source_location l = std::source_location::current()) : fmt(s), loc(l) {
		}
	};

	template <typename... Args>
	auto assert(
		bool condition,
		fmt_loc<std::type_identity_t<Args>...> f,
		Args&&... args
	) -> void;

	template <typename... Args>
	auto assert(
		bool condition,
		std::source_location loc,
		std::format_string<std::type_identity_t<Args>...> fmt,
		Args&&... args
	) -> void;
}

namespace gse {
	[[noreturn]] auto assert_fail(
		std::source_location loc,
		std::string_view comment
	) noexcept -> void;
}

template <class... Args>
auto gse::assert(const bool condition, fmt_loc<std::type_identity_t<Args>...> f, Args&&... args) -> void {
	if (condition) {
		return;
	}

	const std::string comment = std::format(f.fmt, std::forward<Args>(args)...);
	assert_fail(f.loc, comment);
}

template <class... Args>
auto gse::assert(const bool condition, const std::source_location loc, std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) -> void {
	if (condition) {
		return;
	}

	const std::string comment = std::format(fmt, std::forward<Args>(args)...);
	assert_fail(loc, comment);
}

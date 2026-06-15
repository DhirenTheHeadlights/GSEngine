export module gse.meta:hash;

import std;

export namespace gse {
	template <typename T>
	auto hash_combine(
		const T& value
	) -> std::size_t;

	template <typename T>
	auto layout_hash() -> std::uint64_t;
}

template <typename T>
auto gse::hash_combine(const T& value) -> std::size_t {
	std::size_t h = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_type = std::remove_cvref_t<decltype(value.[:m:])>;
		const std::size_t sub = std::hash<member_type>{}(value.[:m:]);
		h ^= sub + 0x9e3779b9u + (h << 6) + (h >> 2);
	}
	return h;
}

template <typename T>
auto gse::layout_hash() -> std::uint64_t {
	auto h = std::uint64_t{14695981039346656037ull};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		for (const char c : std::meta::identifier_of(m)) {
			h ^= static_cast<std::uint64_t>(c);
			h *= std::uint64_t{1099511628211ull};
		}
	}
	return h;
}

export template <typename T>
requires(std::is_class_v<T> && !std::is_polymorphic_v<T>)
struct std::hash<T> {
	auto operator()(const T& value) const noexcept -> std::size_t {
		return gse::hash_combine(value);
	}
};

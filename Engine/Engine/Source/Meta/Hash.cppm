export module gse.meta:hash;

import std;

export namespace gse {
	constexpr auto hash_combine(
		std::uint64_t seed,
		std::uint64_t value
	) -> std::uint64_t;

	template <typename T>
	auto hash_combine(
		std::uint64_t seed,
		const T& value
	) -> std::uint64_t;

	template <typename T>
	auto hash_of(
		const T& value
	) -> std::uint64_t;

	template <typename T>
	auto layout_hash() -> std::uint64_t;
}

constexpr auto gse::hash_combine(std::uint64_t seed, const std::uint64_t value) -> std::uint64_t {
	seed ^= value;
	seed += 0x9E3779B97F4A7C15ull;
	seed = (seed ^ seed >> 30) * 0xBF58476D1CE4E5B9ull;
	seed = (seed ^ seed >> 27) * 0x94D049BB133111EBull;
	return seed ^ seed >> 31;
}

template <typename T>
auto gse::hash_combine(const std::uint64_t seed, const T& value) -> std::uint64_t {
	return hash_combine(seed, static_cast<std::uint64_t>(std::hash<T>{}(value)));
}

template <typename T>
auto gse::hash_of(const T& value) -> std::uint64_t {
	std::uint64_t h = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_type = std::remove_cvref_t<decltype(value.[:m:])>;
		h = hash_combine(h, static_cast<std::uint64_t>(std::hash<member_type>{}(value.[:m:])));
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
		return static_cast<std::size_t>(gse::hash_of(value));
	}
};

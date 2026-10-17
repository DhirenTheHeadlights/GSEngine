export module gse.meta:hash;

import std;

export namespace gse {
	template <typename... Ts>
	constexpr auto hash_of(
		const Ts&... values
	) -> std::uint64_t;

	template <typename T>
	auto layout_hash() -> std::uint64_t;
}

namespace gse {
	constexpr auto hash_combine(
		std::uint64_t seed,
		std::uint64_t value
	) -> std::uint64_t;

	template <typename C>
	constexpr auto hash_units(
		std::basic_string_view<C> units
	) -> std::uint64_t;

	consteval auto specializes(
		std::meta::info type,
		std::meta::info family
	) -> bool;

	template <typename T>
	concept hash_unordered =
		specializes(^^T, ^^std::unordered_map)
		|| specializes(^^T, ^^std::unordered_set)
		|| specializes(^^T, ^^std::unordered_multimap)
		|| specializes(^^T, ^^std::unordered_multiset);

	template <typename T>
	concept hash_tuple_like = requires {
		std::tuple_size<T>::value;
	};

	template <typename T>
	constexpr auto hash_into(
		std::uint64_t seed,
		const T& value
	) -> std::uint64_t;
}

constexpr auto gse::hash_combine(std::uint64_t seed, const std::uint64_t value) -> std::uint64_t {
	seed ^= value;
	seed += 0x9E3779B97F4A7C15ull;
	seed = (seed ^ seed >> 30) * 0xBF58476D1CE4E5B9ull;
	seed = (seed ^ seed >> 27) * 0x94D049BB133111EBull;
	return seed ^ seed >> 31;
}

template <typename C>
constexpr auto gse::hash_units(const std::basic_string_view<C> units) -> std::uint64_t {
	std::uint64_t h = 14695981039346656037ull;
	for (const C c : units) {
		h ^= static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<C>>(c));
		h *= 1099511628211ull;
	}
	return h;
}

consteval auto gse::specializes(const std::meta::info type, const std::meta::info family) -> bool {
	const std::meta::info entity = std::meta::dealias(type);
	return std::meta::has_template_arguments(entity) && std::meta::template_of(entity) == family;
}

template <typename T>
constexpr auto gse::hash_into(const std::uint64_t seed, const T& value) -> std::uint64_t {
	static_assert(!std::is_pointer_v<T> && !std::is_member_pointer_v<T> && !std::is_null_pointer_v<T>, "hash_of: pointers hash an address, which differs between runs; hash the pointee or its id");
	static_assert(!std::is_union_v<T>, "hash_of: a union has no single active member to hash");
	static_assert(!std::is_polymorphic_v<T>, "hash_of: polymorphic types carry identity, not value");
	static_assert(!hash_unordered<T>, "hash_of: unordered containers iterate in an unspecified order");

	if constexpr (std::is_enum_v<T>) {
		return hash_combine(seed, static_cast<std::uint64_t>(std::to_underlying(value)));
	}
	else if constexpr (std::is_integral_v<T>) {
		return hash_combine(seed, static_cast<std::uint64_t>(value));
	}
	else if constexpr (std::is_floating_point_v<T>) {
		static_assert(sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t), "hash_of: unsupported floating-point width");
		using bits = std::conditional_t<sizeof(T) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
		const T canonical = value == T{ 0 } ? T{ 0 } : value;
		return hash_combine(seed, static_cast<std::uint64_t>(std::bit_cast<bits>(canonical)));
	}
	else if constexpr (std::same_as<T, std::filesystem::path>) {
		return hash_into(seed, value.native());
	}
	else if constexpr (std::convertible_to<const T&, std::string_view>) {
		return hash_combine(seed, hash_units(std::string_view(value)));
	}
	else if constexpr (std::convertible_to<const T&, std::wstring_view>) {
		return hash_combine(seed, hash_units(std::wstring_view(value)));
	}
	else if constexpr (specializes(^^T, ^^std::optional)) {
		const std::uint64_t engaged = hash_combine(seed, value.has_value() ? 1ull : 0ull);
		return value.has_value() ? hash_into(engaged, *value) : engaged;
	}
	else if constexpr (specializes(^^T, ^^std::variant)) {
		const std::uint64_t indexed = hash_combine(seed, static_cast<std::uint64_t>(value.index()));
		return std::visit(
			[indexed](const auto& held) {
				return hash_into(indexed, held);
			},
			value
		);
	}
	else if constexpr (std::ranges::input_range<const T>) {
		static_assert(!std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<const T>>, T>, "hash_of: a range whose elements are itself never terminates");
		std::uint64_t h = seed;
		std::uint64_t count = 0;
		for (const auto& element : value) {
			h = hash_into(h, element);
			++count;
		}
		return hash_combine(h, count);
	}
	else if constexpr (hash_tuple_like<T>) {
		return std::apply(
			[seed](const auto&... elements) {
				std::uint64_t h = seed;
				((h = hash_into(h, elements)), ...);
				return h;
			},
			value
		);
	}
	else {
		static_assert(std::is_class_v<T>, "hash_of: no hashing rule for this type");
		std::uint64_t h = seed;
		template for (constexpr auto b : std::define_static_array(std::meta::bases_of(^^T, std::meta::access_context::unchecked()))) {
			h = hash_into(h, static_cast<const typename [:std::meta::type_of(b):]&>(value));
		}
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			h = hash_into(h, value.[:m:]);
		}
		return h;
	}
}

template <typename... Ts>
constexpr auto gse::hash_of(const Ts&... values) -> std::uint64_t {
	std::uint64_t seed = 0;
	((seed = hash_into(seed, values)), ...);
	return seed;
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

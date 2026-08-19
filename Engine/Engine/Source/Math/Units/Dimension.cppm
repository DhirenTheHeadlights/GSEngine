export module gse.math:dimension;

import std;

export namespace gse::internal {
	template <typename T>
	concept is_ratio = requires {
		{ T::num } -> std::convertible_to<std::intmax_t>;
		{ T::den } -> std::convertible_to<std::intmax_t>; };

	struct rational {
		std::intmax_t num = 0;
		std::intmax_t den = 1;

		constexpr auto operator==(
			const rational&
		) const -> bool = default;
	};

	struct dimension_exponents {
		rational length{};
		rational time{};
		rational mass{};
		rational angle{};
		rational information{};

		constexpr auto operator==(
			const dimension_exponents&
		) const -> bool = default;
	};

	template <dimension_exponents E>
	struct dim {
		static constexpr dimension_exponents exponents = E;
	};

	template <typename D>
	concept is_dimension = requires {
		{ D::exponents } -> std::convertible_to<dimension_exponents>; };

	using dimensionless = dim<dimension_exponents{}>;

	template <typename Anchor = void>
	consteval auto axis_count() -> std::size_t;

	consteval auto axis_name(
		std::size_t index
	) -> std::string_view;

	consteval auto normalized(
		rational value
	) -> rational;

	consteval auto exponent_at(
		dimension_exponents exponents,
		std::size_t index
	) -> rational;

	consteval auto added(
		dimension_exponents lhs,
		dimension_exponents rhs
	) -> dimension_exponents;

	consteval auto subtracted(
		dimension_exponents lhs,
		dimension_exponents rhs
	) -> dimension_exponents;

	consteval auto halved(
		dimension_exponents exponents
	) -> dimension_exponents;

	template <int... Exponents>
	consteval auto positional_exponents() -> dimension_exponents;

	template <int... Exponents>
	using dimi = dim<positional_exponents<Exponents...>()>;

	template <is_dimension D1, is_dimension D2>
	constexpr auto operator*(
		D1,
		D2
	) -> dim<added(D1::exponents, D2::exponents)>;

	template <is_dimension D1, is_dimension D2>
	constexpr auto operator/(
		D1,
		D2
	) -> dim<subtracted(D1::exponents, D2::exponents)>;

	template <is_dimension D>
	constexpr auto dim_sqrt(
		D
	) -> dim<halved(D::exponents)>;

	template <typename D1, typename D2>
	concept has_same_dimensions = is_dimension<D1> && is_dimension<D2> && D1::exponents == D2::exponents;

	template <std::size_t Axis>
	consteval auto mismatch_message() -> std::string_view;

	template <std::size_t Axis, std::intmax_t LhsNum, std::intmax_t LhsDen, std::intmax_t RhsNum, std::intmax_t RhsDen>
	struct unit_mismatch {
		static_assert(
			LhsNum * RhsDen == RhsNum * LhsDen,
			mismatch_message<Axis>()
		);
	};

	template <is_dimension Lhs, is_dimension Rhs, typename Indices>
	struct dimension_mismatch_checks;

	template <is_dimension Lhs, is_dimension Rhs, std::size_t... Axes>
	struct dimension_mismatch_checks<Lhs, Rhs, std::index_sequence<Axes...>> : unit_mismatch<
																				  Axes,
																				  exponent_at(Lhs::exponents, Axes).num,
																				  exponent_at(Lhs::exponents, Axes).den,
																				  exponent_at(Rhs::exponents, Axes).num,
																				  exponent_at(Rhs::exponents, Axes).den>... {
	};

	template <is_dimension Lhs, is_dimension Rhs>
	struct dimension_mismatch_diagnostic : dimension_mismatch_checks<Lhs, Rhs, std::make_index_sequence<axis_count<Lhs>()>> {
	};
}

template <typename Anchor>
consteval auto gse::internal::axis_count() -> std::size_t {
	return std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked())).size();
}

consteval auto gse::internal::axis_name(const std::size_t index) -> std::string_view {
	std::string_view result;
	std::size_t current = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked()))) {
		if (current == index) {
			result = std::string_view(std::define_static_string(std::meta::identifier_of(m)));
		}
		++current;
	}
	return result;
}

consteval auto gse::internal::normalized(const rational value) -> rational {
	if (value.num == 0) {
		return { .num = 0, .den = 1 };
	}
	const auto divisor = std::gcd(value.num, value.den);
	const std::intmax_t sign = value.den < 0 ? -1 : 1;
	return { .num = sign * value.num / divisor, .den = sign * value.den / divisor };
}

consteval auto gse::internal::exponent_at(const dimension_exponents exponents, const std::size_t index) -> rational {
	rational result{};
	std::size_t current = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked()))) {
		if (current == index) {
			result = exponents.[:m:];
		}
		++current;
	}
	return result;
}

consteval auto gse::internal::added(const dimension_exponents lhs, const dimension_exponents rhs) -> dimension_exponents {
	dimension_exponents result{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked()))) {
		const auto a = lhs.[:m:];
		const auto b = rhs.[:m:];
		result.[:m:] = normalized({ .num = a.num * b.den + b.num * a.den, .den = a.den * b.den });
	}
	return result;
}

consteval auto gse::internal::subtracted(const dimension_exponents lhs, const dimension_exponents rhs) -> dimension_exponents {
	dimension_exponents result{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked()))) {
		const auto a = lhs.[:m:];
		const auto b = rhs.[:m:];
		result.[:m:] = normalized({ .num = a.num * b.den - b.num * a.den, .den = a.den * b.den });
	}
	return result;
}

consteval auto gse::internal::halved(const dimension_exponents exponents) -> dimension_exponents {
	dimension_exponents result{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked()))) {
		const auto a = exponents.[:m:];
		result.[:m:] = normalized({ .num = a.num, .den = a.den * 2 });
	}
	return result;
}

template <int... Exponents>
consteval auto gse::internal::positional_exponents() -> dimension_exponents {
	static_assert(sizeof...(Exponents) <= axis_count<>(), "more positional exponents than dimension axes");

	constexpr std::array<int, sizeof...(Exponents)> values{ Exponents... };
	dimension_exponents result{};
	std::size_t index = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^dimension_exponents, std::meta::access_context::unchecked()))) {
		if (index < values.size()) {
			result.[:m:] = normalized({ .num = values[index] });
		}
		++index;
	}
	return result;
}

template <gse::internal::is_dimension D1, gse::internal::is_dimension D2>
constexpr auto gse::internal::operator*(D1, D2) -> dim<added(D1::exponents, D2::exponents)> {
	return {};
}

template <gse::internal::is_dimension D1, gse::internal::is_dimension D2>
constexpr auto gse::internal::operator/(D1, D2) -> dim<subtracted(D1::exponents, D2::exponents)> {
	return {};
}

template <gse::internal::is_dimension D>
constexpr auto gse::internal::dim_sqrt(D) -> dim<halved(D::exponents)> {
	return {};
}

template <std::size_t Axis>
consteval auto gse::internal::mismatch_message() -> std::string_view {
	return std::string_view(
		std::define_static_string(
			"UNIT MISMATCH [" + std::string(axis_name(Axis)) +
			"]: exponents differ - template args are <axis, left_num, left_den, right_num, right_den>"
		)
	);
}

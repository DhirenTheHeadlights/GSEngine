export module gse.math:dimension;

import std;

export namespace gse::internal {
	template <typename T>
	concept is_ratio = requires {
		{ T::num } -> std::convertible_to<std::intmax_t>;
		{ T::den } -> std::convertible_to<std::intmax_t>;
	};

	template <is_ratio L, is_ratio T, is_ratio M, is_ratio A = std::ratio<0>>
	struct dim {
		using length = L;
		using time = T;
		using mass = M;
		using angle = A;
	};

	template <typename D>
	concept is_dimension = requires {
		typename D::length;
		typename D::time;
		typename D::mass;
		typename D::angle;
		requires is_ratio<typename D::length>;
		requires is_ratio<typename D::time>;
		requires is_ratio<typename D::mass>;
		requires is_ratio<typename D::angle>;
	};

	template <is_dimension D1, is_dimension D2>
	constexpr auto operator*(D1, D2) -> dim<
		std::ratio_add<typename D1::length, typename D2::length>,
		std::ratio_add<typename D1::time, typename D2::time>,
		std::ratio_add<typename D1::mass, typename D2::mass>,
		std::ratio_add<typename D1::angle, typename D2::angle>
	> { return {}; }

	template <is_dimension D1, is_dimension D2>
	constexpr auto operator/(D1, D2) -> dim<
		std::ratio_subtract<typename D1::length, typename D2::length>,
		std::ratio_subtract<typename D1::time, typename D2::time>,
		std::ratio_subtract<typename D1::mass, typename D2::mass>,
		std::ratio_subtract<typename D1::angle, typename D2::angle>
	> { return {}; }

	template<typename D1, typename D2>
	concept has_same_dimensions =
		is_dimension<D1> && is_dimension<D2>
		&& std::ratio_equal_v<typename D1::length, typename D2::length>
		&& std::ratio_equal_v<typename D1::time, typename D2::time>
		&& std::ratio_equal_v<typename D1::mass, typename D2::mass>
		&& std::ratio_equal_v<typename D1::angle, typename D2::angle>;

	template <std::intmax_t LhsNum, std::intmax_t LhsDen, std::intmax_t RhsNum, std::intmax_t RhsDen>
	struct unit_mismatch_length {
		static_assert(LhsNum * RhsDen == RhsNum * LhsDen,
			"UNIT MISMATCH [L]: length exponents differ — args are <left_num, left_den, right_num, right_den>");
	};

	template <std::intmax_t LhsNum, std::intmax_t LhsDen, std::intmax_t RhsNum, std::intmax_t RhsDen>
	struct unit_mismatch_time {
		static_assert(LhsNum * RhsDen == RhsNum * LhsDen,
			"UNIT MISMATCH [T]: time exponents differ — args are <left_num, left_den, right_num, right_den>");
	};

	template <std::intmax_t LhsNum, std::intmax_t LhsDen, std::intmax_t RhsNum, std::intmax_t RhsDen>
	struct unit_mismatch_mass {
		static_assert(LhsNum * RhsDen == RhsNum * LhsDen,
			"UNIT MISMATCH [M]: mass exponents differ — args are <left_num, left_den, right_num, right_den>");
	};

	template <std::intmax_t LhsNum, std::intmax_t LhsDen, std::intmax_t RhsNum, std::intmax_t RhsDen>
	struct unit_mismatch_angle {
		static_assert(LhsNum * RhsDen == RhsNum * LhsDen,
			"UNIT MISMATCH [A]: angle exponents differ — args are <left_num, left_den, right_num, right_den>");
	};

	template <is_dimension Lhs, is_dimension Rhs>
	struct dimension_mismatch_diagnostic {
		unit_mismatch_length<Lhs::length::num, Lhs::length::den, Rhs::length::num, Rhs::length::den> l;
		unit_mismatch_time<Lhs::time::num, Lhs::time::den, Rhs::time::num, Rhs::time::den> t;
		unit_mismatch_mass<Lhs::mass::num, Lhs::mass::den, Rhs::mass::num, Rhs::mass::den> m;
		unit_mismatch_angle<Lhs::angle::num, Lhs::angle::den, Rhs::angle::num, Rhs::angle::den> a;
	};

	template <is_dimension D>
	constexpr auto dim_sqrt(D) -> dim<
		std::ratio_divide<typename D::length, std::ratio<2>>,
		std::ratio_divide<typename D::time, std::ratio<2>>,
		std::ratio_divide<typename D::mass, std::ratio<2>>,
		std::ratio_divide<typename D::angle, std::ratio<2>>
	> { return {}; }

	template <int L, int T, int M, int A = 0>
	using dimi = dim<std::ratio<L>, std::ratio<T>, std::ratio<M>, std::ratio<A>>;

	using dimensionless = dimi<0, 0, 0, 0>;
}

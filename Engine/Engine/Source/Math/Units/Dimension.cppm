export module gse.math:dimension;

import std;

export namespace gse::internal {
	template <int L, int T, int M>
	struct dim {
		static constexpr int length = L;
		static constexpr int time = T;
		static constexpr int mass = M;
	};

	template <typename D>
	concept is_dimension = requires {
		{ D::length } -> std::convertible_to<int>;
		{ D::time } -> std::convertible_to<int>;
		{ D::mass } -> std::convertible_to<int>;
	};

	template <is_dimension D1, is_dimension D2>
	constexpr auto operator*(D1, D2) -> auto {
		return dim<
			D1::length + D2::length,
			D1::time + D2::time,
			D1::mass + D2::mass
		>{};
	}

	template <is_dimension D1, is_dimension D2>
	constexpr auto operator/(D1, D2) -> auto {
		return dim<
			D1::length - D2::length,
			D1::time - D2::time,
			D1::mass - D2::mass
		>{};
	}

	template<typename D1, typename D2>
	concept has_same_dimensions =
		is_dimension<D1> && is_dimension<D2>
		&& D1::length == D2::length
		&& D1::time == D2::time
		&& D1::mass == D2::mass;

	template <is_dimension D>
		requires (D::length % 2 == 0 && D::time % 2 == 0 && D::mass % 2 == 0)
	constexpr auto dim_sqrt(D) -> auto {
		return dim<D::length / 2, D::time / 2, D::mass / 2>{};
	}

	using dimensionless = dim<0, 0, 0>;
}
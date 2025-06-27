export module gse.physics.math:dimension;

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

	template <typename D1, typename D2> requires has_same_dimensions<D1, D2>
	constexpr auto operator+(D1, D2) -> auto {
		return dim <
			D1::length,
			D1::time,
			D1::mass
		>{};
	}

	template <typename D1, typename D2> requires has_same_dimensions<D1, D2>
	constexpr auto operator-(D1, D2) -> auto {
		return dim <
			D1::length,
			D1::time,
			D1::mass
		>{};
	}
}
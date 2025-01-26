export module gse.physics.math.mat;

import std;

import gse.physics.math.unit_vec;

namespace gse {
	template <typename T, int N, int M>
	struct mat {
		std::array<unitless_vec_t<T, N>, M> data;

		constexpr mat() = default;
		constexpr mat(const std::array<unitless_vec_t<T, N>, M>& data) : data(data) {}
		constexpr mat(std::initializer_list<unitless_vec_t<T, N>> list);

		constexpr auto operator[](size_t index) -> unitless_vec_t<T, N>&;
		constexpr auto operator[](size_t index) const -> const unitless_vec_t<T, N>&;

		constexpr auto operator==(const mat& other) const -> bool;
		constexpr auto operator!=(const mat& other) const -> bool;

		constexpr auto operator+(const mat& other) const -> mat;
		constexpr auto operator-(const mat& other) const -> mat;
		constexpr auto operator*(const mat& other) const -> mat;
		constexpr auto operator*(const unitless_vec_t<T, N>& vec) const -> unitless_vec_t<T, N>;
		constexpr auto operator*(T scalar) const -> mat;
		constexpr auto operator/(T scalar) const -> mat;

		constexpr auto operator+=(const mat& other) -> mat&;
		constexpr auto operator-=(const mat& other) -> mat&;
		constexpr auto operator*=(T scalar) -> mat&;
		constexpr auto operator/=(T scalar) -> mat&;

		constexpr auto operator-() const -> mat;

		constexpr auto transpose() const -> mat;
		constexpr auto inverse() const -> mat;
		constexpr auto determinant() const -> T;
		constexpr auto trace() const -> T;
	};
}

export namespace gse {
	template <typename T, int N, int M> using mat_t = mat<T, N, M>;
	template <typename T> using mat2_t = mat<T, 2, 2>;
	template <typename T> using mat3_t = mat<T, 3, 3>;
	template <typename T> using mat4_t = mat<T, 4, 4>;

	using mat2i = mat<int, 2, 2>;
	using mat3i = mat<int, 3, 3>;
	using mat4i = mat<int, 4, 4>;

	using mat2 = mat<float, 2, 2>;
	using mat3 = mat<float, 3, 3>;
	using mat4 = mat<float, 4, 4>;

	using mat2d = mat<double, 2, 2>;
	using mat3d = mat<double, 3, 3>;
	using mat4d = mat<double, 4, 4>;
}
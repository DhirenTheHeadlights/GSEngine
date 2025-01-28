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

template <typename T, int N, int M>
constexpr gse::mat<T, N, M>::mat(std::initializer_list<unitless_vec_t<T, N>> list) {
	size_t i = 0;
	for (const constexpr auto& vec : list) {
		data[i++] = vec;
	}
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator[](size_t index) -> unitless_vec_t<T, N>& {
	return data[index];
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator[](size_t index) const -> const unitless_vec_t<T, N>& {
	return data[index];
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator==(const mat& other) const -> bool {
	return data == other.data;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator!=(const mat& other) const -> bool {
	return !(*this == other);
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator+(const mat& other) const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		result[i] = data[i] + other[i];
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator-(const mat& other) const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		result[i] = data[i] - other[i];
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator*(const mat& other) const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		for (size_t j = 0; j < N; ++j) {
			result[i][j] = data[i] * other[j];
		}
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator*(const unitless_vec_t<T, N>& vec) const -> unitless_vec_t<T, N> {
	unitless_vec_t<T, M> result;
	for (size_t i = 0; i < M; ++i) {
		result[i] = data[i] * vec;
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator*(T scalar) const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		result[i] = data[i] * scalar;
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator/(T scalar) const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		result[i] = data[i] / scalar;
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator+=(const mat& other) -> mat& {
	for (size_t i = 0; i < M; ++i) {
		data[i] += other[i];
	}
	return *this;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator-=(const mat& other) -> mat& {
	for (size_t i = 0; i < M; ++i) {
		data[i] -= other[i];
	}
	return *this;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator*=(T scalar) -> mat& {
	for (size_t i = 0; i < M; ++i) {
		data[i] *= scalar;
	}
	return *this;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator/=(T scalar) -> mat& {
	for (size_t i = 0; i < M; ++i) {
		data[i] /= scalar;
	}
	return *this;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::operator-() const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		result[i] = -data[i];
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::transpose() const -> mat {
	mat result;
	for (size_t i = 0; i < M; ++i) {
		for (size_t j = 0; j < N; ++j) {
			result[j][i] = data[i][j];
		}
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::inverse() const -> mat {
	mat result;
	T det = determinant();
	if (det == 0) {
		throw std::runtime_error("Matrix is singular and cannot be inverted.");
	}
	for (size_t i = 0; i < M; ++i) {
		for (size_t j = 0; j < N; ++j) {
			result[j][i] = data[i][j] / det;
		}
	}
	return result;
}

template <typename T>
constexpr auto det_2x2(const gse::mat<T, 2, 2>& mat) -> T {
	return mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
}

template <typename T>
constexpr auto det_3x3(const gse::mat<T, 3, 3>& mat) -> T {
	return mat[0][0] * (mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1]) -
		mat[0][1] * (mat[1][0] * mat[2][2] - mat[1][2] * mat[2][0]) +
		mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]);
}

template <typename T>
constexpr auto det_4x4(const gse::mat<T, 4, 4>& mat) -> T {
	return mat[0][0] * (mat[1][1] * (mat[2][2] * mat[3][3] - mat[2][3] * mat[3][2]) -
		mat[1][2] * (mat[2][1] * mat[3][3] - mat[2][3] * mat[3][1]) +
		mat[1][3] * (mat[2][1] * mat[3][2] - mat[2][2] * mat[3][1])) -
		mat[0][1] * (mat[1][0] * (mat[2][2] * mat[3][3] - mat[2][3] * mat[3][2]) -
			mat[1][2] * (mat[2][0] * mat[3][3] - mat[2][3] * mat[3][0]) +
			mat[1][3] * (mat[2][0] * mat[3][2] - mat[2][2] * mat[3][0])) +
		mat[0][2] * (mat[1][0] * (mat[2][1] * mat[3][3] - mat[2][3] * mat[3][1]) -
			mat[1][1] * (mat[2][0] * mat[3][3] - mat[2][3] * mat[3][0]) +
			mat[1][3] * (mat[2][0] * mat[3][1] - mat[2][1] * mat[3][0])) -
		mat[0][3] * (mat[1][0] * (mat[2][1] * mat[3][2] - mat[2][2] * mat[3][1]) -
			mat[1][1] * (mat[2][0] * mat[3][2] - mat[2][2] * mat[3][0]) +
			mat[1][2] * (mat[2][0] * mat[3][1] - mat[2][1] * mat[3][0]));
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::determinant() const -> T {
	if (M != N) {
		throw std::runtime_error("Determinant is only defined for square matrices.");
	}
	if (N == 2) {
		return det_2x2(*this);
	}
	if (N == 3) {
		return det_3x3(*this);
	}
	if (N == 4) {
		return det_4x4(*this);
	}

	mat mirror = *this;
	T det = static_cast<T>(1);

	for (size_t i = 0; i < N; ++i) {
		std::size_t pivot = i;
		for (size_t j = i + 1; j < N; ++j) {
			if (std::abs(mirror[j][i]) > std::abs(mirror[pivot][i])) {
				pivot = j;
			}
		}

		if (mirror[pivot][i] == static_cast<T>(0)) {
			return static_cast<T>(0);
		}

		if (pivot != i) {
			for (size_t j = 0; j < N; ++j) {
				std::swap(mirror[i][j], mirror[pivot][j]);
			}
			det *= -1;
		}

		det *= mirror[i][i];

		for (size_t j = i + 1; j < N; ++j) {
			T factor = mirror[j][i] / mirror[i][i];
			for (size_t k = i; k < N; ++k) {
				mirror[j][k] -= factor * mirror[i][k];
			}
		}
	}

	return det;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::trace() const -> T {
	T trace = 0;
	for (size_t i = 0; i < std::min(N, M); ++i) {
		trace += data[i][i];
	}
	return trace;
}
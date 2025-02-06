export module gse.physics.math.matrix;

import std;

import gse.physics.math.unitless_vec;
import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.platform.perma_assert;

namespace gse {
	template <typename T, int Rows, int Cols>
	struct mat {
		using value_type = T;

		std::array<unitless::vec_t<T, Rows>, Cols> data;

		constexpr mat() = default;
		constexpr mat(const T& value);
		constexpr mat(const std::array<unitless::vec_t<T, Rows>, Cols>& data) : data(data) {}
		constexpr mat(std::initializer_list<unitless::vec_t<T, Rows>> list);

		template <int O, int Q>
		constexpr mat(const mat<T, O, Q>& other);

		constexpr auto operator[](size_t index) -> unitless::vec_t<T, Rows>&;
		constexpr auto operator[](size_t index) const -> const unitless::vec_t<T, Rows>&;

		constexpr auto operator==(const mat& other) const -> bool;
		constexpr auto operator!=(const mat& other) const -> bool;

		constexpr auto operator+(const mat& other) const -> mat;
		constexpr auto operator-(const mat& other) const -> mat;
		constexpr auto operator*(const mat& other) const -> mat;
		constexpr auto operator*(const unitless::vec_t<T, Cols>& vec) const -> unitless::vec_t<T, Rows>;
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
	template <typename T, int Rows, int Cols> using mat_t = mat<T, Rows, Cols>;
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

template <typename T, int Rows, int Cols>
constexpr gse::mat<T, Rows, Cols>::mat(const T& value) : data{} {
	const int n = (Rows < Cols ? Rows : Cols);
	for (int j = 0; j < Cols; ++j) {
		if (j < n) {
			data[j][j] = value;
		}
		else {
			data[j][j] = static_cast<T>(0);
		}
	}
}

template <typename T, int Rows, int Cols>
constexpr gse::mat<T, Rows, Cols>::mat(std::initializer_list<unitless::vec_t<T, Rows>> list) : data{} {
	auto it = list.begin();
	for (int j = 0; j < Cols && it != list.end(); ++j, ++it) {
		data[j] = *it;
	}
}

template <typename T, int Rows, int Cols>
template <int SourceRows, int SourceCols>
constexpr gse::mat<T, Rows, Cols>::mat(const mat<T, SourceRows, SourceCols>& other) : data{} {
	const int minRows = (Rows < SourceRows ? Rows : SourceRows);
	const int minCols = (Cols < SourceCols ? Cols : SourceCols);
	for (int j = 0; j < minCols; ++j) {
		for (int i = 0; i < minRows; ++i) {
			data[j][i] = other[j][i];
		}
	}
	for (int j = 0; j < Cols; ++j) {
		for (int i = 0; i < Rows; ++i) {
			if (i >= minRows || j >= minCols) {
				data[j][i] = (i == j ? static_cast<T>(1) : static_cast<T>(0));
			}
		}
	}
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator[](size_t index) -> unitless::vec_t<T, Rows>& {
	return data[index];
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator[](size_t index) const -> const unitless::vec_t<T, Rows>& {
	return data[index];
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator==(const mat& other) const -> bool {
	for (size_t i = 0; i < Cols; ++i) {
		if (data[i] != other[i]) {
			return false;
		}
	}
	return true;
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator!=(const mat& other) const -> bool {
	return !(*this == other);
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator+(const mat& other) const -> mat {
	mat result;
	for (size_t i = 0; i < Cols; ++i) {
		result[i] = data[i] + other[i];
	}
	return result;
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator-(const mat& other) const -> mat {
	mat result;
	for (size_t i = 0; i < Cols; ++i) {
		result[i] = data[i] - other[i];
	}
	return result;
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator*(const mat& other) const -> mat {
	perma_assert(Rows == other.data.size(), "Matrix dimensions must be compatible for multiplication.");

	mat result;
	mat<T, Cols, Rows> other_transposed = other.transpose();

	for (size_t i = 0; i < Cols; ++i) {
		for (size_t j = 0; j < Rows; ++j) {
			result[i][j] = dot(data[i], other_transposed[j]);
		}
	}

	return result;
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator*(const unitless::vec_t<T, Cols>& vec) const -> unitless::vec_t<T, Rows> {
	unitless::vec_t<T, Rows> result;
	for (int j = 0; j < Cols; ++j) {
		result = result + data[j] * vec[j];
	}
	return result;
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator*(T scalar) const -> mat {
	mat result;
	for (size_t i = 0; i < Cols; ++i) {
		result[i] = data[i] * scalar;
	}
	return result;
}

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::operator/(T scalar) const -> mat {
	mat result;
	for (size_t i = 0; i < Cols; ++i) {
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

template <typename T, int Rows, int Cols>
constexpr auto gse::mat<T, Rows, Cols>::transpose() const -> mat {
	mat result;
	for (size_t i = 0; i < Cols; ++i) {
		for (size_t j = 0; j < Rows; ++j) {
			result[i][j] = data[j][i];
		}
	}
	return result;
}

template <typename T, int N, int M>
constexpr auto gse::mat<T, N, M>::inverse() const -> mat {
	mat result;
	T det = determinant();
	perma_assert(det != 0, "Matrix is singular and cannot be inverted.");
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
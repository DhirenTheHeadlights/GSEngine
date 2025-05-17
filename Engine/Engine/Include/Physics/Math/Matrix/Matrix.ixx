export module gse.physics.math.matrix;

import std;

import gse.physics.math.base_vec;
import gse.physics.math.unitless_vec;
import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.platform.perma_assert;

namespace gse {
    template <typename T, int Rows>
	using col_storage = std::conditional_t<Rows == 3, vec::storage<T, 4>, vec::storage<T, Rows>>;

	template <typename T, int Cols, int Rows>
	struct mat {
		using value_type = T;

		std::array<col_storage<T, Rows>, Cols> data;

		constexpr mat() = default;
		constexpr mat(const T& value);
		constexpr mat(const std::array<vec::storage<T, Rows>, Cols>& data) : data(data) {}
        constexpr mat(std::initializer_list<unitless::vec_t<T, Rows>> list);

		template <int O, int Q>
		constexpr mat(const mat<T, O, Q>& other);

		constexpr auto operator[](size_t index) -> auto&;
		constexpr auto operator[](size_t index) const -> const auto&;

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

		constexpr auto transpose() const -> mat<T, Rows, Cols>;
		constexpr auto inverse() const -> mat;
		constexpr auto determinant() const -> T;
		constexpr auto trace() const -> T;
	};
}

export namespace gse {
	template <typename T, int Cols, int Rows> using mat_t = mat<T, Cols, Rows>;
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

template <typename T, int Cols, int Rows>
constexpr gse::mat<T, Cols, Rows>::mat(const T& value) : data{} {
    const int n = Rows < Cols ? Rows : Cols;
    for (int j = 0; j < Cols; ++j) {
        if (j < n) {
            data[j][j] = value;
        }
        else {
            data[j][j] = static_cast<T>(0);
        }
    }
}

template <typename T, int Cols, int Rows>
constexpr gse::mat<T, Cols, Rows>::mat(std::initializer_list<unitless::vec_t<T, Rows>> list) : data{} {
	auto it = list.begin();
	for (int j = 0; j < Cols && it != list.end(); ++j, ++it) {
        data[j] = it->storage;
	}
}

template <typename T, int Cols, int Rows>
template <int SourceCols, int SourceRows>
constexpr gse::mat<T, Cols, Rows>::mat(const mat<T, SourceCols, SourceRows>& other) : data{} {
    const int min_rows = Rows < SourceRows ? Rows : SourceRows;
    const int min_cols = Cols < SourceCols ? Cols : SourceCols;
    for (int j = 0; j < min_cols; ++j) {
        for (int i = 0; i < min_rows; ++i) {
            data[j][i] = other[j][i];
        }
    }
    for (int j = 0; j < Cols; ++j) {
        for (int i = 0; i < Rows; ++i) {
            if (i >= min_rows || j >= min_cols) {
                data[j][i] = i == j ? static_cast<T>(1) : static_cast<T>(0);
            }
        }
    }
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator[](size_t index) -> auto& {
    return data[index];
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator[](size_t index) const -> const auto& {
    return data[index];
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator==(const mat& other) const -> bool {
    for (size_t i = 0; i < Cols; ++i) {
        if (data[i] != other[i]) {
            return false;
        }
    }
    return true;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator!=(const mat& other) const -> bool {
    return !(*this == other);
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator+(const mat& other) const -> mat {
    mat result;
    for (size_t i = 0; i < Cols; ++i) {
        result[i] = data[i] + other[i];
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator-(const mat& other) const -> mat {
    mat result;
    for (size_t i = 0; i < Cols; ++i) {
        result[i] = data[i] - other[i];
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator*(const mat& other) const -> mat {
    mat result;

    for (size_t i = 0; i < Cols; ++i) {
        for (size_t j = 0; j < Rows; ++j) {
            T sum = T(0);
            for (size_t k = 0; k < Cols; ++k) {
                sum += (*this)[k][j] * other[i][k];
            }
            result[i][j] = sum;
        }
    }

    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator*(const unitless::vec_t<T, Cols>& vec) const -> unitless::vec_t<T, Rows> {
    unitless::vec_t<T, Rows> result;
    for (int j = 0; j < Cols; ++j) {
        result = result + data[j] * vec[j];
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator*(T scalar) const -> mat {
    mat result;
    for (size_t i = 0; i < Cols; ++i) {
        result[i] = data[i] * scalar;
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator/(T scalar) const -> mat {
    mat result;
    for (size_t i = 0; i < Cols; ++i) {
        result[i] = data[i] / scalar;
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator+=(const mat& other) -> mat& {
    for (size_t i = 0; i < Cols; ++i) {
        data[i] += other[i];
    }
    return *this;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator-=(const mat& other) -> mat& {
    for (size_t i = 0; i < Cols; ++i) {
        data[i] -= other[i];
    }
    return *this;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator*=(T scalar) -> mat& {
    for (size_t i = 0; i < Cols; ++i) {
        data[i] *= scalar;
    }
    return *this;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator/=(T scalar) -> mat& {
    for (size_t i = 0; i < Cols; ++i) {
        data[i] /= scalar;
    }
    return *this;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator-() const -> mat {
    mat result;
    for (size_t i = 0; i < Cols; ++i) {
        result[i] = -data[i];
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::transpose() const -> mat<T, Rows, Cols> {
    mat<T, Rows, Cols> result;
    for (size_t i = 0; i < Rows; ++i) {
        for (size_t j = 0; j < Cols; ++j) {
            result[i][j] = data[j][i];
        }
    }
    return result;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::inverse() const -> mat {
    mat result;
    T det = determinant();
    perma_assert(det != 0, "Matrix is singular and cannot be inverted.");
    for (size_t i = 0; i < Rows; ++i) {
        for (size_t j = 0; j < Cols; ++j) {
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

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::determinant() const -> T {
    if (Rows != Cols) {
        throw std::runtime_error("Determinant is only defined for square matrices.");
    }
    if (Cols == 2) {
        return det_2x2(*this);
    }
    if (Cols == 3) {
        return det_3x3(*this);
    }
    if (Cols == 4) {
        return det_4x4(*this);
    }

    mat mirror = *this;
    T det = static_cast<T>(1);

    for (size_t i = 0; i < Cols; ++i) {
        std::size_t pivot = i;
        for (size_t j = i + 1; j < Cols; ++j) {
            if (std::abs(mirror[j][i]) > std::abs(mirror[pivot][i])) {
                pivot = j;
            }
        }

        if (mirror[pivot][i] == static_cast<T>(0)) {
            return static_cast<T>(0);
        }

        if (pivot != i) {
            for (size_t j = 0; j < Cols; ++j) {
                std::swap(mirror[i][j], mirror[pivot][j]);
            }
            det *= -1;
        }

        det *= mirror[i][i];

        for (size_t j = i + 1; j < Cols; ++j) {
            T factor = mirror[j][i] / mirror[i][i];
            for (size_t k = i; k < Cols; ++k) {
                mirror[j][k] -= factor * mirror[i][k];
            }
        }
    }

    return det;
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::trace() const -> T {
    T trace = 0;
    for (size_t i = 0; i < std::min(Cols, Rows); ++i) {
        trace += data[i][i];
    }
    return trace;
}
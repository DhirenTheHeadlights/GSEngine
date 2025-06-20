export module gse.physics.math.matrix;

import std;

import gse.physics.math.base_vec;
import gse.physics.math.unitless_vec;
import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.platform.assert;

namespace gse {
	template <typename T, int Cols, int Rows>
	struct mat {
		using value_type = T;

		std::array<vec::storage<T, Rows>, Cols> data;

		constexpr mat() = default;
		constexpr mat(const T& value);
		constexpr mat(const std::array<vec::storage<T, Rows>, Cols>& data) : data(data) {}
        constexpr mat(std::initializer_list<unitless::vec_t<T, Rows>> list);

		template <int O, int Q>
		constexpr mat(const mat<T, O, Q>& other);

		constexpr auto operator[](size_t index) -> vec::storage<T, Rows>&;
		constexpr auto operator[](size_t index) const -> const vec::storage<T, Rows>&;

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

export template <typename T, int N, typename CharT>
struct std::formatter<gse::mat<T, N, N>, CharT> {
    std::formatter<gse::vec::storage<T, N>, CharT> vec_formatter;

    constexpr auto parse(std::format_parse_context& ctx) {
        return vec_formatter.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const gse::mat<T, N, N>& m, FormatContext& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "mat{}x{}[\n", N, N);
        for (int row = 0; row < N; ++row) {
            gse::vec::storage<T, N> row_vec;
            for (int col = 0; col < N; ++col) {
                row_vec[col] = m.data[col][row];
            }
            out = vec_formatter.format(row_vec, ctx);
            if (row != N - 1) out = std::format_to(out, ",\n");
        }
        out = std::format_to(out, "\n]");
        return out;
    }
};

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
constexpr auto gse::mat<T, Cols, Rows>::operator[](size_t index) -> vec::storage<T, Rows>& {
    return data[index];
}

template <typename T, int Cols, int Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator[](size_t index) const -> const vec::storage<T, Rows>& {
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
    vec::storage<T, Rows> result;
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
    const mat& m = *this;

    T coef_00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    T coef_02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
    T coef_03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];
    T coef_04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    T coef_06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
    T coef_07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];
    T coef_08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    T coef_10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
    T coef_11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
    T coef_12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    T coef_14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
    T coef_15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];
    T coef_16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    T coef_18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
    T coef_19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
    T coef_20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
    T coef_22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
    T coef_23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

    vec::storage<T, 4> fac_0 = { coef_00, coef_00, coef_02, coef_03 };
    vec::storage<T, 4> fac_1 = { coef_04, coef_04, coef_06, coef_07 };
    vec::storage<T, 4> fac_2 = { coef_08, coef_08, coef_10, coef_11 };
    vec::storage<T, 4> fac_3 = { coef_12, coef_12, coef_14, coef_15 };
    vec::storage<T, 4> fac_4 = { coef_16, coef_16, coef_18, coef_19 };
    vec::storage<T, 4> fac_5 = { coef_20, coef_20, coef_22, coef_23 };

    vec::storage<T, 4> vec_0 = { m[1][0], m[0][0], m[0][0], m[0][0] };
    vec::storage<T, 4> vec_1 = { m[1][1], m[0][1], m[0][1], m[0][1] };
    vec::storage<T, 4> vec_2 = { m[1][2], m[0][2], m[0][2], m[0][2] };
    vec::storage<T, 4> vec_3 = { m[1][3], m[0][3], m[0][3], m[0][3] };

    vec::storage<T, 4> inv_0 = (vec_1 * fac_0) - (vec_2 * fac_1) + (vec_3 * fac_2);
    vec::storage<T, 4> inv_1 = (vec_0 * fac_0) - (vec_2 * fac_3) + (vec_3 * fac_4);
    vec::storage<T, 4> inv_2 = (vec_0 * fac_1) - (vec_1 * fac_3) + (vec_3 * fac_5);
    vec::storage<T, 4> inv_3 = (vec_0 * fac_2) - (vec_1 * fac_4) + (vec_2 * fac_5);

    vec::storage<T, 4> sign_a = { +1, -1, +1, -1 };
    vec::storage<T, 4> sign_b = { -1, +1, -1, +1 };

    mat<T, 4, 4> inverse_matrix;
    inverse_matrix[0] = inv_0 * sign_a;
    inverse_matrix[1] = inv_1 * sign_b;
    inverse_matrix[2] = inv_2 * sign_a;
    inverse_matrix[3] = inv_3 * sign_b;

    vec::storage<T, 4> row_0 = { inverse_matrix[0][0], inverse_matrix[1][0], inverse_matrix[2][0], inverse_matrix[3][0] };
    vec::storage<T, 4> dot_0 = m[0] * row_0;
    T dot_1 = (dot_0.x + dot_0.y) + (dot_0.z + dot_0.w);

    assert(
        dot_1 != static_cast<T>(0), 
		std::format("Matrix {} is not invertible, determinant is zero.", *this)
    );

    T one_over_determinant = static_cast<T>(1) / dot_1;

    return inverse_matrix * one_over_determinant;
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


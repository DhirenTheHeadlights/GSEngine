export module gse.physics.math:matrix;

import std;

import :vector;
import :quat;

import gse.assert;

namespace gse {
	template <internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
	struct mat {
		using value_type = T;

		std::array<unitless::vec_t<T, Rows>, Cols> data;

		constexpr mat() = default;
		constexpr mat(const T& value);
		constexpr mat(const std::array<unitless::vec_t<T, Rows>, Cols>& data) : data(data) {}
		constexpr mat(std::initializer_list<unitless::vec_t<T, Rows>> list);
		constexpr mat(const quat_t<T>& q);

		template <std::size_t OtherCols, std::size_t OtherRows>
		constexpr mat(const mat<T, OtherCols, OtherRows>& other);

		constexpr auto operator[](std::size_t index) -> unitless::vec_t<T, Rows>&;
		constexpr auto operator[](std::size_t index) const -> const unitless::vec_t<T, Rows>&;

		constexpr auto transpose() const -> mat<T, Rows, Cols>;
		constexpr auto inverse() const -> mat;
		constexpr auto determinant() const -> T;
		constexpr auto trace() const -> T;
	};
}

export template <typename T, std::size_t Cols, std::size_t Rows, typename CharT>
struct std::formatter<gse::mat<T, Cols, Rows>, CharT> {
	std::formatter<gse::unitless::vec_t<T, Cols>, CharT> vec_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return vec_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::mat<T, Cols, Rows>& m, FormatContext& ctx) const {
		auto out = ctx.out();

		out = std::format_to(out, "mat{}x{}[\n", Cols, Rows);

		for (std::size_t row = 0; row < Rows; ++row) {
			gse::unitless::vec_t<T, Cols> row_vec;
			for (std::size_t col = 0; col < Cols; ++col) {
				row_vec[col] = m[col][row];
			}

			out = std::format_to(out, "  ");
			out = vec_formatter.format(row_vec, ctx);

			if (row < Rows - 1) {
				out = std::format_to(out, ",\n");
			}
		}

		out = std::format_to(out, "\n]");
		return out;
	}
};

export namespace gse {
	template <typename T, std::size_t Cols, std::size_t Rows> using mat_t = mat<T, Cols, Rows>;
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

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<T, Cols, Rows>::mat(const T& value) : data{} {
	for (std::size_t i = 0; i < std::min(Cols, Rows); ++i) {
		data[i][i] = value;
	}
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<T, Cols, Rows>::mat(std::initializer_list<unitless::vec_t<T, Rows>> list) {
	auto it = list.begin();
	for (std::size_t j = 0; j < Cols && it != list.end(); ++j, ++it) {
		data[j] = *it;
	}
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<T, Cols, Rows>::mat(const quat_t<T>& q) {
	static_assert(
		(Cols == 3 && Rows == 3) || (Cols == 4 && Rows == 4),
		"Quaternion to matrix constructor is only defined for mat3x3 and mat4x4."
	);

	const T qx2 = q.x() * q.x();
	const T qy2 = q.y() * q.y();
	const T qz2 = q.z() * q.z();
	const T qxy = q.x() * q.y();
	const T qxz = q.x() * q.z();
	const T qyz = q.y() * q.z();
	const T qsx = q.s() * q.x();
	const T qsy = q.s() * q.y();
	const T qsz = q.s() * q.z();

	data[0][0] = T(1) - T(2) * (qy2 + qz2);
	data[0][1] = T(2) * (qxy + qsz);
	data[0][2] = T(2) * (qxz - qsy);

	data[1][0] = T(2) * (qxy - qsz);
	data[1][1] = T(1) - T(2) * (qx2 + qz2);
	data[1][2] = T(2) * (qyz + qsx);

	data[2][0] = T(2) * (qxz + qsy);
	data[2][1] = T(2) * (qyz - qsx);
	data[2][2] = T(1) - T(2) * (qx2 + qy2);

	if constexpr (Cols == 4 && Rows == 4) {
		data[0][3] = T(0);
		data[1][3] = T(0);
		data[2][3] = T(0);
		data[3][0] = T(0);
		data[3][1] = T(0);
		data[3][2] = T(0);
		data[3][3] = T(1);
	}
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
template <std::size_t OtherCols, std::size_t OtherRows>
constexpr gse::mat<T, Cols, Rows>::mat(const mat<T, OtherCols, OtherRows>& other) : data{} {
	for (std::size_t i = 0; i < std::min(Cols, Rows); ++i) {
		data[i][i] = static_cast<T>(1);
	}

	const std::size_t min_cols = std::min(Cols, OtherCols);
	const std::size_t min_rows = std::min(Rows, OtherRows);
	for (std::size_t j = 0; j < min_cols; ++j) {
		for (std::size_t i = 0; i < min_rows; ++i) {
			data[j][i] = other[j][i];
		}
	}
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator[](std::size_t index) -> unitless::vec_t<T, Rows>& {
	return data[index];
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<T, Cols, Rows>::operator[](std::size_t index) const -> const unitless::vec_t<T, Rows>& {
	return data[index];
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<T, Cols, Rows>::transpose() const -> mat<T, Rows, Cols> {
	mat<T, Rows, Cols> result;
	for (std::size_t j = 0; j < Cols; ++j) {
		for (std::size_t i = 0; i < Rows; ++i) {
			result[i][j] = data[j][i];
		}
	}
	return result;
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<T, Cols, Rows>::inverse() const -> mat {
	static_assert(Cols == Rows, "Inverse is only defined for square matrices.");
	const auto& m = *this;

	if constexpr (Cols == 2) {
		const T det = m.determinant();
		assert(det != static_cast<T>(0), "Matrix is not invertible.");
		const T inv_det = static_cast<T>(1) / det;
		return mat<T, 2, 2>({
			unitless::vec_t<T, 2>{ m[1][1] * inv_det, -m[1][0] * inv_det },
			unitless::vec_t<T, 2>{ -m[0][1] * inv_det, m[0][0] * inv_det }
		});
	}
	else if constexpr (Cols == 3) {
		const T det = m.determinant();
		assert(det != static_cast<T>(0), "Matrix is not invertible.");
		const T inv_det = static_cast<T>(1) / det;
		return mat<T, 3, 3>({
				unitless::vec_t<T, 3>{
					(m[1][1] * m[2][2] - m[2][1] * m[1][2])* inv_det,
					(m[2][1] * m[0][2] - m[0][1] * m[2][2])* inv_det,
					(m[0][1] * m[1][2] - m[1][1] * m[0][2])* inv_det
				},
				unitless::vec_t<T, 3>{
					(m[1][2] * m[2][0] - m[1][0] * m[2][2])* inv_det,
					(m[0][0] * m[2][2] - m[2][0] * m[0][2])* inv_det,
					(m[1][0] * m[0][2] - m[0][0] * m[1][2])* inv_det
				},
				unitless::vec_t<T, 3>{
					(m[1][0] * m[2][1] - m[2][0] * m[1][1])* inv_det,
					(m[2][0] * m[0][1] - m[0][0] * m[2][1])* inv_det,
					(m[0][0] * m[1][1] - m[1][0] * m[0][1])* inv_det
				}
			});
	}
	else if constexpr (Cols == 4) {
		const T c00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
		const T c01 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
		const T c02 = m[1][2] * m[2][3] - m[2][2] * m[1][3];
		const T c03 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
		const T c04 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
		const T c05 = m[1][1] * m[2][3] - m[2][1] * m[1][3];
		const T c06 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
		const T c07 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
		const T c08 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
		const T c09 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
		const T c10 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
		const T c11 = m[1][0] * m[2][3] - m[2][0] * m[1][3];
		const T c12 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
		const T c13 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
		const T c14 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
		const T c15 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
		const T c16 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
		const T c17 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

		unitless::vec_t<T, 4> fac0{ m[1][1] * c00 - m[1][2] * c03 + m[1][3] * c06, -(m[0][1] * c00 - m[0][2] * c03 + m[0][3] * c06), m[0][1] * c01 - m[0][2] * c04 + m[0][3] * c07, -(m[0][1] * c02 - m[0][2] * c05 + m[0][3] * c08) };
		unitless::vec_t<T, 4> fac1{ -(m[1][0] * c00 - m[1][2] * c09 + m[1][3] * c12), m[0][0] * c00 - m[0][2] * c09 + m[0][3] * c12, -(m[0][0] * c01 - m[0][2] * c10 + m[0][3] * c13), m[0][0] * c02 - m[0][2] * c11 + m[0][3] * c14 };
		unitless::vec_t<T, 4> fac2{ m[1][0] * c03 - m[1][1] * c09 + m[1][3] * c15, -(m[0][0] * c03 - m[0][1] * c09 + m[0][3] * c15), m[0][0] * c04 - m[0][1] * c10 + m[0][3] * c16, -(m[0][0] * c05 - m[0][1] * c11 + m[0][3] * c17) };
		unitless::vec_t<T, 4> fac3{ -(m[1][0] * c06 - m[1][1] * c12 + m[1][2] * c15), m[0][0] * c06 - m[0][1] * c12 + m[0][2] * c15, -(m[0][0] * c07 - m[0][1] * c13 + m[0][2] * c16), m[0][0] * c08 - m[0][1] * c14 + m[0][2] * c17 };

		const T det = m[0][0] * fac0[0] + m[0][1] * fac1[0] + m[0][2] * fac2[0] + m[0][3] * fac3[0];
		assert(det != static_cast<T>(0), "Matrix is not invertible.");

		const T inv_det = static_cast<T>(1) / det;
		return mat<T, 4, 4>({
			unitless::vec_t<T, 4>{ fac0* inv_det },
			unitless::vec_t<T, 4>{ fac1* inv_det },
			unitless::vec_t<T, 4>{ fac2* inv_det },
			unitless::vec_t<T, 4>{ fac3* inv_det }
		});
	}
	else {
		static_assert(Cols < 2 || Cols > 4, "Inverse is only implemented for 2x2, 3x3, and 4x4 matrices.");
		return {};
	}
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<T, Cols, Rows>::determinant() const -> T {
	static_assert(Rows == Cols, "Determinant is only defined for square matrices.");
	const auto& m = *this;

	if constexpr (Cols == 2) {
		return m[0][0] * m[1][1] - m[1][0] * m[0][1];
	}
	else if constexpr (Cols == 3) {
		return m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) -
			m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2]) +
			m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]);
	}
	else if constexpr (Cols == 4) {
		const T c0 = m[1][1] * (m[2][2] * m[3][3] - m[3][2] * m[2][3]) - m[1][2] * (m[2][1] * m[3][3] - m[3][1] * m[2][3]) + m[1][3] * (m[2][1] * m[3][2] - m[3][1] * m[2][2]);
		const T c1 = m[1][0] * (m[2][2] * m[3][3] - m[3][2] * m[2][3]) - m[1][2] * (m[2][0] * m[3][3] - m[3][0] * m[2][3]) + m[1][3] * (m[2][0] * m[3][2] - m[3][0] * m[2][2]);
		const T c2 = m[1][0] * (m[2][1] * m[3][3] - m[3][1] * m[2][3]) - m[1][1] * (m[2][0] * m[3][3] - m[3][0] * m[2][3]) + m[1][3] * (m[2][0] * m[3][1] - m[3][0] * m[2][1]);
		const T c3 = m[1][0] * (m[2][1] * m[3][2] - m[3][1] * m[2][2]) - m[1][1] * (m[2][0] * m[3][2] - m[3][0] * m[2][2]) + m[1][2] * (m[2][0] * m[3][1] - m[3][0] * m[2][1]);
		return m[0][0] * c0 - m[0][1] * c1 + m[0][2] * c2 - m[0][3] * c3;
	}
	else {
		mat mirror = m;
		T det = static_cast<T>(1);

		for (std::size_t i = 0; i < Cols; ++i) {
			std::size_t pivot = i;
			for (std::size_t j = i + 1; j < Cols; ++j) {
				if (std::abs(mirror[j][i]) > std::abs(mirror[pivot][i])) {
					pivot = j;
				}
			}

			if (mirror[pivot][i] == static_cast<T>(0)) {
				return static_cast<T>(0);
			}

			if (pivot != i) {
				std::swap(mirror.data[i], mirror.data[pivot]);
				det *= -1;
			}

			det *= mirror[i][i];

			for (std::size_t j = i + 1; j < Cols; ++j) {
				const T factor = mirror[j][i] / mirror[i][i];
				for (std::size_t k = i; k < Cols; ++k) {
					mirror[j][k] -= factor * mirror[i][k];
				}
			}
		}
		return det;
	}
}

template <gse::internal::is_arithmetic T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<T, Cols, Rows>::trace() const -> T {
	static_assert(Rows == Cols, "Trace is only defined for square matrices.");
	T trace_val = 0;
	for (std::size_t i = 0; i < Cols; ++i) {
		trace_val += data[i][i];
	}
	return trace_val;
}
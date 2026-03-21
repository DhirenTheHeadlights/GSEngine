export module gse.math:matrix;

import std;

import :vector;
import :quat;

import gse.assert;

export namespace gse {
	template <internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
	struct mat {
		using value_type = internal::vec_storage_type_t<Element>;
		using element_type = Element;
		using col_type = vec::base<Element, Rows>;

		constexpr mat() = default;

		constexpr mat(
			const Element& value
		);

		constexpr mat(
			std::initializer_list<col_type> list
		);

		constexpr mat(
			const quat_t<value_type>& q
		) requires internal::is_arithmetic<Element>;

		template <std::size_t OtherCols, std::size_t OtherRows>
		constexpr mat(
			const mat<Element, OtherCols, OtherRows>& other
		);

		template <internal::is_vec_element OtherE>
			requires (!std::same_as<Element, OtherE> && requires(OtherE e) { static_cast<Element>(e); })
		constexpr mat(
			const mat<OtherE, Cols, Rows>& other
		);

		constexpr decltype(auto) operator[](
			this auto& self,
			std::size_t index
		);

		constexpr auto operator==(const mat&) const -> bool = default;

		constexpr auto transpose(
		) const -> mat<Element, Rows, Cols>;

		constexpr auto inverse(
		) const;

		constexpr auto determinant(
		) const -> value_type;

		constexpr auto trace(
		) const -> value_type;

		template <typename Unit> requires internal::is_arithmetic_wrapper<Element>
		constexpr auto as(
		) const -> mat<value_type, Cols, Rows>;

		template <auto UnitObj> requires (internal::is_arithmetic_wrapper<Element> && internal::is_unit<decltype(UnitObj)>)
		constexpr auto as(
		) const -> mat<value_type, Cols, Rows>;

	private:
		std::array<col_type, Cols> m_data;
	};
}

export template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows, typename CharT>
struct std::formatter<gse::mat<Element, Cols, Rows>, CharT> {
	using VT = gse::internal::vec_storage_type_t<Element>;
	std::formatter<gse::unitless::vec_t<VT, Cols>, CharT> vec_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return vec_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::mat<Element, Cols, Rows>& m, FormatContext& ctx) const {
		auto out = ctx.out();

		out = std::format_to(out, "mat{}x{}[\n", Cols, Rows);

		for (std::size_t row = 0; row < Rows; ++row) {
			gse::unitless::vec_t<VT, Cols> row_vec;
			for (std::size_t col = 0; col < Cols; ++col) {
				row_vec[col] = m[col].as_storage_span()[row];
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
	template <internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
	using mat_t = mat<Element, Cols, Rows>;

	template <internal::is_vec_element Element = float>
	using mat2_t = mat<Element, 2, 2>;

	template <internal::is_vec_element Element = float>
	using mat3_t = mat<Element, 3, 3>;

	template <internal::is_vec_element Element = float>
	using mat4_t = mat<Element, 4, 4>;

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

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr decltype(auto) gse::mat<Element, Cols, Rows>::operator[](this auto& self, std::size_t index) {
	return (self.m_data[index]);
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <auto UnitObj> requires (gse::internal::is_arithmetic_wrapper<Element> && gse::internal::is_unit<decltype(UnitObj)>)
constexpr auto gse::mat<Element, Cols, Rows>::as() const -> mat<value_type, Cols, Rows> {
	return this->template as<decltype(UnitObj)>();
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<Element, Cols, Rows>::mat(const Element& value) : m_data{} {
	for (std::size_t i = 0; i < std::min(Cols, Rows); ++i) {
		m_data[i][i] = value;
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<Element, Cols, Rows>::mat(std::initializer_list<col_type> list) : m_data{} {
	auto it = list.begin();
	for (std::size_t j = 0; j < Cols && it != list.end(); ++j, ++it) {
		m_data[j] = *it;
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<Element, Cols, Rows>::mat(const quat_t<value_type>& q) requires internal::is_arithmetic<Element> {
	static_assert(
		(Cols == 3 && Rows == 3) || (Cols == 4 && Rows == 4),
		"Quaternion to matrix constructor is only defined for mat3x3 and mat4x4."
	);

	const value_type qx2 = q.x() * q.x();
	const value_type qy2 = q.y() * q.y();
	const value_type qz2 = q.z() * q.z();
	const value_type qxy = q.x() * q.y();
	const value_type qxz = q.x() * q.z();
	const value_type qyz = q.y() * q.z();
	const value_type qsx = q.s() * q.x();
	const value_type qsy = q.s() * q.y();
	const value_type qsz = q.s() * q.z();

	m_data[0][0] = value_type(1) - value_type(2) * (qy2 + qz2);
	m_data[0][1] = value_type(2) * (qxy + qsz);
	m_data[0][2] = value_type(2) * (qxz - qsy);

	m_data[1][0] = value_type(2) * (qxy - qsz);
	m_data[1][1] = value_type(1) - value_type(2) * (qx2 + qz2);
	m_data[1][2] = value_type(2) * (qyz + qsx);

	m_data[2][0] = value_type(2) * (qxz + qsy);
	m_data[2][1] = value_type(2) * (qyz - qsx);
	m_data[2][2] = value_type(1) - value_type(2) * (qx2 + qy2);

	if constexpr (Cols == 4 && Rows == 4) {
		m_data[0][3] = value_type(0);
		m_data[1][3] = value_type(0);
		m_data[2][3] = value_type(0);
		m_data[3][0] = value_type(0);
		m_data[3][1] = value_type(0);
		m_data[3][2] = value_type(0);
		m_data[3][3] = value_type(1);
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <std::size_t OtherCols, std::size_t OtherRows>
constexpr gse::mat<Element, Cols, Rows>::mat(const mat<Element, OtherCols, OtherRows>& other) : m_data{} {
	for (std::size_t i = 0; i < std::min(Cols, Rows); ++i) {
		m_data[i].as_storage_span()[i] = static_cast<value_type>(1);
	}

	const std::size_t min_cols = std::min(Cols, OtherCols);
	const std::size_t min_rows = std::min(Rows, OtherRows);
	for (std::size_t j = 0; j < min_cols; ++j) {
		for (std::size_t i = 0; i < min_rows; ++i) {
			m_data[j][i] = other[j][i];
		}
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <gse::internal::is_vec_element OtherE>
	requires (!std::same_as<Element, OtherE> && requires(OtherE e) { static_cast<Element>(e); })
constexpr gse::mat<Element, Cols, Rows>::mat(const mat<OtherE, Cols, Rows>& other) : m_data{} {
	for (std::size_t j = 0; j < Cols; ++j) {
		for (std::size_t i = 0; i < Rows; ++i) {
			m_data[j][i] = static_cast<Element>(other[j][i]);
		}
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Unit> requires gse::internal::is_arithmetic_wrapper<Element>
constexpr auto gse::mat<Element, Cols, Rows>::as() const -> mat<value_type, Cols, Rows> {
	mat<value_type, Cols, Rows> result;
	for (std::size_t c = 0; c < Cols; ++c) {
		for (std::size_t r = 0; r < Rows; ++r) {
			result[c][r] = m_data[c][r].template as<Unit>();
		}
	}
	return result;
}


template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<Element, Cols, Rows>::transpose() const -> mat<Element, Rows, Cols> {
	mat<Element, Rows, Cols> result;
	for (std::size_t j = 0; j < Cols; ++j) {
		for (std::size_t i = 0; i < Rows; ++i) {
			result[i].as_storage_span()[j] = m_data[j].as_storage_span()[i];
		}
	}
	return result;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<Element, Cols, Rows>::inverse() const {
	static_assert(Cols == Rows, "Inverse is only defined for square matrices.");
	using inv_elem = decltype(std::declval<value_type>() / std::declval<Element>());

	auto r = [this](std::size_t c, std::size_t i) -> value_type { return m_data[c].as_storage_span()[i]; };

	if constexpr (Cols == 2) {
		const value_type det = determinant();
		assert(det != value_type(0), std::source_location::current(), "Matrix is not invertible.");
		const value_type inv_det = value_type(1) / det;

		mat<inv_elem, 2, 2> result;
		auto s0 = result[0].as_storage_span();
		auto s1 = result[1].as_storage_span();
		s0[0] = r(1, 1) * inv_det;
		s0[1] = -r(0, 1) * inv_det;
		s1[0] = -r(1, 0) * inv_det;
		s1[1] = r(0, 0) * inv_det;
		return result;
	}
	else if constexpr (Cols == 3) {
		const value_type det = determinant();
		assert(det != value_type(0), std::source_location::current(), "Matrix is not invertible.");
		const value_type inv_det = value_type(1) / det;

		mat<inv_elem, 3, 3> result;
		auto s0 = result[0].as_storage_span();
		auto s1 = result[1].as_storage_span();
		auto s2 = result[2].as_storage_span();
		s0[0] = (r(1, 1) * r(2, 2) - r(2, 1) * r(1, 2)) * inv_det;
		s0[1] = (r(2, 1) * r(0, 2) - r(0, 1) * r(2, 2)) * inv_det;
		s0[2] = (r(0, 1) * r(1, 2) - r(1, 1) * r(0, 2)) * inv_det;
		s1[0] = (r(1, 2) * r(2, 0) - r(1, 0) * r(2, 2)) * inv_det;
		s1[1] = (r(0, 0) * r(2, 2) - r(2, 0) * r(0, 2)) * inv_det;
		s1[2] = (r(1, 0) * r(0, 2) - r(0, 0) * r(1, 2)) * inv_det;
		s2[0] = (r(1, 0) * r(2, 1) - r(2, 0) * r(1, 1)) * inv_det;
		s2[1] = (r(2, 0) * r(0, 1) - r(0, 0) * r(2, 1)) * inv_det;
		s2[2] = (r(0, 0) * r(1, 1) - r(1, 0) * r(0, 1)) * inv_det;
		return result;
	}
	else if constexpr (Cols == 4) {
		const value_type c00 = r(2, 2) * r(3, 3) - r(3, 2) * r(2, 3);
		const value_type c01 = r(1, 2) * r(3, 3) - r(3, 2) * r(1, 3);
		const value_type c02 = r(1, 2) * r(2, 3) - r(2, 2) * r(1, 3);
		const value_type c03 = r(2, 1) * r(3, 3) - r(3, 1) * r(2, 3);
		const value_type c04 = r(1, 1) * r(3, 3) - r(3, 1) * r(1, 3);
		const value_type c05 = r(1, 1) * r(2, 3) - r(2, 1) * r(1, 3);
		const value_type c06 = r(2, 1) * r(3, 2) - r(3, 1) * r(2, 2);
		const value_type c07 = r(1, 1) * r(3, 2) - r(3, 1) * r(1, 2);
		const value_type c08 = r(1, 1) * r(2, 2) - r(2, 1) * r(1, 2);
		const value_type c09 = r(2, 0) * r(3, 3) - r(3, 0) * r(2, 3);
		const value_type c10 = r(1, 0) * r(3, 3) - r(3, 0) * r(1, 3);
		const value_type c11 = r(1, 0) * r(2, 3) - r(2, 0) * r(1, 3);
		const value_type c12 = r(2, 0) * r(3, 2) - r(3, 0) * r(2, 2);
		const value_type c13 = r(1, 0) * r(3, 2) - r(3, 0) * r(1, 2);
		const value_type c14 = r(1, 0) * r(2, 2) - r(2, 0) * r(1, 2);
		const value_type c15 = r(2, 0) * r(3, 1) - r(3, 0) * r(2, 1);
		const value_type c16 = r(1, 0) * r(3, 1) - r(3, 0) * r(1, 1);
		const value_type c17 = r(1, 0) * r(2, 1) - r(2, 0) * r(1, 1);

		const value_type f00 = r(1, 1) * c00 - r(1, 2) * c03 + r(1, 3) * c06;
		const value_type f01 = -(r(0, 1) * c00 - r(0, 2) * c03 + r(0, 3) * c06);
		const value_type f02 = r(0, 1) * c01 - r(0, 2) * c04 + r(0, 3) * c07;
		const value_type f03 = -(r(0, 1) * c02 - r(0, 2) * c05 + r(0, 3) * c08);
		const value_type f10 = -(r(1, 0) * c00 - r(1, 2) * c09 + r(1, 3) * c12);
		const value_type f11 = r(0, 0) * c00 - r(0, 2) * c09 + r(0, 3) * c12;
		const value_type f12 = -(r(0, 0) * c01 - r(0, 2) * c10 + r(0, 3) * c13);
		const value_type f13 = r(0, 0) * c02 - r(0, 2) * c11 + r(0, 3) * c14;
		const value_type f20 = r(1, 0) * c03 - r(1, 1) * c09 + r(1, 3) * c15;
		const value_type f21 = -(r(0, 0) * c03 - r(0, 1) * c09 + r(0, 3) * c15);
		const value_type f22 = r(0, 0) * c04 - r(0, 1) * c10 + r(0, 3) * c16;
		const value_type f23 = -(r(0, 0) * c05 - r(0, 1) * c11 + r(0, 3) * c17);
		const value_type f30 = -(r(1, 0) * c06 - r(1, 1) * c12 + r(1, 2) * c15);
		const value_type f31 = r(0, 0) * c06 - r(0, 1) * c12 + r(0, 2) * c15;
		const value_type f32 = -(r(0, 0) * c07 - r(0, 1) * c13 + r(0, 2) * c16);
		const value_type f33 = r(0, 0) * c08 - r(0, 1) * c14 + r(0, 2) * c17;

		const value_type det = r(0, 0) * f00 + r(0, 1) * f10 + r(0, 2) * f20 + r(0, 3) * f30;
		assert(det != value_type(0), std::source_location::current(), "Matrix is not invertible.");
		const value_type inv_det = value_type(1) / det;

		mat<inv_elem, 4, 4> result;
		auto s0 = result[0].as_storage_span();
		auto s1 = result[1].as_storage_span();
		auto s2 = result[2].as_storage_span();
		auto s3 = result[3].as_storage_span();
		s0[0] = f00 * inv_det; s0[1] = f01 * inv_det;
		s0[2] = f02 * inv_det; s0[3] = f03 * inv_det;
		s1[0] = f10 * inv_det; s1[1] = f11 * inv_det;
		s1[2] = f12 * inv_det; s1[3] = f13 * inv_det;
		s2[0] = f20 * inv_det; s2[1] = f21 * inv_det;
		s2[2] = f22 * inv_det; s2[3] = f23 * inv_det;
		s3[0] = f30 * inv_det; s3[1] = f31 * inv_det;
		s3[2] = f32 * inv_det; s3[3] = f33 * inv_det;
		return result;
	}
	else {
		static_assert(Cols < 2 || Cols > 4, "Inverse is only implemented for 2x2, 3x3, and 4x4 matrices.");
		return mat<inv_elem, Cols, Rows>{};
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<Element, Cols, Rows>::determinant() const -> value_type {
	static_assert(Rows == Cols, "Determinant is only defined for square matrices.");

	auto r = [this](std::size_t c, std::size_t i) -> value_type { return m_data[c].as_storage_span()[i]; };

	if constexpr (Cols == 2) {
		return r(0, 0) * r(1, 1) - r(1, 0) * r(0, 1);
	}
	else if constexpr (Cols == 3) {
		return r(0, 0) * (r(1, 1) * r(2, 2) - r(2, 1) * r(1, 2)) -
			r(1, 0) * (r(0, 1) * r(2, 2) - r(2, 1) * r(0, 2)) +
			r(2, 0) * (r(0, 1) * r(1, 2) - r(1, 1) * r(0, 2));
	}
	else if constexpr (Cols == 4) {
		const value_type c0 = r(1, 1) * (r(2, 2) * r(3, 3) - r(3, 2) * r(2, 3)) - r(1, 2) * (r(2, 1) * r(3, 3) - r(3, 1) * r(2, 3)) + r(1, 3) * (r(2, 1) * r(3, 2) - r(3, 1) * r(2, 2));
		const value_type c1 = r(1, 0) * (r(2, 2) * r(3, 3) - r(3, 2) * r(2, 3)) - r(1, 2) * (r(2, 0) * r(3, 3) - r(3, 0) * r(2, 3)) + r(1, 3) * (r(2, 0) * r(3, 2) - r(3, 0) * r(2, 2));
		const value_type c2 = r(1, 0) * (r(2, 1) * r(3, 3) - r(3, 1) * r(2, 3)) - r(1, 1) * (r(2, 0) * r(3, 3) - r(3, 0) * r(2, 3)) + r(1, 3) * (r(2, 0) * r(3, 1) - r(3, 0) * r(2, 1));
		const value_type c3 = r(1, 0) * (r(2, 1) * r(3, 2) - r(3, 1) * r(2, 2)) - r(1, 1) * (r(2, 0) * r(3, 2) - r(3, 0) * r(2, 2)) + r(1, 2) * (r(2, 0) * r(3, 1) - r(3, 0) * r(2, 1));
		return r(0, 0) * c0 - r(0, 1) * c1 + r(0, 2) * c2 - r(0, 3) * c3;
	}
	else {
		mat<value_type, Cols, Rows> mirror;
		for (std::size_t c = 0; c < Cols; ++c) {
			for (std::size_t ri = 0; ri < Rows; ++ri) {
				mirror[c][ri] = m_data[c].as_storage_span()[ri];
			}
		}
		value_type det = static_cast<value_type>(1);

		for (std::size_t i = 0; i < Cols; ++i) {
			std::size_t pivot = i;
			for (std::size_t j = i + 1; j < Cols; ++j) {
				if (std::abs(mirror[j][i]) > std::abs(mirror[pivot][i])) {
					pivot = j;
				}
			}

			if (mirror[pivot][i] == static_cast<value_type>(0)) {
				return static_cast<value_type>(0);
			}

			if (pivot != i) {
				auto tmp = mirror[i];
				mirror[i] = mirror[pivot];
				mirror[pivot] = tmp;
				det *= -1;
			}

			det *= mirror[i][i];

			for (std::size_t j = i + 1; j < Cols; ++j) {
				const value_type factor = mirror[j][i] / mirror[i][i];
				for (std::size_t k = i; k < Cols; ++k) {
					mirror[j][k] -= factor * mirror[i][k];
				}
			}
		}
		return det;
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<Element, Cols, Rows>::trace() const -> value_type {
	static_assert(Rows == Cols, "Trace is only defined for square matrices.");
	value_type trace_val = 0;
	for (std::size_t i = 0; i < Cols; ++i) {
		trace_val += m_data[i].as_storage_span()[i];
	}
	return trace_val;
}

export module gse.math:matrix;

import std;

import :vector;
import :matrix_base;
import :quat;

export namespace gse::unitless {
	template <typename A, std::size_t Cols, std::size_t Rows> requires std::is_arithmetic_v<A>
	class mat_t : public mat::base<A, Cols, Rows> {
	public:
		using mat::base<A, Cols, Rows>::base;
		constexpr mat_t(const mat::base<A, Cols, Rows>& b) : mat::base<A, Cols, Rows>(b) {}

		constexpr mat_t(const quat_t<A>& q) requires (Cols >= 3 && Rows >= 3) {
			static_assert(
				(Cols == 3 && Rows == 3) || (Cols == 4 && Rows == 4),
				"Quaternion to matrix constructor is only defined for mat3x3 and mat4x4."
			);

			const A qx2 = q.x() * q.x();
			const A qy2 = q.y() * q.y();
			const A qz2 = q.z() * q.z();
			const A qxy = q.x() * q.y();
			const A qxz = q.x() * q.z();
			const A qyz = q.y() * q.z();
			const A qsx = q.s() * q.x();
			const A qsy = q.s() * q.y();
			const A qsz = q.s() * q.z();

			this->data[0][0] = A(1) - A(2) * (qy2 + qz2);
			this->data[0][1] = A(2) * (qxy + qsz);
			this->data[0][2] = A(2) * (qxz - qsy);

			this->data[1][0] = A(2) * (qxy - qsz);
			this->data[1][1] = A(1) - A(2) * (qx2 + qz2);
			this->data[1][2] = A(2) * (qyz + qsx);

			this->data[2][0] = A(2) * (qxz + qsy);
			this->data[2][1] = A(2) * (qyz - qsx);
			this->data[2][2] = A(1) - A(2) * (qx2 + qy2);

			if constexpr (Cols == 4 && Rows == 4) {
				this->data[0][3] = A(0);
				this->data[1][3] = A(0);
				this->data[2][3] = A(0);
				this->data[3][0] = A(0);
				this->data[3][1] = A(0);
				this->data[3][2] = A(0);
				this->data[3][3] = A(1);
			}
		}
	};
}

export namespace gse {
	template <internal::is_arithmetic_wrapper Q, std::size_t Cols, std::size_t Rows>
	class quantity_mat : public mat::base<Q, Cols, Rows> {
	public:
		using mat::base<Q, Cols, Rows>::base;
		constexpr quantity_mat(const mat::base<Q, Cols, Rows>& b) : mat::base<Q, Cols, Rows>(b) {}

		template <typename Unit> requires internal::is_derivable_unit<Unit>
		constexpr auto as() const -> unitless::mat_t<typename Q::value_type, Cols, Rows>;

		template <auto UnitObj> requires internal::is_derivable_unit<decltype(UnitObj)>
		constexpr auto as() const -> unitless::mat_t<typename Q::value_type, Cols, Rows>;
	};
}

template <gse::internal::is_arithmetic_wrapper Q, std::size_t Cols, std::size_t Rows>
template <typename Unit> requires gse::internal::is_derivable_unit<Unit>
constexpr auto gse::quantity_mat<Q, Cols, Rows>::as() const -> unitless::mat_t<typename Q::value_type, Cols, Rows> {
	using storage_type = mat::base<Q, Cols, Rows>::value_type;
	unitless::mat_t<typename Q::value_type, Cols, Rows> result;

	using ratio = Unit::conversion_ratio;
	const storage_type scalar_multiplier = static_cast<storage_type>(ratio::den) / static_cast<storage_type>(ratio::num);

	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(this->data[c].as_storage_span(), scalar_multiplier, result[c].as_storage_span());
	}
	return result;
}

template <gse::internal::is_arithmetic_wrapper Q, std::size_t Cols, std::size_t Rows>
template <auto UnitObj> requires gse::internal::is_derivable_unit<decltype(UnitObj)>
constexpr auto gse::quantity_mat<Q, Cols, Rows>::as() const -> unitless::mat_t<typename Q::value_type, Cols, Rows> {
	return this->template as<decltype(UnitObj)>();
}

export template <typename A, std::size_t Cols, std::size_t Rows, typename CharT>
	requires std::is_arithmetic_v<A>
struct std::formatter<gse::unitless::mat_t<A, Cols, Rows>, CharT> {
	using VT = A;
	std::formatter<gse::unitless::vec_t<VT, Cols>, CharT> vec_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return vec_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::unitless::mat_t<A, Cols, Rows>& m, FormatContext& ctx) const {
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
	template <internal::is_arithmetic_wrapper Element>
	using mat2 = quantity_mat<Element, 2, 2>;

	template <internal::is_arithmetic_wrapper Element>
	using mat3 = quantity_mat<Element, 3, 3>;

	template <internal::is_arithmetic_wrapper Element>
	using mat4 = quantity_mat<Element, 4, 4>;
}

export namespace gse::unitless {
	template <typename T = float> requires std::is_arithmetic_v<T>
	using mat2_t = mat_t<T, 2, 2>;

	template <typename T = float> requires std::is_arithmetic_v<T>
	using mat3_t = mat_t<T, 3, 3>;

	template <typename T = float> requires std::is_arithmetic_v<T>
	using mat4_t = mat_t<T, 4, 4>;

	using mat2i = mat2_t<int>;
	using mat3i = mat3_t<int>;
	using mat4i = mat4_t<int>;

	using mat2 = mat2_t<>;
	using mat3 = mat3_t<>;
	using mat4 = mat4_t<>;

	using mat2d = mat2_t<double>;
	using mat3d = mat3_t<double>;
	using mat4d = mat4_t<double>;
}

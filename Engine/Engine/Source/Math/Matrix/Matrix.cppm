module;

#include <format>

export module gse.math:matrix;

import std;

import :vector;
import :quat;
import :simd;

import gse.assert;

export namespace gse::internal {
	template <typename M>
	concept is_mat_like = requires(const M& m) {
		typename M::element_type;
		typename M::value_type;
		{ M::extent_cols } -> std::convertible_to<std::size_t>;
		{ M::extent_rows } -> std::convertible_to<std::size_t>;
		{ m[0] };
	};
}

export namespace gse {
	template <typename M>
	concept is_mat = internal::is_mat_like<M>;
}

export namespace gse::internal {
	template <is_vec_element T, std::size_t Cols, std::size_t Rows>
	struct mat_storage {
		std::array<vec<T, Rows>, Cols> data{};
		constexpr auto operator<=>(const mat_storage&) const = default;
	};
}

export namespace gse {
	template <internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
	class mat : public internal::mat_storage<Element, Cols, Rows> {
	public:
		using value_type = internal::vec_storage_type_t<Element>;
		using element_type = Element;
		using col_type = vec<Element, Rows>;
		static constexpr std::size_t extent_cols = Cols;
		static constexpr std::size_t extent_rows = Rows;

		constexpr mat(
		) = default;

		constexpr mat(
			const Element& value
		);

		constexpr mat(
			std::initializer_list<col_type> list
		);

		constexpr mat(
			const quat_t<value_type>& q
		) requires (internal::is_arithmetic<Element> && ((Cols == 3 && Rows == 3) || (Cols == 4 && Rows == 4)));

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

		constexpr auto operator==(
			const mat&
		) const -> bool = default;

		constexpr auto transpose(
		) const -> mat<Element, Rows, Cols>;

		constexpr auto inverse(
		) const;

		constexpr auto determinant(
		) const -> value_type;

		constexpr auto trace(
		) const -> value_type;

		template <typename Self, internal::is_vec_element E2, std::size_t C2, std::size_t R2>
		constexpr auto operator+(
			this const Self& self, 
			const mat<E2, C2, R2>& rhs
		) requires (Cols == C2 && Rows == R2 && gse::internal::are_addable<Element, E2>);

		template <typename Self, internal::is_vec_element E2, std::size_t C2, std::size_t R2>
		constexpr auto operator-(
			this const Self& self, 
			const mat<E2, C2, R2>& rhs
		) requires (Cols == C2 && Rows == R2 && gse::internal::are_subtractable<Element, E2>);

		template <typename Self, internal::is_vec_element E2, std::size_t C2, std::size_t R2>
		constexpr auto operator+=(
			this Self& self, 
			const mat<E2, C2, R2>& rhs
		) -> Self& requires (Cols == C2 && Rows == R2 && gse::internal::are_addable<Element, E2> && std::same_as<internal::add_exposed_t<Element, E2>, Element>);

		template <typename Self, internal::is_vec_element E2, std::size_t C2, std::size_t R2>
		constexpr auto operator-=(
			this Self& self,
			const mat<E2, C2, R2>& rhs
		) -> Self& requires (Cols == C2 && Rows == R2 && gse::internal::are_subtractable<Element, E2> && std::same_as<internal::sub_exposed_t<Element, E2>, Element>);

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator*(
			this const Self& self, 
			const S& rhs
		) requires gse::internal::are_multipliable<Element, S>;

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator/(
			this const Self& self, 
			const S& rhs
		) requires gse::internal::are_divisible<Element, S>;

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator*=(
			this Self& self, 
			const S& rhs
		) -> Self& requires (gse::internal::are_multipliable<Element, S> && std::same_as<internal::mul_exposed_t<Element, S>, Element>);

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator/=(
			this Self& self, 
			const S& rhs
		) -> Self& requires (gse::internal::are_divisible<Element, S> && std::same_as<internal::div_exposed_t<Element, S>, Element>);

		template <typename Self>
		constexpr auto operator-(
			this const Self& self
		) -> Self;

		template <typename Self, internal::is_mat_like M2>
			requires (Cols == M2::extent_rows)
		constexpr auto operator*(
			this const Self& self,
			const M2& rhs
		);

		template <typename Self, typename V>
			requires (internal::is_vec_like<V> && V::extent == Cols)
		constexpr auto operator*(
			this const Self& self,
			const V& rhs
		);

		template <typename Self, internal::is_mat_like M2>
			requires (Cols == Rows && Cols == M2::extent_cols && Rows == M2::extent_rows)
		constexpr auto operator*=(
			this Self& self,
			const M2& rhs
		) -> Self&;

		constexpr auto operator<=>(
			const mat&
		) const = default;
	};

	template <internal::is_vec_element S, internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
	constexpr auto operator*(const S& lhs, const mat<E, Cols, Rows>& rhs);
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<Element, Cols, Rows>::mat(const Element& value) : internal::mat_storage<Element, Cols, Rows>{} {
	for (std::size_t i = 0; i < std::min(Cols, Rows); ++i) {
		this->data[i][i] = value;
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<Element, Cols, Rows>::mat(const quat_t<value_type>& q) requires (internal::is_arithmetic<Element> && ((Cols == 3 && Rows == 3) || (Cols == 4 && Rows == 4))) {
	const value_type qx2 = q.x() * q.x();
	const value_type qy2 = q.y() * q.y();
	const value_type qz2 = q.z() * q.z();
	const value_type qxy = q.x() * q.y();
	const value_type qxz = q.x() * q.z();
	const value_type qyz = q.y() * q.z();
	const value_type qsx = q.s() * q.x();
	const value_type qsy = q.s() * q.y();
	const value_type qsz = q.s() * q.z();

	this->data[0][0] = value_type(1) - value_type(2) * (qy2 + qz2);
	this->data[0][1] = value_type(2) * (qxy + qsz);
	this->data[0][2] = value_type(2) * (qxz - qsy);

	this->data[1][0] = value_type(2) * (qxy - qsz);
	this->data[1][1] = value_type(1) - value_type(2) * (qx2 + qz2);
	this->data[1][2] = value_type(2) * (qyz + qsx);

	this->data[2][0] = value_type(2) * (qxz + qsy);
	this->data[2][1] = value_type(2) * (qyz - qsx);
	this->data[2][2] = value_type(1) - value_type(2) * (qx2 + qy2);

	if constexpr (Cols == 4 && Rows == 4) {
		this->data[0][3] = value_type(0);
		this->data[1][3] = value_type(0);
		this->data[2][3] = value_type(0);
		this->data[3][0] = value_type(0);
		this->data[3][1] = value_type(0);
		this->data[3][2] = value_type(0);
		this->data[3][3] = value_type(1);
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr gse::mat<Element, Cols, Rows>::mat(std::initializer_list<col_type> list) : internal::mat_storage<Element, Cols, Rows>{} {
	auto it = list.begin();
	for (std::size_t j = 0; j < Cols && it != list.end(); ++j, ++it) {
		this->data[j] = *it;
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <std::size_t OtherCols, std::size_t OtherRows>
constexpr gse::mat<Element, Cols, Rows>::mat(const mat<Element, OtherCols, OtherRows>& other) : internal::mat_storage<Element, Cols, Rows>{} {
	for (std::size_t i = 0; i < std::min(Cols, Rows); ++i) {
		this->data[i].as_storage_span()[i] = static_cast<value_type>(1);
	}
	const std::size_t min_cols = std::min(Cols, OtherCols);
	const std::size_t min_rows = std::min(Rows, OtherRows);
	for (std::size_t j = 0; j < min_cols; ++j) {
		for (std::size_t i = 0; i < min_rows; ++i) {
			this->data[j][i] = other[j][i];
		}
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <gse::internal::is_vec_element OtherE>
	requires (!std::same_as<Element, OtherE> && requires(OtherE e) { static_cast<Element>(e); })
constexpr gse::mat<Element, Cols, Rows>::mat(const mat<OtherE, Cols, Rows>& other) : internal::mat_storage<Element, Cols, Rows>{} {
	for (std::size_t j = 0; j < Cols; ++j) {
		for (std::size_t i = 0; i < Rows; ++i) {
			this->data[j][i] = static_cast<Element>(other[j][i]);
		}
	}
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr decltype(auto) gse::mat<Element, Cols, Rows>::operator[](this auto& self, std::size_t index) {
	return (self.data[index]);
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<Element, Cols, Rows>::transpose() const -> mat<Element, Rows, Cols> {
	mat<Element, Rows, Cols> result;
	for (std::size_t j = 0; j < Cols; ++j) {
		for (std::size_t i = 0; i < Rows; ++i) {
			result[i].as_storage_span()[j] = this->data[j].as_storage_span()[i];
		}
	}
	return result;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
constexpr auto gse::mat<Element, Cols, Rows>::inverse() const {
	static_assert(Cols == Rows, "Inverse is only defined for square matrices.");
	using inv_elem = decltype(std::declval<value_type>() / std::declval<Element>());

	auto r = [this](std::size_t c, std::size_t i) -> value_type { return this->data[c].as_storage_span()[i]; };

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

	auto r = [this](std::size_t c, std::size_t i) -> value_type { return this->data[c].as_storage_span()[i]; };

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
				mirror[c][ri] = this->data[c].as_storage_span()[ri];
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
		trace_val += this->data[i].as_storage_span()[i];
	}
	return trace_val;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element E2, std::size_t C2, std::size_t R2>
constexpr auto gse::mat<Element, Cols, Rows>::operator+(this const Self& self, const mat<E2, C2, R2>& rhs)
	requires (Cols == C2 && Rows == R2 && gse::internal::are_addable<Element, E2>) {
	using R = internal::add_exposed_t<Element, E2>;
	std::conditional_t<std::same_as<R, Element>, Self, mat<R, Cols, Rows>> out{};
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::add(self.data[c].as_storage_span(), rhs.data[c].as_storage_span(), out.data[c].as_storage_span());
	}
	return out;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element E2, std::size_t C2, std::size_t R2>
constexpr auto gse::mat<Element, Cols, Rows>::operator-(this const Self& self, const mat<E2, C2, R2>& rhs)
	requires (Cols == C2 && Rows == R2 && gse::internal::are_subtractable<Element, E2>) {
	using R = internal::sub_exposed_t<Element, E2>;
	std::conditional_t<std::same_as<R, Element>, Self, mat<R, Cols, Rows>> out{};
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::sub(self.data[c].as_storage_span(), rhs.data[c].as_storage_span(), out.data[c].as_storage_span());
	}
	return out;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element E2, std::size_t C2, std::size_t R2>
constexpr auto gse::mat<Element, Cols, Rows>::operator+=(this Self& self, const mat<E2, C2, R2>& rhs) -> Self&
	requires (Cols == C2 && Rows == R2 && gse::internal::are_addable<Element, E2> && std::same_as<internal::add_exposed_t<Element, E2>, Element>) {
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::add(self.data[c].as_storage_span(), rhs.data[c].as_storage_span(), self.data[c].as_storage_span());
	}
	return self;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element E2, std::size_t C2, std::size_t R2>
constexpr auto gse::mat<Element, Cols, Rows>::operator-=(this Self& self, const mat<E2, C2, R2>& rhs) -> Self&
	requires (Cols == C2 && Rows == R2 && gse::internal::are_subtractable<Element, E2> && std::same_as<internal::sub_exposed_t<Element, E2>, Element>) {
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::sub(self.data[c].as_storage_span(), rhs.data[c].as_storage_span(), self.data[c].as_storage_span());
	}
	return self;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::mat<Element, Cols, Rows>::operator*(this const Self& self, const S& rhs) requires gse::internal::are_multipliable<Element, S> {
	using R = internal::mul_exposed_t<Element, S>;
	std::conditional_t<std::same_as<R, Element>, Self, mat<R, Cols, Rows>> out{};
	const auto scalar = static_cast<value_type>(internal::to_storage(rhs));
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(self.data[c].as_storage_span(), scalar, out.data[c].as_storage_span());
	}
	return out;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::mat<Element, Cols, Rows>::operator/(this const Self& self, const S& rhs) requires gse::internal::are_divisible<Element, S> {
	using R = internal::div_exposed_t<Element, S>;
	std::conditional_t<std::same_as<R, Element>, Self, mat<R, Cols, Rows>> out{};
	const auto scalar = static_cast<value_type>(internal::to_storage(rhs));
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::div_s(self.data[c].as_storage_span(), scalar, out.data[c].as_storage_span());
	}
	return out;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::mat<Element, Cols, Rows>::operator*=(this Self& self, const S& rhs) -> Self&
	requires (gse::internal::are_multipliable<Element, S> && std::same_as<internal::mul_exposed_t<Element, S>, Element>) {
	const auto scalar = static_cast<value_type>(internal::to_storage(rhs));
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(self.data[c].as_storage_span(), scalar, self.data[c].as_storage_span());
	}
	return self;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::mat<Element, Cols, Rows>::operator/=(this Self& self, const S& rhs) -> Self&
	requires (gse::internal::are_divisible<Element, S> && std::same_as<internal::div_exposed_t<Element, S>, Element>) {
	const auto scalar = static_cast<value_type>(internal::to_storage(rhs));
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::div_s(self.data[c].as_storage_span(), scalar, self.data[c].as_storage_span());
	}
	return self;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self>
constexpr auto gse::mat<Element, Cols, Rows>::operator-(this const Self& self) -> Self {
	Self out{};
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(self.data[c].as_storage_span(), static_cast<value_type>(-1), out.data[c].as_storage_span());
	}
	return out;
}

template <gse::internal::is_vec_element S, gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator*(const S& lhs, const mat<E, Cols, Rows>& rhs) {
	return rhs * lhs;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_mat_like M2>
	requires (Cols == M2::extent_rows)
constexpr auto gse::mat<Element, Cols, Rows>::operator*(this const Self& self, const M2& rhs) {
	using E2 = typename M2::element_type;
	using RE = internal::mul_exposed_t<Element, E2>;
	constexpr auto OtherCols = M2::extent_cols;
	mat<RE, OtherCols, Rows> result;

	if constexpr (std::is_same_v<Element, float> && std::is_same_v<E2, float> && Cols == 4 && Rows == 4 && OtherCols == 4) {
		if (!std::is_constant_evaluated()) {
			simd::mul_mat4(self.data[0].as_storage_span().data(), rhs[0].as_storage_span().data(), result.data[0].as_storage_span().data());
			return result;
		}
	}

	for (std::size_t j = 0; j < OtherCols; ++j) {
		auto rj = result.data[j].as_storage_span();
		for (std::size_t i = 0; i < Rows; ++i) {
			typename mat<RE, OtherCols, Rows>::value_type sum{};
			for (std::size_t k = 0; k < Cols; ++k) {
				sum += self.data[k].as_storage_span()[i] * rhs[j].as_storage_span()[k];
			}
			rj[i] = sum;
		}
	}
	return result;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, typename V>
	requires (gse::internal::is_vec_like<V> && V::extent == Cols)
constexpr auto gse::mat<Element, Cols, Rows>::operator*(this const Self& self, const V& rhs) {
	using result_elem = internal::mul_exposed_t<Element, typename V::value_type>;
	vec<result_elem, Rows> result{};
	auto result_span = result.as_storage_span();
	auto rhs_span = rhs.as_storage_span();

	for (std::size_t i = 0; i < Rows; ++i) {
		value_type sum{};
		for (std::size_t j = 0; j < Cols; ++j) {
			sum += self.data[j].as_storage_span()[i] * rhs_span[j];
		}
		result_span[i] = sum;
	}
	return result;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows>
template <typename Self, gse::internal::is_mat_like M2>
	requires (Cols == Rows && Cols == M2::extent_cols && Rows == M2::extent_rows)
constexpr auto gse::mat<Element, Cols, Rows>::operator*=(this Self& self, const M2& rhs) -> Self& {
	self = self * rhs;
	return self;
}

export namespace gse {
	template <internal::is_vec_element Element>
	using mat2 = mat<Element, 2, 2>;

	template <internal::is_vec_element Element>
	using mat3 = mat<Element, 3, 3>;

	template <internal::is_vec_element Element>
	using mat4 = mat<Element, 4, 4>;

	using mat2f = mat2<float>;
	using mat3f = mat3<float>;
	using mat4f = mat4<float>;

	using mat2i = mat2<int>;
	using mat3i = mat3<int>;
	using mat4i = mat4<int>;

	using mat2d = mat2<double>;
	using mat3d = mat3<double>;
	using mat4d = mat4<double>;
}

template <gse::internal::is_vec_element Element, std::size_t Cols, std::size_t Rows, typename CharT>
struct std::formatter<gse::mat<Element, Cols, Rows>, CharT> {
	using VT = gse::internal::vec_storage_type_t<Element>;
	std::formatter<gse::vec<VT, Cols>, CharT> vec_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return vec_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::mat<Element, Cols, Rows>& m, FormatContext& ctx) const {
		auto out = ctx.out();
		out = std::format_to(out, "mat{}x{}[\n", Cols, Rows);
		for (std::size_t row = 0; row < Rows; ++row) {
			gse::vec<VT, Cols> row_vec;
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

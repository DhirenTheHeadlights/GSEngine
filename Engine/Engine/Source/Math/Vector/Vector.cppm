export module gse.math:vector;

import std;

import :simd;
import :vector_base;

export namespace gse::unitless {
	template <typename A, std::size_t N> requires std::is_arithmetic_v<A>
	class vec_t : public vec::base<A, N> {
	public:
		using vec::base<A, N>::base;
	};
}

export namespace gse::internal {
	template <typename T>
	concept is_derivable_unit = requires { typename T::conversion_ratio; };
}

export namespace gse {
	template <internal::is_arithmetic_wrapper Q, std::size_t N>
	class quantity_vec : public vec::base<Q, N> {
	public:
		using vec::base<Q, N>::base;

		template <typename Unit> requires internal::is_derivable_unit<Unit>
		constexpr auto as() const -> unitless::vec_t<typename Q::value_type, N>;

		template <auto TargetUnit> requires internal::is_derivable_unit<decltype(TargetUnit)>
		constexpr auto as() const -> unitless::vec_t<typename Q::value_type, N>;
	};
}

export namespace gse {
	template <typename V>
	concept is_vec = internal::is_vec_like<V>;
}

export template <typename A, std::size_t N, typename CharT>
struct std::formatter<gse::unitless::vec_t<A, N>, CharT> {
	std::formatter<A, CharT> elem_;

	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
		return elem_.parse(ctx);
	}

	template <class FormatContext>
	auto format(const gse::unitless::vec_t<A, N>& v, FormatContext& ctx) const -> FormatContext::iterator {
		auto out = ctx.out();
		out = std::format_to(out, "(");
		for (std::size_t i = 0; i < N; ++i) {
			if (i > 0) {
				out = std::format_to(out, ", ");
			}
			out = elem_.format(v[i], ctx);
		}
		return std::format_to(out, ")");
	}
};

export template <gse::internal::is_arithmetic_wrapper Q, std::size_t N, typename CharT>
struct std::formatter<gse::quantity_vec<Q, N>, CharT> {
	std::formatter<Q, CharT> elem_;

	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
		return elem_.parse(ctx);
	}

	template <class FormatContext>
	auto format(const gse::quantity_vec<Q, N>& v, FormatContext& ctx) const -> FormatContext::iterator {
		auto out = ctx.out();
		out = std::format_to(out, "(");
		for (std::size_t i = 0; i < N; ++i) {
			if (i > 0) {
				out = std::format_to(out, ", ");
			}
			out = elem_.format(v[i], ctx);
		}
		return std::format_to(out, ")");
	}
};

template <gse::internal::is_arithmetic_wrapper Q, std::size_t N >
template <typename Unit> requires gse::internal::is_derivable_unit<Unit>
constexpr auto gse::quantity_vec<Q, N>::as() const -> unitless::vec_t<typename Q::value_type, N> {
	using storage_type = vec::base<Q, N>::storage_type;
	unitless::vec_t<typename Q::value_type, N> result{};

	using ratio = Unit::conversion_ratio;
	const storage_type scalar_multiplier = static_cast<storage_type>(ratio::den) / static_cast<storage_type>(ratio::num);

	simd::mul_s(this->as_storage_span(), scalar_multiplier, result.as_storage_span());

	return result;
}

template <gse::internal::is_arithmetic_wrapper Q, std::size_t N>
template <auto TargetUnit> requires gse::internal::is_derivable_unit<decltype(TargetUnit)>
constexpr auto gse::quantity_vec<Q, N>::as() const -> unitless::vec_t<typename Q::value_type, N> {
	return this->as<decltype(TargetUnit)>();
}

export namespace gse::unitless {
	template <typename T>
	using vec2_t = vec_t<T, 2>;

	template <typename T>
	using vec3_t = vec_t<T, 3>;

	template <typename T>
	using vec4_t = vec_t<T, 4>;

	using vec2u = vec2_t<unsigned int>;
	using vec3u = vec3_t<unsigned int>;
	using vec4u = vec4_t<unsigned int>;

	using vec2i = vec2_t<int>;
	using vec3i = vec3_t<int>;
	using vec4i = vec4_t<int>;

	using vec2 = vec2_t<float>;
	using vec3 = vec3_t<float>;
	using vec4 = vec4_t<float>;

	using vec2d = vec2_t<double>;
	using vec3d = vec3_t<double>;
	using vec4d = vec4_t<double>;

	enum class axis {
		x = 0,
		y = 1,
		z = 2,
		w = 3
	};

	template <typename T>
	auto to_axis_v(axis a) -> vec3_t<T>;

	auto axis_x = to_axis_v<float>(axis::x);
	auto axis_y = to_axis_v<float>(axis::y);
	auto axis_z = to_axis_v<float>(axis::z);
}

template <typename T>
auto gse::unitless::to_axis_v(const axis a) -> vec3_t<T> {
	switch (a) {
		case axis::x: return { 1, 0, 0 };
		case axis::y: return { 0, 1, 0 };
		case axis::z: return { 0, 0, 1 };
		case axis::w: return { 0, 0, 0 };
	}
	return {};
}

export namespace gse {
	template <internal::is_arithmetic_wrapper T, std::size_t N>
	using vec_t = quantity_vec<T, N>;

	template <typename T>
	using vec2 = vec_t<T, 2>;

	template <typename T>
	using vec3 = vec_t<T, 3>;

	template <typename T>
	using vec4 = vec_t<T, 4>;
}

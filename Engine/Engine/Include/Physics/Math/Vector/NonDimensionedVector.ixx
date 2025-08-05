export module gse.physics.math:unitless_vec;

import std;

import :quant;
import :base_vec;

export namespace gse::unitless {
	template <internal::is_arithmetic T, int N>
	struct vec_t : internal::vec_t<vec_t<T, N>, T, N> {
		using internal::vec_t<vec_t, T, N>::vec_t;

		template <typename U>
		constexpr vec_t(const vec_t<U, N>& other) {
			for (int i = 0; i < N; ++i) {
				this->storage[i] = static_cast<T>(other[i]);
			}
		}

		template <typename U>
		constexpr vec_t(const vec::storage<U, N>& other) {
			for (int i = 0; i < N; ++i) {
				this->storage[i] = static_cast<T>(other[i]);
			}
		}

		constexpr auto operator<=>(const vec_t& other) const {
			return this->storage <=> other.storage;
		}
		constexpr auto operator==(const vec_t& other) const -> bool {
			return this->storage == other.storage;
		}
	};

	template <typename T> using vec2_t = vec_t<T, 2>;
	template <typename T> using vec3_t = vec_t<T, 3>;
	template <typename T> using vec4_t = vec_t<T, 4>;

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

	enum struct axis {
		x, y, z, w
	};

	template <typename T>
	auto axis_v(const axis axis) -> vec3_t<T> {
		switch (axis) {
			case axis::x: return vec3_t<T>(1, 0, 0);
			case axis::y: return vec3_t<T>(0, 1, 0);
			case axis::z: return vec3_t<T>(0, 0, 1);
			case axis::w: return vec3_t<T>(0, 0, 0);
		}
		return {};
	}
}

export template <gse::internal::is_arithmetic T, int N, typename CharT>
struct std::formatter<gse::unitless::vec_t<T, N>, CharT> {
	std::formatter<gse::vec::storage<T, N>, CharT> storage_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return storage_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::unitless::vec_t<T, N>& v, FormatContext& ctx) const {
		return storage_formatter.format(v.storage, ctx);
	}
};
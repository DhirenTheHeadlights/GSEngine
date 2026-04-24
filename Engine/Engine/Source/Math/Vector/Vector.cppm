export module gse.math:vector;

import std;

import :simd;

export namespace gse::internal {
	template <typename T>
	concept is_arithmetic_wrapper =
		!std::is_arithmetic_v<T> &&
		requires { typename T::value_type; }&&
	std::is_arithmetic_v<typename T::value_type>&&
		std::is_trivially_copyable_v<T> &&
		sizeof(T) == sizeof(typename T::value_type) &&
		std::is_standard_layout_v<T> &&
		alignof(T) == alignof(typename T::value_type);

	template <typename T>
	concept is_vec_element = std::is_arithmetic_v<T> || is_arithmetic_wrapper<T>;

	template <typename T>
	struct vec_storage_type {
		using type = T;
	};

	template <is_arithmetic_wrapper W>
	struct vec_storage_type<W> {
		using type = W::value_type;
	};

	template <typename T>
	using vec_storage_type_t = vec_storage_type<T>::type;

	template <typename T>
	constexpr auto to_storage(const T& val) -> const vec_storage_type_t<T>& {
		return *reinterpret_cast<const vec_storage_type_t<T>*>(&val);
	}

	template <typename V>
	concept is_vec_like =
		requires(const V & v) {
		typename V::tag;
		typename V::value_type;
		{
			v.as_storage_span()
		};
	};

	template <typename T, std::size_t N, typename Arg, bool IsVecLike>
	struct vec_arg_traits_impl;

	template <typename T, std::size_t N, typename Arg>
	struct vec_arg_traits_impl<T, N, Arg, false> {
		using a = std::remove_reference_t<Arg>;
		static constexpr bool valid = std::is_constructible_v<T, Arg>;
		static constexpr std::size_t count = 1;
	};

	template <typename T, std::size_t N, typename Arg>
	struct vec_arg_traits_impl<T, N, Arg, true> {
		using a = std::remove_reference_t<Arg>;
		static constexpr bool valid =
			std::is_same_v<typename a::value_type, T> &&
			(a::extent <= N);
		static constexpr std::size_t count = a::extent;
	};

	template <typename T, std::size_t N, typename Arg>
	using vec_arg_traits = vec_arg_traits_impl<T, N, Arg, is_vec_like<std::remove_reference_t<Arg>>>;

	template <typename T, std::size_t N, typename... Args>
	concept vec_ctor_compatible =
		(sizeof...(Args) > 0) &&
		(vec_arg_traits<T, N, Args>::valid && ...) &&
		((vec_arg_traits<T, N, Args>::count + ...) == N);
}

export namespace gse::internal {
	template <typename T, std::size_t N>
	struct vec_storage {
		std::array<T, N> data{};
		constexpr auto operator<=>(const vec_storage&) const = default;
	};

	template <typename U, typename V>
	using add_exposed_t = decltype(std::declval<U>() + std::declval<V>());

	template <typename U, typename V>
	using sub_exposed_t = decltype(std::declval<U>() - std::declval<V>());

	template <typename U, typename V>
	using mul_exposed_t = decltype(std::declval<U>() * std::declval<V>());

	template <typename U, typename V>
	using div_exposed_t = decltype(std::declval<U>() / std::declval<V>());

	template <typename U, typename V>
	concept are_addable = requires { typename add_exposed_t<U, V>; };

	template <typename U, typename V>
	concept are_subtractable = requires { typename sub_exposed_t<U, V>; };

	template <typename U, typename V>
	concept are_multipliable = requires { typename mul_exposed_t<U, V>; };

	template <typename U, typename V>
	concept are_divisible = requires { typename div_exposed_t<U, V>; };
}

export namespace gse {
	template <internal::is_vec_element T, std::size_t N>
	class vec : public internal::vec_storage<T, N> {
	public:
		using tag = void;
		using value_type = T;
		using storage_type = internal::vec_storage_type_t<T>;
		static constexpr std::size_t extent = N;

		constexpr vec() = default;
		constexpr vec(const T& value);

		template <typename U>
			requires (!std::same_as<T, U> && std::is_constructible_v<T, const U&> && !internal::is_vec_like<std::remove_cvref_t<U>>)
		constexpr vec(const U& value) { this->data.fill(T(value)); }

		constexpr vec(const T* values);
		constexpr vec(const internal::vec_storage<T, N>& data);
		constexpr vec(const std::array<T, N>& data);

		template <typename V> requires (sizeof(V) < sizeof(T))
		constexpr vec(const internal::vec_storage<V, N>& data);

		template <typename V> requires (sizeof(V) < sizeof(T))
		constexpr vec(const std::array<V, N>& data);

		template <typename... Args>
		constexpr vec(Args&&... args) requires gse::internal::vec_ctor_compatible<T, N, Args...>;

		template <std::size_t M>
		constexpr vec(const vec<T, M>& other) requires (M <= N);

		template <internal::is_vec_element T2>
		constexpr vec(const vec<T2, N>& other);

		constexpr decltype(auto) at(this auto& self, std::size_t index);
		constexpr decltype(auto) operator[](this auto& self, std::size_t index);

		template <typename E> requires std::is_enum_v<E>
		constexpr decltype(auto) operator[](this auto& self, E index);

		constexpr decltype(auto) x(this auto&& self) requires(N > 0);
		constexpr decltype(auto) y(this auto&& self) requires(N > 1);
		constexpr decltype(auto) z(this auto&& self) requires(N > 2);
		constexpr decltype(auto) w(this auto&& self) requires(N > 3);

		constexpr decltype(auto) r(this auto&& self) requires(N > 0);
		constexpr decltype(auto) g(this auto&& self) requires(N > 1);
		constexpr decltype(auto) b(this auto&& self) requires(N > 2);
		constexpr decltype(auto) a(this auto&& self) requires(N > 3);

		auto as_storage_span(this auto&& self);

		constexpr auto begin(this auto&& self);
		constexpr auto end(this auto&& self);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator+(
			this const Self& self,
			const vec<T2, M>& rhs
		) requires (N == M && gse::internal::are_addable<T, T2>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator-(
			this const Self& self,
			const vec<T2, M>& rhs
		) requires (N == M && gse::internal::are_subtractable<T, T2>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator*(
			this const Self& self,
			const vec<T2, M>& rhs
		) requires (N == M && gse::internal::are_multipliable<T, T2>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator/(
			this const Self& self,
			const vec<T2, M>& rhs
		) requires (N == M && gse::internal::are_divisible<T, T2>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator+=(
			this Self& self,
			const vec<T2, M>& rhs
		) -> Self& requires (N == M && gse::internal::are_addable<T, T2> && std::same_as<gse::internal::add_exposed_t<T, T2>, T>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator-=(
			this Self& self,
			const vec<T2, M>& rhs
		) -> Self& requires (N == M && gse::internal::are_subtractable<T, T2> && std::same_as<gse::internal::sub_exposed_t<T, T2>, T>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator*=(
			this Self& self,
			const vec<T2, M>& rhs
		) -> Self& requires (N == M && gse::internal::are_multipliable<T, T2> && std::same_as<gse::internal::mul_exposed_t<T, T2>, T>);

		template <typename Self, internal::is_vec_element T2, std::size_t M>
		constexpr auto operator/=(
			this Self& self,
			const vec<T2, M>& rhs
		) -> Self& requires (N == M && gse::internal::are_divisible<T, T2> && std::same_as<gse::internal::div_exposed_t<T, T2>, T>);

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator*(
			this const Self& self,
			const S& rhs
		) requires gse::internal::are_multipliable<T, S>;

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator/(
			this const Self& self,
			const S& rhs
		) requires gse::internal::are_divisible<T, S>;

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator*=(
			this Self& self,
			const S& rhs
		) -> Self& requires (gse::internal::are_multipliable<T, S> && std::same_as<gse::internal::mul_exposed_t<T, S>, T>);

		template <typename Self, internal::is_vec_element S>
		constexpr auto operator/=(
			this Self& self,
			const S& rhs
		) -> Self& requires (gse::internal::are_divisible<T, S> && std::same_as<gse::internal::div_exposed_t<T, S>, T>);

		template <typename Self>
		constexpr auto operator+(
			this const Self& self
		) -> Self;

		template <typename Self>
		constexpr auto operator-(
			this const Self& self
		) -> Self;

		constexpr auto operator<=>(
			const vec&
		) const = default;

	};

	template <internal::is_vec_element S, typename V>
		requires internal::is_vec_like<V>
	constexpr auto operator*(
		const S& lhs,
		const V& rhs
	);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec<T, N>::vec(const T& value) {
	this->data.fill(value);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec<T, N>::vec(const T* values) {
	std::copy_n(values, N, this->data.begin());
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec<T, N>::vec(const internal::vec_storage<T, N>& data) : internal::vec_storage<T, N>(data) {}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec<T, N>::vec(const std::array<T, N>& data) : internal::vec_storage<T, N>{ data } {}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename V> requires (sizeof(V) < sizeof(T))
	constexpr gse::vec<T, N>::vec(const internal::vec_storage<V, N>& data) {
	for (std::size_t i = 0; i < data.data.size(); ++i) {
		this->data[i] = static_cast<T>(data.data[i]);
	}
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename V> requires (sizeof(V) < sizeof(T))
	constexpr gse::vec<T, N>::vec(const std::array<V, N>& data) {
	for (std::size_t i = 0; i < data.size(); ++i) {
		this->data[i] = static_cast<T>(data[i]);
	}
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename... Args>
constexpr gse::vec<T, N>::vec(Args&&... args)
	requires gse::internal::vec_ctor_compatible<T, N, Args...> {
	std::size_t idx = 0;

	auto append = [this, &idx]<typename T0>(T0&& arg) {
		using a = std::remove_reference_t<T0>;
		if constexpr (gse::internal::is_vec_like<a>) {
			for (std::size_t i = 0; i < a::extent; ++i) {
				this->data[idx++] = static_cast<T>(arg[i]);
			}
		}
		else {
			this->data[idx++] = static_cast<T>(std::forward<T0>(arg));
		}
	};

	(append(std::forward<Args>(args)), ...);
}

template <gse::internal::is_vec_element T, std::size_t N>
template <std::size_t M>
constexpr gse::vec<T, N>::vec(const vec<T, M>& other) requires (M <= N) {
	for (std::size_t i = 0; i < M; ++i) {
		this->data[i] = other.data[i];
	}
	for (std::size_t i = M; i < N; ++i) {
		this->data[i] = T{};
	}
}

template <gse::internal::is_vec_element T, std::size_t N>
template <gse::internal::is_vec_element T2>
constexpr gse::vec<T, N>::vec(const vec<T2, N>& other) {
	for (std::size_t i = 0; i < N; ++i) {
		this->data[i] = static_cast<T>(other[i]);
	}
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::at(this auto& self, std::size_t index) {
	return self.data.at(index);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::operator[](this auto& self, std::size_t index) {
	return self.data[index];
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename E> requires std::is_enum_v<E>
constexpr decltype(auto) gse::vec<T, N>::operator[](this auto& self, E index) {
	return self.at(static_cast<std::size_t>(index));
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::x(this auto&& self) requires(N > 0) {
	return self.at(0);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::y(this auto&& self) requires(N > 1) {
	return self.at(1);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::z(this auto&& self) requires(N > 2) {
	return self.at(2);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::w(this auto&& self) requires(N > 3) {
	return self.at(3);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::r(this auto&& self) requires (N > 0) {
	return self.at(0);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::g(this auto&& self) requires (N > 1) {
	return self.at(1);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::b(this auto&& self) requires (N > 2) {
	return self.at(2);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec<T, N>::a(this auto&& self) requires (N > 3) {
	return self.at(3);
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr auto gse::vec<T, N>::begin(this auto&& self) {
	return self.data.begin();
}

template <gse::internal::is_vec_element T, std::size_t N>
constexpr auto gse::vec<T, N>::end(this auto&& self) {
	return self.data.end();
}

template <gse::internal::is_vec_element T, std::size_t N>
auto gse::vec<T, N>::as_storage_span(this auto&& self) {
	using qualified_storage = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const storage_type, storage_type>;
	return std::span<qualified_storage, N>(reinterpret_cast<qualified_storage*>(self.data.data()), N);
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator+(this const Self& self, const vec<T2, M>& rhs) requires (N == M && gse::internal::are_addable<T, T2>) {
	using R = gse::internal::add_exposed_t<T, T2>;
	std::conditional_t<std::same_as<R, T>, Self, vec<R, N>> out{};
	simd::add(self.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator-(this const Self& self, const vec<T2, M>& rhs) requires (N == M && gse::internal::are_subtractable<T, T2>) {
	using R = gse::internal::sub_exposed_t<T, T2>;
	std::conditional_t<std::same_as<R, T>, Self, vec<R, N>> out{};
	simd::sub(self.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator*(this const Self& self, const vec<T2, M>& rhs) requires (N == M && gse::internal::are_multipliable<T, T2>) {
	using R = gse::internal::mul_exposed_t<T, T2>;
	std::conditional_t<std::same_as<R, T>, Self, vec<R, N>> out{};
	simd::mul(self.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator/(this const Self& self, const vec<T2, M>& rhs) requires (N == M && gse::internal::are_divisible<T, T2>) {
	using R = gse::internal::div_exposed_t<T, T2>;
	std::conditional_t<std::same_as<R, T>, Self, vec<R, N>> out{};
	simd::div(self.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator+=(this Self& self, const vec<T2, M>& rhs) -> Self& requires (N == M && gse::internal::are_addable<T, T2> && std::same_as<gse::internal::add_exposed_t<T, T2>, T>) {
	simd::add(self.as_storage_span(), rhs.as_storage_span(), self.as_storage_span());
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator-=(this Self& self, const vec<T2, M>& rhs) -> Self& requires (N == M && gse::internal::are_subtractable<T, T2> && std::same_as<gse::internal::sub_exposed_t<T, T2>, T>) {
	simd::sub(self.as_storage_span(), rhs.as_storage_span(), self.as_storage_span());
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator*=(this Self& self, const vec<T2, M>& rhs) -> Self& requires (N == M && gse::internal::are_multipliable<T, T2> && std::same_as<gse::internal::mul_exposed_t<T, T2>, T>) {
	simd::mul(self.as_storage_span(), rhs.as_storage_span(), self.as_storage_span());
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element T2, std::size_t M>
constexpr auto gse::vec<T, N>::operator/=(this Self& self, const vec<T2, M>& rhs) -> Self& requires (N == M && gse::internal::are_divisible<T, T2> && std::same_as<gse::internal::div_exposed_t<T, T2>, T>) {
	simd::div(self.as_storage_span(), rhs.as_storage_span(), self.as_storage_span());
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::vec<T, N>::operator*(this const Self& self, const S& rhs) requires gse::internal::are_multipliable<T, S> {
	using R = gse::internal::mul_exposed_t<T, S>;
	std::conditional_t<std::same_as<R, T>, Self, vec<R, N>> out{};
	simd::mul_s(self.as_storage_span(), static_cast<storage_type>(internal::to_storage(rhs)), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::vec<T, N>::operator/(this const Self& self, const S& rhs) requires gse::internal::are_divisible<T, S> {
	using R = gse::internal::div_exposed_t<T, S>;
	std::conditional_t<std::same_as<R, T>, Self, vec<R, N>> out{};
	simd::div_s(self.as_storage_span(), static_cast<storage_type>(internal::to_storage(rhs)), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::vec<T, N>::operator*=(this Self& self, const S& rhs) -> Self& requires (gse::internal::are_multipliable<T, S> && std::same_as<gse::internal::mul_exposed_t<T, S>, T>) {
	simd::mul_s(self.as_storage_span(), static_cast<storage_type>(internal::to_storage(rhs)), self.as_storage_span());
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self, gse::internal::is_vec_element S>
constexpr auto gse::vec<T, N>::operator/=(this Self& self, const S& rhs) -> Self& requires (gse::internal::are_divisible<T, S> && std::same_as<gse::internal::div_exposed_t<T, S>, T>) {
	simd::div_s(self.as_storage_span(), static_cast<storage_type>(internal::to_storage(rhs)), self.as_storage_span());
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self>
constexpr auto gse::vec<T, N>::operator+(this const Self& self) -> Self {
	return self;
}

template <gse::internal::is_vec_element T, std::size_t N>
template <typename Self>
constexpr auto gse::vec<T, N>::operator-(this const Self& self) -> Self {
	Self out{};
	simd::mul_s(self.as_storage_span(), static_cast<storage_type>(-1), out.as_storage_span());
	return out;
}

template <gse::internal::is_vec_element S, typename V>
	requires gse::internal::is_vec_like<V>
constexpr auto gse::operator*(const S& lhs, const V& rhs) {
	return rhs * lhs;
}

export namespace gse {
	template <internal::is_vec_element T>
	using vec2 = vec<T, 2>;

	template <internal::is_vec_element T>
	using vec3 = vec<T, 3>;

	template <internal::is_vec_element T>
	using vec4 = vec<T, 4>;

	using vec2f = vec2<float>;
	using vec3f = vec3<float>;
	using vec4f = vec4<float>;

	using vec2d = vec2<double>;
	using vec3d = vec3<double>;
	using vec4d = vec4<double>;

	using vec2i = vec2<int>;
	using vec3i = vec3<int>;
	using vec4i = vec4<int>;

	using vec2u = vec2<unsigned int>;
	using vec3u = vec3<unsigned int>;
	using vec4u = vec4<unsigned int>;

	template <typename V>
	concept is_vec = internal::is_vec_like<V>;

	enum class axis {
		x = 0,
		y = 1,
		z = 2,
		w = 3
	};

	template <typename T>
	constexpr auto to_axis_v(const axis a) -> vec3<T> {
		switch (a) {
			case axis::x: return { 1, 0, 0 };
			case axis::y: return { 0, 1, 0 };
			case axis::z: return { 0, 0, 1 };
			case axis::w: return { 0, 0, 0 };
		}
		return {};
	}

	constexpr auto axis_x = to_axis_v<float>(axis::x);
	constexpr auto axis_y = to_axis_v<float>(axis::y);
	constexpr auto axis_z = to_axis_v<float>(axis::z);
}

template <gse::internal::is_vec_element T, std::size_t N, typename CharT>
struct std::formatter<gse::vec<T, N>, CharT> {
	std::formatter<T, CharT> elem_;

	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
		return elem_.parse(ctx);
	}

	template <class FormatContext>
	auto format(const gse::vec<T, N>& v, FormatContext& ctx) const -> FormatContext::iterator {
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


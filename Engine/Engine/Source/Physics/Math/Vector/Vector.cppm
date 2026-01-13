export module gse.physics.math:vector;

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
	concept is_derivable_unit = requires { typename T::conversion_ratio; };

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
		static constexpr bool valid = std::is_convertible_v<Arg, T>;
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

export namespace gse::vec {
	template <typename T, std::size_t N>
	struct storage {
		std::array<T, N> data{};
		constexpr auto operator<=>(const storage&) const = default;
	};
}

export namespace gse::vec {
	template <typename Derived, internal::is_vec_element T, std::size_t N>
	class base : public storage<T, N> {
	public:
		using tag = void;
		using value_type = T;
		using storage_type = internal::vec_storage_type_t<T>;
		static constexpr std::size_t extent = N;

		constexpr base() = default;
		constexpr base(const T& value);
		constexpr base(const T* values);
		constexpr base(const storage<T, N>& data);
		constexpr base(const std::array<T, N>& data);

		template <typename V> requires (sizeof(V) < sizeof(T))
		constexpr base(const storage<V, N>& data);

		template <typename V> requires (sizeof(V) < sizeof(T))
		constexpr base(const std::array<V, N>& data);

		template <typename... Args>
		constexpr base(Args&&... args) requires gse::internal::vec_ctor_compatible<T, N, Args...>;

		template <typename D, std::size_t M>
		constexpr base(const base<D, T, M>& other) requires (M <= N);

		template <typename D, internal::is_vec_element T2>
		constexpr base(const base<D, T2, N>& other);

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
	};
}

export namespace gse::unitless {
	template <typename A, std::size_t N> requires std::is_arithmetic_v<A>
	class vec_t : public vec::base<vec_t<A, N>, A, N> {
	public:
		using vec::base<vec_t, A, N>::base;
	};
}

export namespace gse {
	template <internal::is_arithmetic_wrapper Q, std::size_t N>
	class quantity_vec : public vec::base<quantity_vec<Q, N>, Q, N> {
	public:
		using vec::base<quantity_vec, Q, N>::base;

		template <typename Unit> requires internal::is_derivable_unit<Unit>
		constexpr auto as() const -> unitless::vec_t<typename Q::value_type, N>;

		template <auto TargetUnit> requires internal::is_derivable_unit<decltype(TargetUnit)>
		constexpr auto as() const -> unitless::vec_t<typename Q::value_type, N>;
	};
}

export namespace gse {
	template <typename T, std::size_t N>
	struct vector_type_for_element;

	template <typename T, std::size_t N> requires std::is_arithmetic_v<T>
	struct vector_type_for_element<T, N> {
		using type = unitless::vec_t<T, N>;
	};

	template <internal::is_arithmetic_wrapper T, std::size_t N>
	struct vector_type_for_element<T, N> {
		using type = quantity_vec<T, N>;
	};

	template <typename T, std::size_t N>
	using vector_type_for_element_t = vector_type_for_element<T, N>::type;

	template <typename V>
	concept is_vec = 
		requires(const V& v) {
        typename V::tag;   
        typename V::value_type; 
        { v.as_storage_span() }; 
    };
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

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec::base<Derived, T, N>::base(const T& value) {
	this->data.fill(value);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec::base<Derived, T, N>::base(const T* values) {
	std::copy_n(values, N, this->data.begin());
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec::base<Derived, T, N>::base(const storage<T, N>& data) : storage<T, N>(data) {}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr gse::vec::base<Derived, T, N>::base(const std::array<T, N>& data) : storage<T, N>{ data } {}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
template <typename V> requires (sizeof(V) < sizeof(T))
	constexpr gse::vec::base<Derived, T, N>::base(const storage<V, N>& data) {
	for (std::size_t i = 0; i < data.data.size(); ++i) {
		this->data[i] = static_cast<T>(data.data[i]);
	}
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
template <typename V> requires (sizeof(V) < sizeof(T))
	constexpr gse::vec::base<Derived, T, N>::base(const std::array<V, N>& data) {
	for (std::size_t i = 0; i < data.size(); ++i) {
		this->data[i] = static_cast<T>(data[i]);
	}
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
template <typename... Args>
constexpr gse::vec::base<Derived, T, N>::base(Args&&... args)
	requires gse::internal::vec_ctor_compatible<T, N, Args...> {
	std::size_t idx = 0;

	auto append = [this, &idx]<typename T0>(T0&& arg) {
		using a = std::remove_reference_t<T0>;
		if constexpr (gse::is_vec<a>) {
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

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
template <typename D, std::size_t M>
constexpr gse::vec::base<Derived, T, N>::base(const base<D, T, M>& other) requires (M <= N) {
	for (std::size_t i = 0; i < M; ++i) {
		this->data[i] = other.data[i];
	}
	for (std::size_t i = M; i < N; ++i) {
		this->data[i] = T{};
	}
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
template <typename D, gse::internal::is_vec_element T2>
constexpr gse::vec::base<Derived, T, N>::base(const base<D, T2, N>& other) {
	for (std::size_t i = 0; i < N; ++i) {
		this->data[i] = static_cast<T>(other[i]);
	}
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::at(this auto& self, std::size_t index) {
	return self.data.at(index);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::operator[](this auto& self, std::size_t index) {
	return self.data[index];
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
template <typename E> requires std::is_enum_v<E>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::operator[](this auto& self, E index) {
	return self.at(static_cast<std::size_t>(index));
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::x(this auto&& self) requires(N > 0) {
	return self.at(0);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::y(this auto&& self) requires(N > 1) {
	return self.at(1);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::z(this auto&& self) requires(N > 2) {
	return self.at(2);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::w(this auto&& self) requires(N > 3) {
	return self.at(3);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::r(this auto&& self) requires (N > 0) {
	return self.at(0);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::g(this auto&& self) requires (N > 1) {
	return self.at(1);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::b(this auto&& self) requires (N > 2) {
	return self.at(2);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr decltype(auto) gse::vec::base<Derived, T, N>::a(this auto&& self) requires (N > 3) {
	return self.at(3);
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr auto gse::vec::base<Derived, T, N>::begin(this auto&& self) {
	return self.data.begin();
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
constexpr auto gse::vec::base<Derived, T, N>::end(this auto&& self) {
	return self.data.end();
}

template <typename Derived, gse::internal::is_vec_element T, std::size_t N>
auto gse::vec::base<Derived, T, N>::as_storage_span(this auto&& self) {
	if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
		return std::span<const storage_type, N>(
			reinterpret_cast<const storage_type*>(self.data.data()), N
		);
	}
	else {
		return std::span<storage_type, N>(
			reinterpret_cast<storage_type*>(self.data.data()), N
		);
	}
}

template <gse::internal::is_arithmetic_wrapper Q, std::size_t N >
template <typename Unit> requires gse::internal::is_derivable_unit<Unit>
constexpr auto gse::quantity_vec<Q, N>::as() const -> unitless::vec_t<typename Q::value_type, N> {
	using storage_type = vec::base<quantity_vec, Q, N>::storage_type;
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

template <typename T, std::size_t N>
concept ref_t = sizeof(T) * N > 16;

template <typename T, std::size_t N>
concept val_t = !ref_t<T, N>;

export namespace gse::vec {
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

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires ref_t<T1, N>
	constexpr auto operator+(
		const base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires val_t<T1, N>
	constexpr auto operator+(
		base<D1, T1, N> lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires ref_t<T1, N>
	constexpr auto operator-(
		const base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires val_t<T1, N>
	constexpr auto operator-(
		base<D1, T1, N> lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires ref_t<T1, N>
	constexpr auto operator*(
		const base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires val_t<T1, N>
	constexpr auto operator*(
		base<D1, T1, N> lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires ref_t<T1, N>
	constexpr auto operator/(
		const base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires val_t<T1, N>
	constexpr auto operator/(
		base<D1, T1, N> lhs,
		const base<D2, T2, N>& rhs
	);

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires are_addable<T1, T2>
	constexpr auto operator+=(
		base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	) -> base<D1, T1, N>&;

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires are_subtractable<T1, T2>
	constexpr auto operator-=(
		base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	) -> base<D1, T1, N>&;

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires are_multipliable<T1, T2>
	constexpr auto operator*=(
		base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	) -> base<D1, T1, N>&;

	template <typename D1, typename D2, internal::is_vec_element T1, internal::is_vec_element T2, std::size_t N>
		requires are_divisible<T1, T2>
	constexpr auto operator/=(
		base<D1, T1, N>& lhs,
		const base<D2, T2, N>& rhs
	) -> base<D1, T1, N>&;

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
		requires ref_t<T, N>
	constexpr auto operator*(
		const base<D, T, N>& lhs,
		const S& rhs
	);

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
		requires ref_t<T, N>
	constexpr auto operator*(
		const S& lhs,
		const base<D, T, N>& rhs
	);

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
		requires ref_t<T, N>
	constexpr auto operator/(
		const base<D, T, N>& lhs,
		const S& rhs
	);

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
		requires val_t<T, N>
	constexpr auto operator*(
		base<D, T, N> lhs,
		const S& rhs
	);

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
		requires val_t<T, N>
	constexpr auto operator*(
		const S& lhs,
		base<D, T, N> rhs
	);

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
		requires val_t<T, N>
	constexpr auto operator/(
		base<D, T, N> lhs,
		const S& rhs
	);

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
	constexpr auto operator*=(
		base<D, T, N>& lhs,
		const S& rhs
	) -> base<D, T, N>&;

	template <typename D, internal::is_vec_element T, internal::is_vec_element S, std::size_t N>
	constexpr auto operator/=(
		base<D, T, N>& lhs,
		const S& rhs
	) -> base<D, T, N>&;

	template <typename D, internal::is_vec_element T, std::size_t N>
	constexpr auto operator+(
		const base<D, T, N>& v
	);

	template <typename D, internal::is_vec_element T, std::size_t N>
	constexpr auto operator-(
		const base<D, T, N>& v
	);
}
template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires ref_t<T1, N>
constexpr auto gse::vec::operator+(const base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) {
	using result_type = add_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::add(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires val_t<T1, N>
constexpr auto gse::vec::operator+(base<D1, T1, N> lhs, const base<D2, T2, N>& rhs) {
	using result_type = add_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::add(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires ref_t<T1, N>
constexpr auto gse::vec::operator-(const base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) {
	using result_type = sub_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::sub(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires val_t<T1, N>
constexpr auto gse::vec::operator-(base<D1, T1, N> lhs, const base<D2, T2, N>& rhs) {
	using result_type = sub_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::sub(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires ref_t<T1, N>
constexpr auto gse::vec::operator*(const base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) {
	using result_type = mul_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::mul(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires val_t<T1, N>
constexpr auto gse::vec::operator*(base<D1, T1, N> lhs, const base<D2, T2, N>& rhs) {
	using result_type = mul_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::mul(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires ref_t<T1, N>
constexpr auto gse::vec::operator/(const base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) {
	using result_type = div_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::div(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires val_t<T1, N>
constexpr auto gse::vec::operator/(base<D1, T1, N> lhs, const base<D2, T2, N>& rhs) {
	using result_type = div_exposed_t<T1, T2>;
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	simd::div(lhs.as_storage_span(), rhs.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires gse::vec::are_addable<T1, T2>
constexpr auto gse::vec::operator+=(base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) -> base<D1, T1, N>& {
	simd::add(lhs.as_storage_span(), rhs.as_storage_span(), lhs.as_storage_span());
	return lhs;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires gse::vec::are_subtractable<T1, T2>
constexpr auto gse::vec::operator-=(base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) -> base<D1, T1, N>& {
	simd::sub(lhs.as_storage_span(), rhs.as_storage_span(), lhs.as_storage_span());
	return lhs;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires gse::vec::are_multipliable<T1, T2>
constexpr auto gse::vec::operator*=(base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) -> base<D1, T1, N>& {
	simd::mul(lhs.as_storage_span(), rhs.as_storage_span(), lhs.as_storage_span());
	return lhs;
}

template <typename D1, typename D2, gse::internal::is_vec_element T1, gse::internal::is_vec_element T2, std::size_t N>
	requires gse::vec::are_divisible<T1, T2>
constexpr auto gse::vec::operator/=(base<D1, T1, N>& lhs, const base<D2, T2, N>& rhs) -> base<D1, T1, N>& {
	simd::div(lhs.as_storage_span(), rhs.as_storage_span(), lhs.as_storage_span());
	return lhs;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
	requires ref_t<T, N>
constexpr auto gse::vec::operator*(const base<D, T, N>& lhs, const S& rhs) {
	using result_type = decltype(std::declval<T>()* rhs);
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	const auto scalar_primitive = [&] {
		if constexpr (internal::is_arithmetic_wrapper<S>) {
			using scalar_storage_t = internal::vec_storage_type_t<S>;
			return *reinterpret_cast<const scalar_storage_t*>(&rhs);
		}
		else {
			return rhs;
		}
	}();
	simd::mul_s(lhs.as_storage_span(), static_cast<base<D, T, N>::storage_type>(scalar_primitive), out.as_storage_span());
	return out;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
	requires ref_t<T, N>
constexpr auto gse::vec::operator*(const S& lhs, const base<D, T, N>& rhs) {
	return rhs * lhs;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
	requires ref_t<T, N>
constexpr auto gse::vec::operator/(const base<D, T, N>& lhs, const S& rhs) {
	using result_type = decltype(std::declval<T>() / rhs);
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	const auto scalar_primitive = [&] {
		if constexpr (internal::is_arithmetic_wrapper<S>) {
			using scalar_storage_t = internal::vec_storage_type_t<S>;
			return *reinterpret_cast<const scalar_storage_t*>(&rhs);
		}
		else {
			return rhs;
		}
	}();
	simd::div_s(lhs.as_storage_span(), static_cast<base<D, T, N>::storage_type>(scalar_primitive), out.as_storage_span());
	return out;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
	requires val_t<T, N>
constexpr auto gse::vec::operator*(base<D, T, N> lhs, const S& rhs) {
	using result_type = decltype(std::declval<T>() * rhs);
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	const auto scalar_primitive = [&] {
		if constexpr (internal::is_arithmetic_wrapper<S>) {
			using scalar_storage_t = internal::vec_storage_type_t<S>;
			return *reinterpret_cast<const scalar_storage_t*>(&rhs);
		}
		else {
			return rhs;
		}
	}();
	simd::mul_s(lhs.as_storage_span(), static_cast<base<D, T, N>::storage_type>(scalar_primitive), out.as_storage_span());
	return out;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
	requires val_t<T, N>
constexpr auto gse::vec::operator*(const S& lhs, base<D, T, N> rhs) {
	return rhs * lhs;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
	requires val_t<T, N>
constexpr auto gse::vec::operator/(base<D, T, N> lhs, const S& rhs) {
	using result_type = decltype(std::declval<T>() / rhs);
	using result_vec = vector_type_for_element_t<result_type, N>;

	result_vec out{};
	const auto scalar_primitive = [&] {
		if constexpr (internal::is_arithmetic_wrapper<S>) {
			using scalar_storage_t = internal::vec_storage_type_t<S>;
			return *reinterpret_cast<const scalar_storage_t*>(&rhs);
		}
		else {
			return rhs;
		}
	}();
	simd::div_s(lhs.as_storage_span(), static_cast<base<D, T, N>::storage_type>(scalar_primitive), out.as_storage_span());
	return out;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
constexpr auto gse::vec::operator*=(base<D, T, N>& lhs, const S& rhs) -> base<D, T, N>& {
	const auto scalar_primitive = [&] {
		if constexpr (internal::is_arithmetic_wrapper<S>) {
			using scalar_storage_t = internal::vec_storage_type_t<S>;
			return *reinterpret_cast<const scalar_storage_t*>(&rhs);
		}
		else {
			return rhs;
		}
	}();
	simd::mul_s(lhs.as_storage_span(), static_cast<base<D, T, N>::storage_type>(scalar_primitive), lhs.as_storage_span());
	return lhs;
}

template <typename D, gse::internal::is_vec_element T, gse::internal::is_vec_element S, std::size_t N>
constexpr auto gse::vec::operator/=(base<D, T, N>& lhs, const S& rhs) -> base<D, T, N>& {
	const auto scalar_primitive = [&] {
		if constexpr (internal::is_arithmetic_wrapper<S>) {
			using scalar_storage_t = internal::vec_storage_type_t<S>;
			return *reinterpret_cast<const scalar_storage_t*>(&rhs);
		}
		else {
			return rhs;
		}
	}();
	simd::div_s(lhs.as_storage_span(), static_cast<base<D, T, N>::storage_type>(scalar_primitive), lhs.as_storage_span());
	return lhs;
}

template <typename D, gse::internal::is_vec_element T, std::size_t N>
constexpr auto gse::vec::operator+(const base<D, T, N>& v) {
	return v;
}

template <typename D, gse::internal::is_vec_element T, std::size_t N>
constexpr auto gse::vec::operator-(const base<D, T, N>& v) {
	D out{};
	simd::mul_s(v.as_storage_span(), static_cast<base<D, T, N>::storage_type>(-1), out.as_storage_span());
	return out;
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
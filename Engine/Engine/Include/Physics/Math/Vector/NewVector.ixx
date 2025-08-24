export module gse.physics.math:new_vec;

import std;

import :simd;
import :quant;

export namespace gse::vec {
	template <typename T, std::size_t N>
	struct storage {
		std::array<T, N> data{};
	};
}

namespace gse::vec {
	template <internal::is_quantity Q, typename Stored>
	struct quantity_ref {
		using value_type = typename Q::value_type;
		
		Stored* p = nullptr;

		explicit constexpr operator Q() const noexcept;

		constexpr auto operator=(const Q& other) noexcept -> quantity_ref&;
		constexpr auto operator=(internal::is_arithmetic auto v) -> quantity_ref&;

		constexpr auto operator+=(const Q& other) noexcept -> quantity_ref&;
		constexpr auto operator-=(const Q& other) noexcept -> quantity_ref&;

		constexpr auto operator*=(value_type scalar) noexcept -> quantity_ref&;
		constexpr auto operator/=(value_type scalar) noexcept -> quantity_ref&;
	};
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr gse::vec::quantity_ref<Q, Stored>::operator Q() const noexcept {
	return Q(static_cast<value_type>(*p));
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr auto gse::vec::quantity_ref<Q, Stored>::operator=(const Q& other) noexcept -> quantity_ref& {
	*p = static_cast<Stored>(other.as_default_unit());
	return *this;
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr auto gse::vec::quantity_ref<Q, Stored>::operator=(internal::is_arithmetic auto v) -> quantity_ref& {
	*p = static_cast<Stored>(v);
	return *this;
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr auto gse::vec::quantity_ref<Q, Stored>::operator+=(const Q& other) noexcept -> quantity_ref& {
	*p += static_cast<Stored>(other.as_default_unit());
	return *this;
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr auto gse::vec::quantity_ref<Q, Stored>::operator-=(const Q& other) noexcept -> quantity_ref& {
	*p -= static_cast<Stored>(other.as_default_unit());
	return *this;
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr auto gse::vec::quantity_ref<Q, Stored>::operator*=(value_type scalar) noexcept -> quantity_ref& {
	*p *= static_cast<Stored>(scalar);
	return *this;
}

template <gse::internal::is_quantity Q, typename Stored>
constexpr auto gse::vec::quantity_ref<Q, Stored>::operator/=(value_type scalar) noexcept -> quantity_ref& {
	*p /= static_cast<Stored>(scalar);
	return *this;
}

namespace gse::vec {
	template <typename T, std::size_t N, typename U = T>
	class base : storage<T, N> {
	public:
		using stored_type = U;
		using exposed_type = T;
		static constexpr bool exposes_quantity = internal::is_quantity<U>;
		using value_type = std::conditional_t<exposes_quantity, typename U::value_type, T>;

		constexpr base() = default;
		constexpr base(const U& value);
		constexpr base(const T* values);
		constexpr base(auto&&... args) requires ((std::is_convertible_v<decltype(args), value_type> || (exposes_quantity && std::is_same_v<std::remove_cvref_t<decltype(args)>, U>)) && ...);

		template <std::size_t M>
		constexpr base(const base<T, M, U>& other) requires (M <= N);

		constexpr auto x() -> U& requires(N > 0) {
			return this->data[0];
		}

		constexpr auto at(this auto& self, std::size_t index) -> auto;

		constexpr auto operator[](std::size_t index) -> U&;

		constexpr auto x(this auto& self) -> auto& requires(N > 0);
		constexpr auto y(this auto& self) -> auto& requires(N > 1);
		constexpr auto z(this auto& self) -> auto& requires(N > 2);
		constexpr auto w(this auto& self) -> auto& requires(N > 3);
	private:
		static auto to_value(const U& v) -> value_type;
	};
}

template <typename T, std::size_t N, typename U>
constexpr gse::vec::base<T, N, U>::base(const U& value) {
	for (auto& elem : this->data) {
		elem = value;
	}
}

template <typename T, std::size_t N, typename U>
constexpr gse::vec::base<T, N, U>::base(const T* values) {
	for (std::size_t i = 0; i < N; ++i) {
		this->data[i] = static_cast<U>(values[i]);
	}
}

template <typename T, std::size_t N, typename U>
constexpr gse::vec::base<T, N, U>::base(auto&&... args) requires ((std::is_convertible_v<decltype(args), value_type> || (exposes_quantity && std::is_same_v<std::remove_cvref_t<decltype(args)>, U>)) && ...) {
	constexpr std::size_t arg_count = sizeof...(args);
	constexpr std::size_t to_fill = arg_count < N ? N - arg_count : 0;
	value_type fill_values[to_fill] = {};
	value_type init_values[arg_count + to_fill] = { (static_cast<value_type>(args))..., fill_values... };

	for (std::size_t i = 0; i < N; ++i) {
		this->data[i] = static_cast<U>(init_values[i]);
	}
}

template <typename T, std::size_t N, typename U>
template <std::size_t M>
constexpr gse::vec::base<T, N, U>::base(const base<T, M, U>& other) requires (M <= N) {
	for (std::size_t i = 0; i < M; ++i) {
		this->data[i] = other.data[i];
	}
	for (std::size_t i = M; i < N; ++i) {
		this->data[i] = U{};
	}
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::base<T, N, U>::at(this auto& self, std::size_t index) -> auto {
	if constexpr (exposes_quantity) {
		if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
			return U(static_cast<value_type>(self.data[index]));
		}
		else {
			return quantity_ref<U, T>{ &self.data[index] };
		}
	}
	else {
		if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
			return static_cast<T>(self.data[index]);
		}
		else {
			return (self.data[index]);
		}
	}
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::base<T, N, U>::operator[](std::size_t index) -> U& {
	return at(index);
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::base<T, N, U>::x(this auto& self) -> auto& requires(N > 0) {
	return at(0);
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::base<T, N, U>::y(this auto& self) -> auto& requires(N > 1) {
	return at(1);
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::base<T, N, U>::z(this auto& self) -> auto& requires(N > 2) {
	return at(2);
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::base<T, N, U>::w(this auto& self) -> auto& requires(N > 3) {
	return at(3);
}

template <typename T, std::size_t N, typename U>
auto gse::vec::base<T, N, U>::to_value(const U& v) -> value_type {
	if constexpr (exposes_quantity) {
		return static_cast<value_type>(v.as_default_unit());
	}
	else {
		return static_cast<value_type>(v);
	}
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
	using mul_exposed_t = decltype(std::declval<U>()* std::declval<V>());

	template <typename U, typename V>
	using div_exposed_t = decltype(std::declval<U>() / std::declval<V>());

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator+(
		const base<T, N, U>& lhs,
		const base<T, N, V>& rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires val_t<T, N>
	constexpr auto operator+(
		base<T, N, U> lhs,
		base<T, N, V> rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator-(
		const base<T, N, U>& lhs, 
		const base<T, N, V>& rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires val_t<T, N>
	constexpr auto operator-(
		base<T, N, U> lhs,
		base<T, N, V> rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator*(
		const base<T, N, U>& lhs,
		const base<T, N, V>& rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires val_t<T, N>
	constexpr auto operator*(
		base<T, N, U> lhs, 
		base<T, N, V> rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator/(
		const base<T, N, U>& lhs,
		const base<T, N, V>& rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires val_t<T, N>
	constexpr auto operator/(
		base<T, N, U> lhs, 
		base<T, N, V> rhs
	);

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator+=(
		base<T, N, U>& lhs, 
		const base<T, N, V>& rhs
	) -> base<T, N, U>&;

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator-=(
		base<T, N, U>& lhs, 
		const base<T, N, V>& rhs
	) -> base<T, N, U>&;

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator*=(
		base<T, N, U>& lhs,
		const base<T, N, V>& rhs
	) -> base<T, N, U>&;

	template <typename T, std::size_t N, typename U, typename V>
		requires ref_t<T, N>
	constexpr auto operator/=(
		base<T, N, U>& lhs,
		const base<T, N, V>& rhs
	) -> base<T, N, U>&;

	template <typename T, std::size_t N, typename U>
		requires ref_t<T, N>
	constexpr auto operator*(
		const base<T, N, U>& lhs,
		T rhs
	);

	template <typename T, std::size_t N, typename U>
		requires ref_t<T, N>
	constexpr auto operator*(
		T lhs, 
		const base<T, N, U>& rhs
	);

	template <typename T, std::size_t N, typename U>
		requires ref_t<T, N>
	constexpr auto operator/(
		const base<T, N, U>& lhs,
		T rhs
	);

	template <typename T, std::size_t N, typename U>
		requires ref_t<T, N>
	constexpr auto operator/(
		T lhs,
		const base<T, N, U>& rhs
	);

	template <typename T, std::size_t N, typename U>
		requires val_t<T, N>
	constexpr auto operator*(
		base<T, N, U> lhs,
		T rhs
	);

	template <typename T, std::size_t N, typename U>
		requires val_t<T, N>
	constexpr auto operator*(
		T lhs,
		base<T, N, U> rhs
	);

	template <typename T, std::size_t N, typename U>
		requires val_t<T, N>
	constexpr auto operator/(
		base<T, N, U> lhs, 
		T rhs
	);

	template <typename T, std::size_t N, typename U>
		requires val_t<T, N>
	constexpr auto operator/(
		T lhs, 
		base<T, N, U> rhs
	);

	template <typename T, std::size_t N, typename U>
	constexpr auto operator*=(
		base<T, N, U>& lhs,
		T rhs
	) -> base<T, N, U>&;

	template <typename T, std::size_t N, typename U>
	constexpr auto operator/=(
		base<T, N, U>& lhs, 
		T rhs
	) -> base<T, N, U>&;

	template <typename T, std::size_t N, typename U>
	constexpr auto operator+(
		const base<T, N, U>& v
	);

	template <typename T, std::size_t N, typename U>
	constexpr auto operator-(
		const base<T, N, U>& v
	);
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator+(const base<T, N, U>& lhs, const base<T, N, V>& rhs) {
	using rexp = add_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::add(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires val_t<T, N>
constexpr auto gse::vec::operator+(base<T, N, U> lhs, base<T, N, V> rhs) {
	using rexp = add_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::add(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator-(const base<T, N, U>& lhs, const base<T, N, V>& rhs) {
	using rexp = sub_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::sub(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires val_t<T, N>
constexpr auto gse::vec::operator-(base<T, N, U> lhs, base<T, N, V> rhs) {
	using rexp = sub_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::sub(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator*(const base<T, N, U>& lhs, const base<T, N, V>& rhs) {
	using rexp = mul_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::mul(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires val_t<T, N>
constexpr auto gse::vec::operator*(base<T, N, U> lhs, base<T, N, V> rhs) {
	using rexp = mul_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::mul(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator/(const base<T, N, U>& lhs, const base<T, N, V>& rhs) {
	using rexp = div_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::div(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires val_t<T, N>
constexpr auto gse::vec::operator/(base<T, N, U> lhs, base<T, N, V> rhs) {
	using rexp = div_exposed_t<U, V>;
	base<T, N, rexp> out{};
	simd::div(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator+=(base<T, N, U>& lhs, const base<T, N, V>& rhs) -> base<T, N, U>& {
	[[maybe_unused]] using check = add_exposed_t<U, V>;
	simd::add(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator-=(base<T, N, U>& lhs, const base<T, N, V>& rhs) -> base<T, N, U>& {
	[[maybe_unused]] using check = sub_exposed_t<U, V>;
	simd::sub(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator*=(base<T, N, U>& lhs, const base<T, N, V>& rhs) -> base<T, N, U>& {
	[[maybe_unused]] using check = mul_exposed_t<U, V>;
	simd::mul(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, std::size_t N, typename U, typename V> requires ref_t<T, N>
constexpr auto gse::vec::operator/=(base<T, N, U>& lhs, const base<T, N, V>& rhs) -> base<T, N, U>& {
	[[maybe_unused]] using check = div_exposed_t<U, V>;
	simd::div(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, std::size_t N, typename U> requires ref_t<T, N>
constexpr auto gse::vec::operator*(const base<T, N, U>& lhs, T rhs) {
	base<T, N, U> out{};
	simd::mul_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires ref_t<T, N>
constexpr auto gse::vec::operator*(T lhs, const base<T, N, U>& rhs) {
	base<T, N, U> out{};
	simd::mul_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires ref_t<T, N>
constexpr auto gse::vec::operator/(const base<T, N, U>& lhs, T rhs) {
	base<T, N, U> out{};
	simd::div_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires ref_t<T, N>
constexpr auto gse::vec::operator/(T lhs, const base<T, N, U>& rhs) {
	base<T, N, U> out{};
	simd::div_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires val_t<T, N>
constexpr auto gse::vec::operator*(base<T, N, U> lhs, T rhs) {
	base<T, N, U> out{};
	simd::mul_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires val_t<T, N>
constexpr auto gse::vec::operator*(T lhs, base<T, N, U> rhs) {
	base<T, N, U> out{};
	simd::mul_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires val_t<T, N>
constexpr auto gse::vec::operator/(base<T, N, U> lhs, T rhs) {
	base<T, N, U> out{};
	simd::div_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U> requires val_t<T, N>
constexpr auto gse::vec::operator/(T lhs, base<T, N, U> rhs) {
	base<T, N, U> out{};
	simd::div_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(out.data));
	return out;
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::operator*=(base<T, N, U>& lhs, T rhs) -> base<T, N, U>& {
	simd::mul_s(std::span<T, N>(lhs.data), rhs, std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::operator/=(base<T, N, U>& lhs, T rhs) -> base<T, N, U>& {
	simd::div_s(std::span<T, N>(lhs.data), rhs, std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::operator+(const base<T, N, U>& v) {
	return v;
}

template <typename T, std::size_t N, typename U>
constexpr auto gse::vec::operator-(const base<T, N, U>& v) {
	base<T, N, U> out{};
	simd::mul_s(std::span<const T, N>(v.data), static_cast<T>(-1), std::span<T, N>(out.data));
	return out;
}

export namespace gse::exp::unitless {
	template <typename T, std::size_t N>
	using vec = vec::base<T, N>;

	template <typename T>
	using vec2_t = vec<T, 2>;

	template <typename T>
	using vec3_t = vec<T, 3>;

	template <typename T>
	using vec4_t = vec<T, 4>;

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
}

export namespace gse::exp::unit {
	template <internal::is_quantity T, std::size_t N>
	using vec = vec::base<typename T::value_type, N, T>;

	template <typename T>
	using vec2_t = vec<T, 2>;

	template <typename T>
	using vec3_t = vec<T, 3>;

	template <typename T>
	using vec4_t = vec<T, 4>;
}

export module gse.math:mixed_vec;

import std;

import :vector;
import :quant;

namespace gse::internal {
	template <std::size_t I, typename... Ts>
	struct nth_type;

	template <std::size_t I, typename T, typename... Rest>
	struct nth_type<I, T, Rest...> : nth_type<I - 1, Rest...> {};

	template <typename T, typename... Rest>
	struct nth_type<0, T, Rest...> {
		using type = T;
	};

	template <std::size_t I, typename... Ts>
	using nth_t = nth_type<I, Ts...>::type;
}

export namespace gse {
	template <typename... Qs>
	class mixed_vec : vec<float, sizeof...(Qs)> {
		using base = vec<float, sizeof...(Qs)>;
	public:
		static constexpr std::size_t size = sizeof...(Qs);

		constexpr mixed_vec() = default;

		explicit constexpr mixed_vec(
			const vec<float, size>& raw
		);

		template <std::size_t I>
		constexpr auto get(
		) const -> internal::nth_t<I, Qs...>;

		template <std::size_t I>
		constexpr auto set(
			internal::nth_t<I, Qs...> val
		) -> void;
	};
}

template <typename... Qs>
constexpr gse::mixed_vec<Qs...>::mixed_vec(const vec<float, size>& raw) : base(raw) {}

template <typename... Qs>
template <std::size_t I>
constexpr auto gse::mixed_vec<Qs...>::get() const -> internal::nth_t<I, Qs...> {
	return internal::nth_t<I, Qs...>((*this)[I]);
}

template <typename... Qs>
template <std::size_t I>
constexpr auto gse::mixed_vec<Qs...>::set(internal::nth_t<I, Qs...> val) -> void {
	(*this)[I] = internal::to_storage(val);
}

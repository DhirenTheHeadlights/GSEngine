export module gse.math:mixed_mat;

import std;

import :vector;
import :matrix;
import :mixed_vec;
import :quant;

export namespace gse {
	template <typename... Ts>
	struct unit_spec {
		static constexpr std::size_t size = sizeof...(Ts);

		template <std::size_t I>
		using type = internal::nth_t<I, Ts...>;
	};

	template <typename ColSpec, typename RowSpec, typename T = float>
	class mixed_mat : public mat<T, ColSpec::size, ColSpec::size> {
		using base = mat<T, ColSpec::size, ColSpec::size>;
		static constexpr std::size_t N = ColSpec::size;
		static_assert(ColSpec::size == RowSpec::size);
	public:
		template <std::size_t Col>
		using col_t = ColSpec::template type<Col>;

		template <std::size_t Row>
		using row_t = RowSpec::template type<Row>;

		template <std::size_t Col, std::size_t Row>
		using element_t = decltype(std::declval<row_t<Row>>() / std::declval<col_t<Col>>());

		template <typename Spec, typename Seq>
		struct mixed_vec_for;

		template <typename Spec, std::size_t... Is>
		struct mixed_vec_for<Spec, std::index_sequence<Is...>> {
			using type = mixed_vec<typename Spec::template type<Is>...>;
		};

		using input_vec = mixed_vec_for<ColSpec, std::make_index_sequence<N>>::type;
		using output_vec = mixed_vec_for<RowSpec, std::make_index_sequence<N>>::type;

		constexpr mixed_mat() = default;

		explicit constexpr mixed_mat(
			const mat<T, N, N>& raw
		);

		template <std::size_t Col, std::size_t Row>
		constexpr auto at(
		) const -> element_t<Col, Row>;

		template <std::size_t Col, std::size_t Row>
		constexpr auto set(
			element_t<Col, Row> val
		) -> void;

		constexpr auto operator*(
			const mixed_mat& rhs
		) const -> mixed_mat;

		template <typename OtherCol, typename OtherRow>
			requires std::same_as<typename OtherRow::template type<0>, typename ColSpec::template type<0>>
		constexpr auto operator*(
			const mixed_mat<OtherCol, OtherRow, T>& rhs
		) const -> mixed_mat<OtherCol, RowSpec, T>;

		template <internal::is_quantity Q>
			requires (internal::same_unit_family_v<typename Q::quantity_tag, typename ColSpec::template type<0>::quantity_tag>)
		constexpr auto transform_point(
			const vec3<Q>& p
		) const -> vec3<row_t<0>>
			requires (N == 4 &&
				std::same_as<col_t<0>, col_t<1>> && std::same_as<col_t<1>, col_t<2>> &&
				std::same_as<row_t<0>, row_t<1>> && std::same_as<row_t<1>, row_t<2>>);

		constexpr auto transform_direction(
			const vec3f& d
		) const -> vec3f
			requires (N == 4);

		constexpr auto inverse(
		) const -> mixed_mat<RowSpec, ColSpec, T>;

		template <typename OtherCol, typename OtherRow>
		constexpr auto inverse_as(
		) const -> mixed_mat<OtherCol, OtherRow, T>;

	};
}

template <typename ColSpec, typename RowSpec, typename T>
constexpr gse::mixed_mat<ColSpec, RowSpec, T>::mixed_mat(const mat<T, N, N>& raw) : base(raw) {}

template <typename ColSpec, typename RowSpec, typename T>
template <std::size_t Col, std::size_t Row>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::at() const -> element_t<Col, Row> {
	return element_t<Col, Row>((*this)[Col][Row]);
}

template <typename ColSpec, typename RowSpec, typename T>
template <std::size_t Col, std::size_t Row>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::set(element_t<Col, Row> val) -> void {
	(*this)[Col][Row] = internal::to_storage(val);
}

template <typename ColSpec, typename RowSpec, typename T>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::operator*(const mixed_mat& rhs) const -> mixed_mat {
	return mixed_mat{ static_cast<const base&>(*this) * static_cast<const base&>(rhs) };
}

template <typename ColSpec, typename RowSpec, typename T>
template <typename OtherCol, typename OtherRow> requires std::same_as<typename OtherRow::template type<0>, typename ColSpec::template type<0>>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::operator*(const mixed_mat<OtherCol, OtherRow, T>& rhs) const -> mixed_mat<OtherCol, RowSpec, T> {
	return mixed_mat<OtherCol, RowSpec, T>{
		static_cast<const base&>(*this) * static_cast<const mixed_mat<OtherCol, OtherRow, T>::base&>(rhs)
	};
}

template <typename ColSpec, typename RowSpec, typename T>
template <gse::internal::is_quantity Q>
	requires (gse::internal::same_unit_family_v<typename Q::quantity_tag, typename ColSpec::template type<0>::quantity_tag>)
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::transform_point(const vec3<Q>& p) const -> vec3<row_t<0>>
	requires (N == 4 &&
		std::same_as<col_t<0>, col_t<1>> && std::same_as<col_t<1>, col_t<2>> &&
		std::same_as<row_t<0>, row_t<1>> && std::same_as<row_t<1>, row_t<2>>) {
	const auto v4 = static_cast<const base&>(*this) * vec4f{
		internal::to_storage(p.x()),
		internal::to_storage(p.y()),
		internal::to_storage(p.z()),
		T(1)
	};
	return { row_t<0>(v4[0]), row_t<0>(v4[1]), row_t<0>(v4[2]) };
}

template <typename ColSpec, typename RowSpec, typename T>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::transform_direction(const vec3f& d) const -> vec3f
	requires (N == 4) {
	const auto v4 = static_cast<const base&>(*this) * vec4f{ d.x(), d.y(), d.z(), T(0) };
	return { v4[0], v4[1], v4[2] };
}

template <typename ColSpec, typename RowSpec, typename T>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::inverse() const -> mixed_mat<RowSpec, ColSpec, T> {
	return mixed_mat<RowSpec, ColSpec, T>{ static_cast<const base&>(*this).inverse() };
}

template <typename ColSpec, typename RowSpec, typename T>
template <typename OtherCol, typename OtherRow>
constexpr auto gse::mixed_mat<ColSpec, RowSpec, T>::inverse_as() const -> mixed_mat<OtherCol, OtherRow, T> {
	return mixed_mat<OtherCol, OtherRow, T>{ static_cast<const base&>(*this).inverse() };
}

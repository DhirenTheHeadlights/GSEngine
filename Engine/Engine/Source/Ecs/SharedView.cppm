export module gse.ecs:shared_view;

import std;
import gse.meta;

import :traits;

export namespace gse {
	struct shared_tag {};
	constexpr shared_tag shared{};
}

namespace gse {
	enum class publish_kind : std::uint8_t {
		value,
		decay,
		live
	};

	template <typename T>
	struct unique_ptr_traits {
		static constexpr bool is_unique_ptr = false;
	};

	template <typename T, typename D>
	struct unique_ptr_traits<std::unique_ptr<T, D>> {
		static constexpr bool is_unique_ptr = true;
		using pointee = T;
	};

	template <typename T>
	consteval auto publish_kind_of() -> publish_kind {
		if constexpr (unique_ptr_traits<T>::is_unique_ptr) {
			return publish_kind::decay;
		}
		else if constexpr (std::is_trivially_copyable_v<T> && std::is_copy_assignable_v<T>) {
			return publish_kind::value;
		}
		else {
			return publish_kind::live;
		}
	}

	template <typename Data>
	consteval auto shared_member_infos() {
		std::vector<std::meta::info> members;
		for (auto m : std::meta::nonstatic_data_members_of(^^Data, std::meta::access_context::unchecked())) {
			if (has_annotation<shared_tag>(m)) {
				members.push_back(m);
			}
		}
		return members;
	}

	template <typename Data>
	constexpr auto shared_members_v = std::define_static_array(shared_member_infos<Data>());

	template <typename Data>
	consteval auto view_member_specs() -> std::vector<std::meta::info> {
		std::vector<std::meta::info> specs;
		template for (constexpr auto m : shared_members_v<Data>) {
			constexpr auto field_type = std::meta::dealias(std::meta::type_of(m));
			using field_t = typename[:field_type:];
			if constexpr (publish_kind_of<field_t>() == publish_kind::decay) {
				specs.push_back(std::meta::data_member_spec(
					std::meta::add_pointer(std::meta::dealias(^^typename unique_ptr_traits<field_t>::pointee)),
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			else {
				specs.push_back(std::meta::data_member_spec(
					std::meta::add_lvalue_reference(std::meta::add_const(field_type)),
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
		}
		return specs;
	}

	template <typename Data>
	consteval auto snapshot_member_specs() -> std::vector<std::meta::info> {
		std::vector<std::meta::info> specs;
		template for (constexpr auto m : shared_members_v<Data>) {
			constexpr auto field_type = std::meta::dealias(std::meta::type_of(m));
			using field_t = typename[:field_type:];
			constexpr auto kind = publish_kind_of<field_t>();
			if constexpr (kind == publish_kind::decay) {
				specs.push_back(std::meta::data_member_spec(
					std::meta::add_pointer(std::meta::dealias(^^typename unique_ptr_traits<field_t>::pointee)),
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			else if constexpr (kind == publish_kind::value) {
				specs.push_back(std::meta::data_member_spec(
					field_type,
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			else {
				specs.push_back(std::meta::data_member_spec(
					std::meta::add_pointer(std::meta::add_const(field_type)),
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
		}
		return specs;
	}

	template <typename Data>
	struct shared_view_base {
		struct type;

		consteval {
			std::meta::define_aggregate(^^type, view_member_specs<Data>());
		}
	};

	template <typename Data>
	struct shared_snapshot_base {
		struct type;

		consteval {
			std::meta::define_aggregate(^^type, snapshot_member_specs<Data>());
		}
	};
}

export namespace gse {
	template <typename Data>
	using shared_snapshot = typename shared_snapshot_base<Data>::type;

	template <typename S>
	struct shared_view : shared_view_base<state_of_t<S>>::type {
		using target_system_t = S;
	};

	template <typename T>
	struct is_shared_view : std::false_type {};

	template <typename S>
	struct is_shared_view<shared_view<S>> : std::true_type {};

	template <typename T>
	constexpr bool is_shared_view_v = is_shared_view<std::remove_cvref_t<T>>::value;

	template <typename T>
	struct shared_view_target;

	template <typename S>
	struct shared_view_target<shared_view<S>> {
		using type = S;
	};

	template <typename T>
	using shared_view_target_t = typename shared_view_target<std::remove_cvref_t<T>>::type;

	template <typename T>
	struct optional_shared_view_traits {
		static constexpr bool is_optional_shared_view = false;
	};

	template <typename S>
	struct optional_shared_view_traits<std::optional<shared_view<S>>> {
		static constexpr bool is_optional_shared_view = true;
		using target = S;
	};

	template <typename T>
	constexpr bool is_optional_shared_view_v = optional_shared_view_traits<std::remove_cvref_t<T>>::is_optional_shared_view;

	template <typename T>
	using optional_shared_view_target_t = typename optional_shared_view_traits<std::remove_cvref_t<T>>::target;

	template <typename Data>
	auto copy_shared_fields(
		const Data& d,
		shared_snapshot<Data>& out
	) -> void;

	template <typename S>
	auto make_shared_view_live(
		const state_of_t<S>& d
	) -> shared_view<S>;

	template <typename S>
	auto make_shared_view_snapshot(
		const shared_snapshot<state_of_t<S>>& s
	) -> shared_view<S>;
}

namespace gse {
	template <typename Data>
	constexpr auto snapshot_members_v = std::define_static_array(std::meta::nonstatic_data_members_of(^^shared_snapshot<Data>, std::meta::access_context::unchecked()));

	template <typename Data, std::size_t I>
	auto live_shared_field(
		const Data& d
	) -> decltype(auto);

	template <typename Data, std::size_t I>
	auto snapshot_shared_field(
		const shared_snapshot<Data>& s
	) -> decltype(auto);

	template <typename Data, std::size_t I>
	auto copy_shared_field(
		const Data& d,
		shared_snapshot<Data>& out
	) -> void;
}

template <typename Data, std::size_t I>
auto gse::live_shared_field(const Data& d) -> decltype(auto) {
	constexpr auto m = shared_members_v<Data>[I];
	using field_t = typename[:std::meta::dealias(std::meta::type_of(m)):];
	if constexpr (publish_kind_of<field_t>() == publish_kind::decay) {
		return d.[:m:].get();
	}
	else {
		return (d.[:m:]);
	}
}

template <typename Data, std::size_t I>
auto gse::snapshot_shared_field(const shared_snapshot<Data>& s) -> decltype(auto) {
	constexpr auto m = shared_members_v<Data>[I];
	constexpr auto sm = snapshot_members_v<Data>[I];
	using field_t = typename[:std::meta::dealias(std::meta::type_of(m)):];
	constexpr auto kind = publish_kind_of<field_t>();
	if constexpr (kind == publish_kind::live) {
		return (*s.[:sm:]);
	}
	else if constexpr (kind == publish_kind::decay) {
		return s.[:sm:];
	}
	else {
		return (s.[:sm:]);
	}
}

template <typename Data, std::size_t I>
auto gse::copy_shared_field(const Data& d, shared_snapshot<Data>& out) -> void {
	constexpr auto m = shared_members_v<Data>[I];
	constexpr auto sm = snapshot_members_v<Data>[I];
	using field_t = typename[:std::meta::dealias(std::meta::type_of(m)):];
	constexpr auto kind = publish_kind_of<field_t>();
	if constexpr (kind == publish_kind::decay) {
		out.[:sm:] = d.[:m:].get();
	}
	else if constexpr (kind == publish_kind::value) {
		out.[:sm:] = d.[:m:];
	}
	else {
		out.[:sm:] = &d.[:m:];
	}
}

template <typename Data>
auto gse::copy_shared_fields(const Data& d, shared_snapshot<Data>& out) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((copy_shared_field<Data, Is>(d, out)), ...);
	}(std::make_index_sequence<shared_members_v<Data>.size()>{});
}

template <typename S>
auto gse::make_shared_view_live(const state_of_t<S>& d) -> shared_view<S> {
	using base_t = shared_view_base<state_of_t<S>>::type;
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return shared_view<S>{ base_t{ live_shared_field<state_of_t<S>, Is>(d)... } };
	}(std::make_index_sequence<shared_members_v<state_of_t<S>>.size()>{});
}

template <typename S>
auto gse::make_shared_view_snapshot(const shared_snapshot<state_of_t<S>>& s) -> shared_view<S> {
	using base_t = shared_view_base<state_of_t<S>>::type;
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return shared_view<S>{ base_t{ snapshot_shared_field<state_of_t<S>, Is>(s)... } };
	}(std::make_index_sequence<shared_members_v<state_of_t<S>>.size()>{});
}

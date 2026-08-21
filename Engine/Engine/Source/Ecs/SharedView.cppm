export module gse.ecs:shared_view;

import std;
import gse.meta;
import gse.assert;

import :traits;

export namespace gse {
	struct shared_tag {};
	constexpr shared_tag shared{};
	struct stable_shared_tag {};
	constexpr stable_shared_tag stable_shared{};
}

namespace gse {
	enum class publish_kind : std::uint8_t {
		value,
		stable_pointer,
		owned_snapshot,
		nested,
		live
	};

	template <typename Data>
	struct shared_view_base;

	template <typename Data>
	struct shared_snapshot_base;

	template <typename T>
	struct pointer_like_traits {
		static constexpr bool is_unique = false;
		static constexpr bool is_shared = false;
	};

	template <typename T, typename D>
	struct pointer_like_traits<std::unique_ptr<T, D>> {
		static constexpr bool is_unique = true;
		static constexpr bool is_shared = false;
		using pointee = T;
	};

	template <typename T>
	struct pointer_like_traits<std::shared_ptr<T>> {
		static constexpr bool is_unique = false;
		static constexpr bool is_shared = true;
		using pointee = T;
	};

	template <typename Data>
	consteval auto shared_member_infos() {
		std::vector<std::meta::info> members;
		for (auto m : std::meta::nonstatic_data_members_of(^^Data, std::meta::access_context::unchecked())) {
			if (has_annotation<shared_tag>(m) || has_annotation<stable_shared_tag>(m)) {
				members.push_back(m);
			}
		}
		return members;
	}

	template <typename T>
	consteval auto publishes_nested() -> bool {
		if constexpr (std::is_class_v<T> && !pointer_like_traits<T>::is_unique && !pointer_like_traits<T>::is_shared) {
			return !shared_member_infos<T>().empty();
		}
		else {
			return false;
		}
	}

	template <typename T, std::meta::info Member>
	consteval auto publish_kind_of() -> publish_kind {
		if constexpr (pointer_like_traits<T>::is_unique) {
			static_assert(has_annotation<stable_shared_tag>(Member), "shared unique_ptr fields must use the stable_shared annotation");
			static_assert(!has_annotation<shared_tag>(Member), "stable shared unique_ptr fields must not also use the shared annotation");
			return publish_kind::stable_pointer;
		}
		else if constexpr (pointer_like_traits<T>::is_shared) {
			using pointee = typename pointer_like_traits<T>::pointee;
			static_assert(std::is_const_v<pointee>, "shared_ptr fields published through shared_view must point to const data");
			static_assert(!has_annotation<stable_shared_tag>(Member), "stable_shared is only for unique_ptr fields");
			return publish_kind::owned_snapshot;
		}
		else if constexpr (publishes_nested<T>()) {
			static_assert(!has_annotation<stable_shared_tag>(Member), "stable_shared is only for unique_ptr fields");
			return publish_kind::nested;
		}
		else if constexpr (std::is_trivially_copyable_v<T> && std::is_copy_assignable_v<T>) {
			static_assert(!has_annotation<stable_shared_tag>(Member), "stable_shared is only for unique_ptr fields");
			return publish_kind::value;
		}
		else {
			static_assert(!has_annotation<stable_shared_tag>(Member), "stable_shared is only for unique_ptr fields");
			return publish_kind::live;
		}
	}

	template <typename Data>
	constexpr auto shared_members_v = std::define_static_array(shared_member_infos<Data>());

	enum class live_field_form : std::uint8_t {
		reference,
		pointer
	};

	template <typename Data>
	consteval auto shared_field_specs(const live_field_form form) -> std::vector<std::meta::info> {
		std::vector<std::meta::info> specs;
		template for (constexpr auto m : shared_members_v<Data>) {
			constexpr auto field_type = std::meta::dealias(std::meta::type_of(m));
			using field_t = typename[:field_type:];
			constexpr auto kind = publish_kind_of<field_t, m>();
			if constexpr (kind == publish_kind::stable_pointer) {
				specs.push_back(std::meta::data_member_spec(
					std::meta::add_pointer(std::meta::dealias(^^typename pointer_like_traits<field_t>::pointee)),
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			else if constexpr (kind == publish_kind::owned_snapshot || kind == publish_kind::value) {
				specs.push_back(std::meta::data_member_spec(
					field_type,
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			else if constexpr (kind == publish_kind::nested) {
				using nested_view_t = typename shared_view_base<field_t>::type;
				using nested_snapshot_t = typename shared_snapshot_base<field_t>::type;
				specs.push_back(std::meta::data_member_spec(
					form == live_field_form::reference ? ^^nested_view_t : ^^nested_snapshot_t,
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			else {
				const auto live_type = form == live_field_form::reference
					? std::meta::add_lvalue_reference(std::meta::add_const(field_type))
					: std::meta::add_pointer(std::meta::add_const(field_type));
				specs.push_back(std::meta::data_member_spec(
					live_type,
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
			std::meta::define_aggregate(^^type, shared_field_specs<Data>(live_field_form::reference));
		}
	};

	template <typename Data>
	struct shared_snapshot_base {
		struct type;

		consteval {
			std::meta::define_aggregate(^^type, shared_field_specs<Data>(live_field_form::pointer));
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

	template <typename Data>
	auto live_shared_aggregate(
		const Data& d
	) -> typename shared_view_base<Data>::type;

	template <typename Data>
	auto snapshot_shared_aggregate(
		const shared_snapshot<Data>& s
	) -> typename shared_view_base<Data>::type;
}

template <typename Data, std::size_t I>
auto gse::live_shared_field(const Data& d) -> decltype(auto) {
	constexpr auto m = shared_members_v<Data>[I];
	using field_t = typename[:std::meta::dealias(std::meta::type_of(m)):];
	constexpr auto kind = publish_kind_of<field_t, m>();
	if constexpr (kind == publish_kind::stable_pointer) {
		return d.[:m:].get();
	}
	else if constexpr (kind == publish_kind::nested) {
		return live_shared_aggregate<field_t>(d.[:m:]);
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
	constexpr auto kind = publish_kind_of<field_t, m>();
	if constexpr (kind == publish_kind::live) {
		return (*s.[:sm:]);
	}
	else if constexpr (kind == publish_kind::nested) {
		return snapshot_shared_aggregate<field_t>(s.[:sm:]);
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
	constexpr auto kind = publish_kind_of<field_t, m>();
	if constexpr (kind == publish_kind::stable_pointer) {
		assert(out.[:sm:] == nullptr || out.[:sm:] == d.[:m:].get(), "stable shared field '{}' was reseated", std::meta::identifier_of(m));
		out.[:sm:] = d.[:m:].get();
	}
	else if constexpr (kind == publish_kind::owned_snapshot || kind == publish_kind::value) {
		out.[:sm:] = d.[:m:];
	}
	else if constexpr (kind == publish_kind::nested) {
		copy_shared_fields(d.[:m:], out.[:sm:]);
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

template <typename Data>
auto gse::live_shared_aggregate(const Data& d) -> typename shared_view_base<Data>::type {
	using base_t = typename shared_view_base<Data>::type;
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return base_t{ live_shared_field<Data, Is>(d)... };
	}(std::make_index_sequence<shared_members_v<Data>.size()>{});
}

template <typename Data>
auto gse::snapshot_shared_aggregate(const shared_snapshot<Data>& s) -> typename shared_view_base<Data>::type {
	using base_t = typename shared_view_base<Data>::type;
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return base_t{ snapshot_shared_field<Data, Is>(s)... };
	}(std::make_index_sequence<shared_members_v<Data>.size()>{});
}

template <typename S>
auto gse::make_shared_view_live(const state_of_t<S>& d) -> shared_view<S> {
	return shared_view<S>{ live_shared_aggregate<state_of_t<S>>(d) };
}

template <typename S>
auto gse::make_shared_view_snapshot(const shared_snapshot<state_of_t<S>>& s) -> shared_view<S> {
	return shared_view<S>{ snapshot_shared_aggregate<state_of_t<S>>(s) };
}

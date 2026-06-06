export module gse.ecs:shared_view;

import std;
import gse.meta;

export namespace gse {
	struct shared_tag {};
	constexpr shared_tag shared{};
}

namespace gse {
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
	struct shared_fields_aggregate {
		struct type;

		consteval {
			std::vector<std::meta::info> members;
			for (auto m : shared_member_infos<Data>()) {
				auto m_type = std::meta::type_of(m);
				auto ref_type = std::meta::add_lvalue_reference(std::meta::add_const(m_type));
				members.push_back(std::meta::data_member_spec(
					ref_type,
					{
						.name = std::meta::identifier_of(m)
					}
				));
			}
			std::meta::define_aggregate(^^type, members);
		}
	};
}

export namespace gse {
	template <typename S>
	struct shared_view : shared_fields_aggregate<typename S::data>::type {
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

	template <typename S>
	auto make_shared_view(
		const typename S::data& d
	) -> shared_view<S>;
}

namespace gse {
	template <typename S, std::size_t... Is>
	auto make_shared_view_impl(
		const typename S::data& d,
		std::index_sequence<Is...>
	) -> shared_view<S>;
}

template <typename S, std::size_t... Is>
auto gse::make_shared_view_impl(const typename S::data& d, std::index_sequence<Is...>) -> shared_view<S> {
	using view_t = shared_view<S>;
	using base_t = shared_fields_aggregate<typename S::data>::type;
	constexpr auto members = std::define_static_array(shared_member_infos<typename S::data>());
	return view_t{ base_t{ d.[:members[Is]:]... } };
}

template <typename S>
auto gse::make_shared_view(const typename S::data& d) -> shared_view<S> {
	constexpr auto members = std::define_static_array(shared_member_infos<typename S::data>());
	return make_shared_view_impl<S>(d, std::make_index_sequence<members.size()>{});
}

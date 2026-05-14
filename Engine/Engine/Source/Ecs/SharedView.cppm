export module gse.ecs:shared_view;

import std;
import gse.std_meta;
import gse.meta;

namespace gse {
	template <typename Data>
	struct shared_fields_aggregate {
		struct type;

		consteval {
			std::vector<std::meta::info> members;
			for (auto m : std::meta::nonstatic_data_members_of(^^Data, std::meta::access_context::unchecked())) {
				if (has_annotation<shared_tag>(m)) {
					auto m_type = std::meta::type_of(m);
					auto ref_type = std::meta::add_lvalue_reference(std::meta::add_const(m_type));
					members.push_back(std::meta::data_member_spec(
						ref_type,
						{ .name = std::meta::identifier_of(m) }
					));
				}
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

template <typename S>
auto gse::make_shared_view(const typename S::data& d) -> shared_view<S> {
	using V = shared_view<S>;
	using base_t = typename shared_fields_aggregate<typename S::data>::type;
	constexpr auto members = std::define_static_array([]consteval {
		std::vector<std::meta::info> result;
		for (auto m : std::meta::nonstatic_data_members_of(^^typename S::data, std::meta::access_context::unchecked())) {
			if (has_annotation<shared_tag>(m)) {
				result.push_back(m);
			}
		}
		return result;
	}());
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> V {
		return V{ base_t{ d.[:members[Is]:]... } };
	}(std::make_index_sequence<members.size()>{});
}

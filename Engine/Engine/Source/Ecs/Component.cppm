export module gse.ecs:component;

import std;
import gse.std_meta;

import gse.core;
import gse.meta;

export namespace gse {
	template <typename T>
	using network_data_t = project_by_annotation<T, networked_tag>;

	template <typename T>
	auto extract_networked(
		const T& c
	) -> network_data_t<T>;

	template <typename T>
	auto apply_networked(
		T& c,
		const network_data_t<T>& nd
	) -> void;
}

template <typename T>
auto gse::extract_networked(const T& c) -> network_data_t<T> {
	network_data_t<T> nd{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (has_annotation<networked_tag>(m)) {
			constexpr auto target = find_field_by_name<network_data_t<T>>(std::meta::identifier_of(m));
			if constexpr (target != std::meta::info{}) {
				using src_type = typename[:std::meta::type_of(m):];
				using dst_type = typename[:std::meta::type_of(target):];
				if constexpr (std::is_same_v<src_type, dst_type>) {
					nd.[:target:] = c.[:m:];
				}
				else {
					nd.[:target:] = extract_networked<src_type>(c.[:m:]);
				}
			}
		}
	}
	return nd;
}

template <typename T>
auto gse::apply_networked(T& c, const network_data_t<T>& nd) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (has_annotation<networked_tag>(m)) {
			constexpr auto src = find_field_by_name<network_data_t<T>>(std::meta::identifier_of(m));
			if constexpr (src != std::meta::info{}) {
				using src_type = typename[:std::meta::type_of(src):];
				using dst_type = typename[:std::meta::type_of(m):];
				if constexpr (std::is_same_v<src_type, dst_type>) {
					c.[:m:] = nd.[:src:];
				}
				else {
					apply_networked<dst_type>(c.[:m:], nd.[:src:]);
				}
			}
		}
	}
}

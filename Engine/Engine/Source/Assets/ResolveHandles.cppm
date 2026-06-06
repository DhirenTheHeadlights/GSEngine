export module gse.assets:resolve_handles;

import std;
import gse.meta;
import gse.ecs;

import :registry;
import :resource_handle;

export namespace gse::asset {
	template <typename T>
	auto resolve(
		resource::handle<T>& h,
		const data& assets
	) -> void;

	template <typename T>
	auto resolve_handles(
		T& c,
		const data& assets
	) -> void;
}

namespace gse::asset {
	template <typename>
	constexpr bool is_resource_handle_v = false;

	template <typename T>
	constexpr bool is_resource_handle_v<resource::handle<T>> = true;

	template <typename T>
	consteval auto has_networked_members() -> bool;
}

template <typename T>
consteval auto gse::asset::has_networked_members() -> bool {
	if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
		for (auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
			if (has_annotation<networked_tag>(m)) {
				return true;
			}
		}
	}
	return false;
}

template <typename T>
auto gse::asset::resolve(resource::handle<T>& h, const data& assets) -> void {
	if (h.id().exists()) {
		h = try_get<T>(assets, h.id());
	}
}

template <typename T>
auto gse::asset::resolve_handles(T& c, const data& assets) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (has_annotation<networked_tag>(m)) {
			using m_type = typename[:std::meta::type_of(m):];

			if constexpr (is_resource_handle_v<m_type>) {
				resolve(c.[:m:], assets);
			}
			else if constexpr (std::ranges::range<m_type>) {
				if constexpr (is_resource_handle_v<std::ranges::range_value_t<m_type>>) {
					for (auto& h : c.[:m:]) {
						resolve(h, assets);
					}
				}
			}
			else if constexpr (has_networked_members<m_type>()) {
				resolve_handles(c.[:m:], assets);
			}
		}
	}
}

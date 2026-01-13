export module gse.utility:concepts;

import std;

import :hook;
import :entity;

export namespace gse {
	template <typename T>
	concept is_entity_hook = std::derived_from<T, hook<entity>>;

	template <typename T>
	concept is_component = !is_entity_hook<T>;

	template <typename T>
	concept has_params = requires { typename T::params; };

	template <typename Hook, typename T>
	concept is_hook = std::derived_from<Hook, hook<T>>;
}

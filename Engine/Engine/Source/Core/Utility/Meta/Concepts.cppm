export module gse.utility:concepts;

import std;

import :hook;
import :component;

export namespace gse {
	template <typename T>
	concept is_component = std::derived_from<T, component_tag>;

	template <typename T>
	concept has_params = requires { typename T::params; };

	template <typename Hook, typename T>
	concept is_hook = std::derived_from<Hook, hook<T>>;
}

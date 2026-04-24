export module gse.ecs:traits;

import std;

import :access_token;
import :component;

export namespace gse {
	template <typename T>
	struct access_traits {
		static constexpr bool is_access = false;
		static constexpr bool is_const_element = false;
		using element_type = void;
	};

	template <is_component T, access_mode M>
	struct access_traits<access<T, M>> {
		static constexpr bool is_access = true;
		static constexpr bool is_const_element = (M == access_mode::read);
		using element_type = T;
	};

	template <typename T>
	constexpr bool is_access_v = access_traits<std::remove_cvref_t<T>>::is_access;

	template <typename T>
	constexpr bool is_read_access_v = access_traits<std::remove_cvref_t<T>>::is_const_element;

	template <typename T>
	using access_element_t = access_traits<std::remove_cvref_t<T>>::element_type;
}

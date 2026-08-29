export module gse.ecs:traits;

import std;

import :access_token;
import :component;

export namespace gse {
	template <typename S>
	concept names_data = requires { typename S::data; };

	template <typename S, bool = names_data<S>>
	struct state_of_helper {
		using type = S;
	};

	template <typename S>
	struct state_of_helper<S, true> {
		using type = typename S::data;
	};

	template <typename S>
	using state_of_t = typename state_of_helper<S>::type;

	template <typename T>
	struct access_traits {
		static constexpr bool is_access = false;
		static constexpr bool is_const_element = false;
		using element_type = void;
	};

	template <typename T, access_mode M>
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

	template <typename T>
	struct structural_traits {
		static constexpr bool is_structural = false;
		using element_type = void;
	};

	template <typename T>
	struct structural_traits<structural<T>> {
		static constexpr bool is_structural = true;
		using element_type = T;
	};

	template <typename T>
	constexpr bool is_structural_v = structural_traits<std::remove_cvref_t<T>>::is_structural;

	template <typename T>
	using structural_element_t = structural_traits<std::remove_cvref_t<T>>::element_type;

	template <typename T>
	constexpr bool is_entities_v = std::is_same_v<std::remove_cvref_t<T>, entities>;
}

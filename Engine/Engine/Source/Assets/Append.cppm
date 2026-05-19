export module gse.assets:append;

import std;
import gse.containers;

namespace gse::assets {
	template <typename T>
	struct as_type_pack {
		using type = type_pack<T>;
	};

	template <typename... Ts>
	struct as_type_pack<type_pack<Ts...>> {
		using type = type_pack<Ts...>;
	};

	template <typename... Args>
	struct flatten;

	template <>
	struct flatten<> {
		using type = type_pack<>;
	};

	template <typename Head, typename... Rest>
	struct flatten<Head, Rest...> {
		using type =
			typename type_pack_concat<typename as_type_pack<Head>::type, typename flatten<Rest...>::type>::type;
	};
}

export namespace gse::assets {
	template <typename... Args>
	using append = typename flatten<Args...>::type;
}

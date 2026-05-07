export module gse.containers:type_pack;

import std;

export namespace gse {
    template <typename... Ts>
    struct type_pack {
        static constexpr std::size_t size = sizeof...(Ts);

        template <template <typename...> typename Tmpl, typename... Prefix>
        using apply = Tmpl<Prefix..., Ts...>;
    };

    template <typename A, typename B>
    struct type_pack_concat;

    template <typename... As, typename... Bs>
    struct type_pack_concat<type_pack<As...>, type_pack<Bs...>> {
        using type = type_pack<As..., Bs...>;
    };
}

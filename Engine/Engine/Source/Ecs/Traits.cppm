export module gse.ecs:traits;

import std;

import gse.meta;

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

    template <typename F>
    auto system_reads(
    ) -> std::vector<std::type_index>;

    template <typename F>
    auto system_writes(
    ) -> std::vector<std::type_index>;
}

template <typename F>
auto gse::system_reads() -> std::vector<std::type_index> {
    using arg_tuple = typename lambda_traits<std::remove_cvref_t<F>>::arg_tuple;
    return [&]<typename... Args>(std::type_identity<std::tuple<Args...>>) {
        std::vector<std::type_index> result;
        ((is_access_v<Args> && is_read_access_v<Args>
            ? result.push_back(typeid(access_element_t<Args>))
            : void()), ...);
        return result;
    }(std::type_identity<arg_tuple>{});
}

template <typename F>
auto gse::system_writes() -> std::vector<std::type_index> {
    using arg_tuple = typename lambda_traits<std::remove_cvref_t<F>>::arg_tuple;
    return [&]<typename... Args>(std::type_identity<std::tuple<Args...>>) {
        std::vector<std::type_index> result;
        ((is_access_v<Args> && !is_read_access_v<Args>
            ? result.push_back(typeid(access_element_t<Args>))
            : void()), ...);
        return result;
    }(std::type_identity<arg_tuple>{});
}

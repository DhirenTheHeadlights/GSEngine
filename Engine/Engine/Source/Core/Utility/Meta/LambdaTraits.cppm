export module gse.utility:lambda_traits;

import std;

export namespace gse {
    template <typename T>
    class chunk;

    template <typename T>
    struct chunk_traits {
        static constexpr bool is_chunk = false;
        static constexpr bool is_const_element = false;
        using element_type = void;
    };

    template <typename T>
    struct chunk_traits<chunk<T>> {
        static constexpr bool is_chunk = true;
        static constexpr bool is_const_element = std::is_const_v<T>;
        using element_type = std::remove_const_t<T>;
    };

    template <typename T>
    constexpr bool is_chunk_v = chunk_traits<std::remove_cvref_t<T>>::is_chunk;

    template <typename T>
    constexpr bool is_read_chunk_v = chunk_traits<std::remove_cvref_t<T>>::is_const_element;

    template <typename T>
    using chunk_element_t = chunk_traits<std::remove_cvref_t<T>>::element_type;

    template <typename... Args>
    struct param_info {
        static auto read_types(
        ) -> std::vector<std::type_index>;

        static auto write_types(
        ) -> std::vector<std::type_index>;
    };

    template <typename T>
    struct is_write_param : std::bool_constant<!std::is_const_v<std::remove_reference_t<T>>> {};

    template <typename T>
    constexpr bool is_write_param_v = is_write_param<T>::value;

    template <typename T>
    using param_base_type = std::remove_cvref_t<T>;

    template <typename ReturnType, typename... Args>
    struct lambda_traits_base {
        using arg_tuple = std::tuple<Args...>;
        using return_type = ReturnType;
        static constexpr std::size_t arity = sizeof...(Args);

        using first_arg = std::conditional_t<
            (sizeof...(Args) > 0),
            std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>,
            void
        >;

        static auto reads(
        ) -> std::vector<std::type_index>;

        static auto writes(
        ) -> std::vector<std::type_index>;
    };

    template <typename T>
    struct lambda_traits : lambda_traits<decltype(&T::operator())> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) const> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...)> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...)&> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...)&&> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) const&> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) const noexcept> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) noexcept> : lambda_traits_base<ReturnType, Args...> {};

    template <typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(*)(Args...)> : lambda_traits_base<ReturnType, Args...> {};

    template <typename F>
    using first_arg_t = lambda_traits<std::remove_cvref_t<F>>::first_arg;
}

template <typename... Args>
auto gse::param_info<Args...>::read_types() -> std::vector<std::type_index> {
    std::vector<std::type_index> result;
    ((is_chunk_v<Args> && is_read_chunk_v<Args>
        ? result.push_back(typeid(chunk_element_t<Args>))
        : void()), ...);
    return result;
}

template <typename... Args>
auto gse::param_info<Args...>::write_types() -> std::vector<std::type_index> {
    std::vector<std::type_index> result;
    ((is_chunk_v<Args> && !is_read_chunk_v<Args>
        ? result.push_back(typeid(chunk_element_t<Args>))
        : void()), ...);
    return result;
}

template <typename ReturnType, typename... Args>
auto gse::lambda_traits_base<ReturnType, Args...>::reads() -> std::vector<std::type_index> {
    return param_info<Args...>::read_types();
}

template <typename ReturnType, typename... Args>
auto gse::lambda_traits_base<ReturnType, Args...>::writes() -> std::vector<std::type_index> {
    return param_info<Args...>::write_types();
}

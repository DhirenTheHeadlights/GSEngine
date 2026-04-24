export module gse.meta:lambda_traits;

import std;

export namespace gse {
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

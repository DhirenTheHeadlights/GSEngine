export module gse.utility:lambda_traits;

import std;

export namespace gse {
    template <typename T>
    struct lambda_traits : lambda_traits<decltype(&T::operator())>{};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) const> {
        using arg_tuple = std::tuple<Args...>;
        static_assert(std::tuple_size_v<arg_tuple> >= 1, "lambda_traits: need at least one arg");
        using first_arg = std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>;
    };

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...)> {
        using arg_tuple = std::tuple<Args...>;
        static_assert(std::tuple_size_v<arg_tuple> >= 1, "lambda_traits: need at least one arg");
        using first_arg = std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>;
    };

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...)&> {
        using arg_tuple = std::tuple<Args...>;
        static_assert(std::tuple_size_v<arg_tuple> >= 1, "lambda_traits: need at least one arg");
        using first_arg = std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>;
    };

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...)&&> {
        using arg_tuple = std::tuple<Args...>;
        static_assert(std::tuple_size_v<arg_tuple> >= 1, "lambda_traits: need at least one arg");
        using first_arg = std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>;
    };

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) const&> {
        using arg_tuple = std::tuple<Args...>;
        static_assert(std::tuple_size_v<arg_tuple> >= 1, "lambda_traits: need at least one arg");
        using first_arg = std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>;
    };

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) const noexcept> : lambda_traits<ReturnType(ClassType::*)(Args...) const> {};

    template <typename ClassType, typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(ClassType::*)(Args...) noexcept> : lambda_traits<ReturnType(ClassType::*)(Args...)> {};

    template <typename ReturnType, typename... Args>
    struct lambda_traits<ReturnType(*)(Args...)> {
        using arg_tuple = std::tuple<Args...>;
        static_assert(std::tuple_size_v<arg_tuple> >= 1, "lambda_traits: need at least one arg");
        using first_arg = std::remove_cvref_t<std::tuple_element_t<0, arg_tuple>>;
    };

    template <typename F>
    using first_arg_t = lambda_traits<std::remove_cvref_t<std::remove_reference_t<F>>>::first_arg;
}

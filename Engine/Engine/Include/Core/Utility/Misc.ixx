export module gse.utility:misc;

import std;

namespace gse {
    template<typename T>
    decltype(auto) de_ref(T&& val) {
        if constexpr (std::is_pointer_v<std::decay_t<T>>) {
            return *val;
        }
        else if constexpr (
            requires { val.operator*(); }
            ) {
            return *val;
        }
        else {
            return std::forward<T>(val);
        }
    }

}

export namespace gse {
    template <
        typename Container,
        typename T = std::remove_reference_t<decltype(**std::begin(std::declval<Container&>()))>,
        typename Ret,
        typename... Args
    >
    auto bulk_invoke(Container&& container, Ret(T::* func)(Args...) const, Args&&... args) -> void {
        for (auto&& obj : container) {
            std::invoke(func, de_ref(obj), std::forward<Args>(args)...);
        }
    }

    template <
        typename Container,
        typename T = std::remove_reference_t<decltype(**std::begin(std::declval<Container&>()))>,
        typename Ret,
        typename... Args
    >
    auto bulk_invoke(Container&& container, Ret(T::* func)(Args...), Args&&... args) -> void {
        for (auto&& obj : container) {
            std::invoke(func, de_ref(obj), std::forward<Args>(args)...);
        }
    }
}
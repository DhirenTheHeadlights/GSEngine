export module gse.utility:misc;

import std;

export namespace gse {
    template <
        typename Container,
        typename T = std::remove_reference_t<decltype(**std::begin(std::declval<Container&>()))>,
        typename Ret,
        typename... Args
    >
    auto bulk_invoke(Container&& container, Ret(T::* func)(Args...) const, Args&&... args) -> void {
        for (auto&& obj : container) {
            std::invoke(func, obj, std::forward<Args>(args)...);
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
            std::invoke(func, obj, std::forward<Args>(args)...);
        }
    }
}
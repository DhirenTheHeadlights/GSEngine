export module gse.utility;

export import :clock;
export import :component;
export import :config;
export import :hook;
export import :id;
export import :json;
export import :registry;
export import :timed_lock;
export import :timer;
export import :non_copyable;

export namespace gse {
    template <
        typename Container,
        typename T = std::remove_reference_t<decltype(*std::begin(std::declval<Container&>()))>,
        typename Ret,
        typename... Args
    >
    auto bulk_invoke(Container&& container, Ret (T::*func)(Args...) const, Args&&... args) -> void {
        for (auto&& obj : container) {
            (obj.*func)(std::forward<Args>(args)...);
        }
    }
}
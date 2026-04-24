export module gse.core:move_only_function;

import std;

import :non_copyable;

export namespace gse {
    template <typename Sig>
    class move_only_function;

    template <typename R, typename... Args>
    class move_only_function<R(Args...)> : public non_copyable {
    public:
        move_only_function() noexcept = default;

        ~move_only_function() override;

        move_only_function(
            move_only_function&&
        ) noexcept = default;

        auto operator=(
            move_only_function&&
        ) noexcept -> move_only_function& = default;

        move_only_function(
            std::nullptr_t
        ) noexcept;

        template <typename F>
            requires (!std::same_as<std::remove_cvref_t<F>, move_only_function<R(Args...)>> && std::invocable<std::decay_t<F>&, Args...>)
        move_only_function(F&& f) : m_holder(std::make_unique<holder<std::decay_t<F>>>(std::forward<F>(f))) {}

        explicit operator bool(
        ) const noexcept;

        auto operator()(
            Args... args
        ) -> R;

    private:
        struct holder_base {
            virtual ~holder_base() = default;

            virtual auto invoke(
                Args... args
            ) -> R = 0;
        };

        template <typename F>
        struct holder final : holder_base {
            F m_fn;

            template <typename G>
            explicit holder(G&& g) : m_fn(std::forward<G>(g)) {}

            auto invoke(Args... args) -> R override {
                return std::invoke(m_fn, std::forward<Args>(args)...);
            }
        };

        std::unique_ptr<holder_base> m_holder;
    };
}

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::~move_only_function() = default;

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::move_only_function(std::nullptr_t) noexcept {}

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::operator bool() const noexcept {
    return m_holder != nullptr;
}

template <typename R, typename... Args>
auto gse::move_only_function<R(Args...)>::operator()(Args... args) -> R {
    return m_holder->invoke(std::forward<Args>(args)...);
}

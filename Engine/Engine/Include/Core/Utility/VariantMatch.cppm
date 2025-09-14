export module gse.utility:variant_match;

import std;

export namespace gse {
    template <class Variant>
    class variant {
    public:
        using var_t = std::remove_reference_t<Variant>;

        explicit variant(
            var_t& v
        );

        template <class T, class F>
        auto if_is(
            F&& f
        ) & ->variant&;

        template <class T, class F>
        auto else_if_is(
            F&& f
        ) & ->variant&;

        template <class F>
        auto otherwise(
            F&& f
        ) & -> void;

        template <class T, class F>
        auto if_is(
            F&& f
        ) && ->variant&&;

        template <class T, class F>
        auto else_if_is(
            F&& f
        ) && ->variant&&;

        template <class F>
        auto otherwise(
            F&& f
        ) && -> void;
    private:
        var_t* m_ptr;
        bool m_handled = false;
    };

    template <typename... Ts>
    auto match(
        std::variant<Ts...>& v
    ) -> variant<std::variant<Ts...>&>;

    template <typename... Ts>
    auto match(
        const std::variant<Ts...>& v
    ) -> variant<const std::variant<Ts...>&>;
}

template <class Variant>
gse::variant<Variant>::variant(var_t& v)
    : m_ptr(std::addressof(v)) {
}

template <class Variant>
template <class T, class F>
auto gse::variant<Variant>::if_is(F&& f) & -> variant& {
    if (!m_handled) {
        if (auto* p = std::get_if<T>(m_ptr)) {
            std::invoke(std::forward<F>(f), *p);
            m_handled = true;
        }
    }
    return *this;
}

template <class Variant>
template <class T, class F>
auto gse::variant<Variant>::else_if_is(F&& f) & -> variant& {
    return this->template if_is<T>(std::forward<F>(f));
}

template <class Variant>
template <class F>
auto gse::variant<Variant>::otherwise(F&& f) & -> void {
    if (!m_handled) {
        if constexpr (std::is_invocable_v<F&, decltype(*m_ptr)>) {
            std::invoke(std::forward<F>(f), *m_ptr);
        }
        else {
            std::invoke(std::forward<F>(f));
        }
    }
}

template <class Variant>
template <class T, class F>
auto gse::variant<Variant>::if_is(F&& f) && -> variant&& {
    static_cast<variant&>(*this).template if_is<T>(std::forward<F>(f));
    return std::move(*this);
}

template <class Variant>
template <class T, class F>
auto gse::variant<Variant>::else_if_is(F&& f) && -> variant&& {
    static_cast<variant&>(*this).template else_if_is<T>(std::forward<F>(f));
    return std::move(*this);
}

template <class Variant>
template <class F>
auto gse::variant<Variant>::otherwise(F&& f) && -> void {
    static_cast<variant&>(*this).otherwise(std::forward<F>(f));
}

template <typename ... Ts>
auto gse::match(std::variant<Ts...>& v) -> variant<std::variant<Ts...>&> {
    return variant<std::variant<Ts...>&>(v);
}

template <typename ... Ts>
auto gse::match(const std::variant<Ts...>& v) -> variant<const std::variant<Ts...>&> {
    return variant<const std::variant<Ts...>&>(v);
}

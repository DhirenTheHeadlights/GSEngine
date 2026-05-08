export module gse.core:move_only_function;

import std;
import gse.assert;

export namespace gse {
    template <typename Sig>
    class move_only_function;

    template <typename R, typename... Args>
    class move_only_function<R(Args...)> {
    public:
        move_only_function(
        ) noexcept = default;

        ~move_only_function();

        move_only_function(
            const move_only_function&
        ) = delete;

        auto operator=(
            const move_only_function&
        ) -> move_only_function& = delete;

        move_only_function(
            move_only_function&& other
        ) noexcept;

        auto operator=(
            move_only_function&& other
        ) noexcept -> move_only_function&;

        move_only_function(
            std::nullptr_t
        ) noexcept;

        template <typename F>
            requires (!std::same_as<std::remove_cvref_t<F>, move_only_function<R(Args...)>> && std::invocable<std::decay_t<F>&, Args...>)
        move_only_function(F&& f) {
            using D = std::decay_t<F>;
            if constexpr (fits_inline_v<D>) {
                std::construct_at(reinterpret_cast<D*>(m_buffer), std::forward<F>(f));
                m_vtable = &inline_vtable_of<D>();
            }
            else {
                auto* heap = new D(std::forward<F>(f));
                std::construct_at(reinterpret_cast<D**>(m_buffer), heap);
                m_vtable = &heap_vtable_of<D>();
            }
        }

        explicit operator bool(
        ) const noexcept;

        auto operator()(
            Args... args
        ) -> R;

    private:
        static constexpr std::size_t buffer_size = 3 * sizeof(void*);
        static constexpr std::size_t buffer_align = alignof(void*);

        struct vtable {
            R (*invoke)(void*, Args...);
            void (*move_init)(void* src, void* dst) noexcept;
            void (*destroy)(void*) noexcept;
        };

        template <typename F>
        static constexpr bool fits_inline_v = sizeof(F) <= buffer_size
                                           && alignof(F) <= buffer_align
                                           && std::is_nothrow_move_constructible_v<F>;

        template <typename F>
        static auto inline_invoke(void* p, Args... args) -> R {
            return std::invoke(*static_cast<F*>(p), std::forward<Args>(args)...);
        }

        template <typename F>
        static auto inline_move_init(void* src, void* dst) noexcept -> void {
            std::construct_at(static_cast<F*>(dst), std::move(*static_cast<F*>(src)));
            std::destroy_at(static_cast<F*>(src));
        }

        template <typename F>
        static auto inline_destroy(void* p) noexcept -> void {
            std::destroy_at(static_cast<F*>(p));
        }

        template <typename F>
        static auto heap_invoke(void* p, Args... args) -> R {
            return std::invoke(**static_cast<F**>(p), std::forward<Args>(args)...);
        }

        template <typename F>
        static auto heap_move_init(void* src, void* dst) noexcept -> void {
            *static_cast<F**>(dst) = *static_cast<F**>(src);
            *static_cast<F**>(src) = nullptr;
        }

        template <typename F>
        static auto heap_destroy(void* p) noexcept -> void {
            delete *static_cast<F**>(p);
        }

        template <typename F>
        static auto inline_vtable_of() -> const vtable& {
            static const vtable v = {
                .invoke = &inline_invoke<F>,
                .move_init = &inline_move_init<F>,
                .destroy = &inline_destroy<F>,
            };
            return v;
        }

        template <typename F>
        static auto heap_vtable_of() -> const vtable& {
            static const vtable v = {
                .invoke = &heap_invoke<F>,
                .move_init = &heap_move_init<F>,
                .destroy = &heap_destroy<F>,
            };
            return v;
        }

        const vtable* m_vtable = nullptr;
        alignas(buffer_align) std::byte m_buffer[buffer_size]{};
    };
}

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::~move_only_function() {
    if (m_vtable) {
        m_vtable->destroy(m_buffer);
    }
}

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::move_only_function(move_only_function&& other) noexcept : m_vtable(other.m_vtable) {
    if (m_vtable) {
        m_vtable->move_init(other.m_buffer, m_buffer);
        other.m_vtable = nullptr;
    }
}

template <typename R, typename... Args>
auto gse::move_only_function<R(Args...)>::operator=(move_only_function&& other) noexcept -> move_only_function& {
    if (this == &other) {
        return *this;
    }
    if (m_vtable) {
        m_vtable->destroy(m_buffer);
        m_vtable = nullptr;
    }
    if (other.m_vtable) {
        other.m_vtable->move_init(other.m_buffer, m_buffer);
        m_vtable = other.m_vtable;
        other.m_vtable = nullptr;
    }
    return *this;
}

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::move_only_function(std::nullptr_t) noexcept {}

template <typename R, typename... Args>
gse::move_only_function<R(Args...)>::operator bool() const noexcept {
    return m_vtable != nullptr;
}

template <typename R, typename... Args>
auto gse::move_only_function<R(Args...)>::operator()(Args... args) -> R {
    gse::assert(m_vtable != nullptr, "move_only_function invoked with null vtable (moved-from or default-constructed)");
    return m_vtable->invoke(m_buffer, std::forward<Args>(args)...);
}

export module gse.utility:static_vector;

import std;
import gse.assert;

export namespace gse {
    template <typename T, std::size_t N>
    class static_vector {
    public:
        using value_type      = T;
        using size_type       = std::size_t;
        using reference       = T&;
        using const_reference = const T&;
        using iterator        = T*;
        using const_iterator  = const T*;

        static_vector() = default;

        ~static_vector() { clear(); }

        static_vector(const static_vector& other) {
            for (size_type i = 0; i < other.m_size; ++i) {
                emplace_back(other[i]);
            }
        }

        auto operator=(const static_vector& other) -> static_vector& {
            if (this != &other) {
                clear();
                for (size_type i = 0; i < other.m_size; ++i) {
                    emplace_back(other[i]);
                }
            }
            return *this;
        }

        static_vector(static_vector&& other) noexcept {
            for (size_type i = 0; i < other.m_size; ++i) {
                emplace_back(std::move(other[i]));
            }
            other.clear();
        }

        auto operator=(static_vector&& other) noexcept -> static_vector& {
            if (this != &other) {
                clear();
                for (size_type i = 0; i < other.m_size; ++i) {
                    emplace_back(std::move(other[i]));
                }
                other.clear();
            }
            return *this;
        }

        auto push_back(const T& value) -> reference {
            gse::assert(m_size < N, std::source_location::current(), "static_vector overflow (capacity: {})", N);
            new (slot(m_size)) T(value);
            return *slot(m_size++);
        }

        auto push_back(T&& value) -> reference {
            gse::assert(m_size < N, std::source_location::current(), "static_vector overflow (capacity: {})", N);
            new (slot(m_size)) T(std::move(value));
            return *slot(m_size++);
        }

        template <typename... Args>
        auto emplace_back(Args&&... args) -> reference {
            gse::assert(m_size < N, std::source_location::current(), "static_vector overflow (capacity: {})", N);
            new (slot(m_size)) T(std::forward<Args>(args)...);
            return *slot(m_size++);
        }

        auto pop_back() -> void {
            gse::assert(m_size > 0, std::source_location::current(), "pop_back called on empty static_vector");
            slot(--m_size)->~T();
        }

        auto clear() -> void {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = 0; i < m_size; ++i) {
                    slot(i)->~T();
                }
            }
            m_size = 0;
        }

        [[nodiscard]] auto size() const -> size_type                     { return m_size; }
        [[nodiscard]] static constexpr auto capacity() -> size_type      { return N; }
        [[nodiscard]] auto empty() const -> bool                         { return m_size == 0; }
        [[nodiscard]] auto full() const -> bool                          { return m_size == N; }
        [[nodiscard]] auto data() -> T*                                  { return slot(0); }
        [[nodiscard]] auto data() const -> const T*                      { return slot(0); }

        [[nodiscard]] auto begin() -> iterator             { return slot(0); }
        [[nodiscard]] auto end() -> iterator               { return slot(m_size); }
        [[nodiscard]] auto begin() const -> const_iterator { return slot(0); }
        [[nodiscard]] auto end() const -> const_iterator   { return slot(m_size); }

        auto operator[](const size_type i) -> reference             { return *slot(i); }
        auto operator[](const size_type i) const -> const_reference { return *slot(i); }

        [[nodiscard]] auto span() -> std::span<T>             { return { slot(0), m_size }; }
        [[nodiscard]] auto span() const -> std::span<const T> { return { slot(0), m_size }; }

    private:
        auto slot(const size_type i) -> T* {
            return std::launder(reinterpret_cast<T*>(&m_storage[i * sizeof(T)]));
        }

        auto slot(const size_type i) const -> const T* {
            return std::launder(reinterpret_cast<const T*>(&m_storage[i * sizeof(T)]));
        }

        alignas(T) std::byte m_storage[sizeof(T) * N]{};
        size_type m_size = 0;
    };
}

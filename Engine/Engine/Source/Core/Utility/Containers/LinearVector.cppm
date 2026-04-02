export module gse.utility:linear_vector;

import std;
import gse.assert;

export namespace gse {
    template <typename T>
    class linear_vector {
    public:
        using value_type      = T;
        using size_type       = std::size_t;
        using reference       = T&;
        using const_reference = const T&;
        using iterator        = T*;
        using const_iterator  = const T*;

        linear_vector() = default;

        explicit linear_vector(const size_type capacity)
            : m_data(static_cast<T*>(::operator new(capacity * sizeof(T), std::align_val_t{ alignof(T) })))
            , m_capacity(capacity) {}

        ~linear_vector() {
            clear();
            ::operator delete(m_data, std::align_val_t{ alignof(T) });
        }

        linear_vector(linear_vector&& other) noexcept
            : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
            other.m_data     = nullptr;
            other.m_size     = 0;
            other.m_capacity = 0;
        }

        auto operator=(linear_vector&& other) noexcept -> linear_vector& {
            if (this != &other) {
                clear();
                ::operator delete(m_data, std::align_val_t{ alignof(T) });
                m_data           = other.m_data;
                m_size           = other.m_size;
                m_capacity       = other.m_capacity;
                other.m_data     = nullptr;
                other.m_size     = 0;
                other.m_capacity = 0;
            }
            return *this;
        }

        linear_vector(const linear_vector&)                    = delete;
        auto operator=(const linear_vector&) -> linear_vector& = delete;

        auto push_back(const T& value) -> reference {
            gse::assert(m_size < m_capacity, std::source_location::current(), "linear_vector overflow (capacity: {})", m_capacity);
            new (m_data + m_size) T(value);
            return m_data[m_size++];
        }

        auto push_back(T&& value) -> reference {
            gse::assert(m_size < m_capacity, std::source_location::current(), "linear_vector overflow (capacity: {})", m_capacity);
            new (m_data + m_size) T(std::move(value));
            return m_data[m_size++];
        }

        template <typename... Args>
        auto emplace_back(Args&&... args) -> reference {
            gse::assert(m_size < m_capacity, std::source_location::current(), "linear_vector overflow (capacity: {})", m_capacity);
            new (m_data + m_size) T(std::forward<Args>(args)...);
            return m_data[m_size++];
        }

        auto clear() -> void {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = 0; i < m_size; ++i) {
                    m_data[i].~T();
                }
            }
            m_size = 0;
        }

        [[nodiscard]] auto size() const -> size_type     { return m_size; }
        [[nodiscard]] auto capacity() const -> size_type { return m_capacity; }
        [[nodiscard]] auto empty() const -> bool         { return m_size == 0; }
        [[nodiscard]] auto data() -> T*                  { return m_data; }
        [[nodiscard]] auto data() const -> const T*      { return m_data; }

        [[nodiscard]] auto begin() -> iterator             { return m_data; }
        [[nodiscard]] auto end() -> iterator               { return m_data + m_size; }
        [[nodiscard]] auto begin() const -> const_iterator { return m_data; }
        [[nodiscard]] auto end() const -> const_iterator   { return m_data + m_size; }

        auto operator[](const size_type i) -> reference             { return m_data[i]; }
        auto operator[](const size_type i) const -> const_reference { return m_data[i]; }

        [[nodiscard]] auto span() -> std::span<T>             { return { m_data, m_size }; }
        [[nodiscard]] auto span() const -> std::span<const T> { return { m_data, m_size }; }

    private:
        T*        m_data     = nullptr;
        size_type m_size     = 0;
        size_type m_capacity = 0;
    };
}

export module gse.containers:linear_vector;

import std;
import gse.assert;
import gse.core;

export namespace gse {
    template <typename T>
    class linear_vector : public non_copyable {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using const_reference = const T&;
        using iterator = T*;
        using const_iterator = const T*;

        linear_vector() = default;

        explicit linear_vector(
            size_type capacity
        );

        ~linear_vector() override;

        linear_vector(
            linear_vector&& other
        ) noexcept;

        auto operator=(
            linear_vector&& other
        ) noexcept -> linear_vector&;

        auto push_back(
            const T& value
        ) -> reference;

        auto push_back(
            T&& value
        ) -> reference;

        template <typename... Args>
        auto emplace_back(
            Args&&... args
        ) -> reference;

        auto reserve(
            size_type new_capacity
        ) -> void;

        auto resize(
            size_type n
        ) -> void;

        auto resize(
            size_type n,
            const T& val
        ) -> void;

        auto assign(
            size_type n,
            const T& val
        ) -> void;

        template <std::input_iterator InputIt>
        auto assign(
            InputIt first,
            InputIt last
        ) -> void;

        auto pop_back(
        ) -> void;

        auto clear(
        ) -> void;

        [[nodiscard]] auto size(
        ) const -> size_type;

        [[nodiscard]] auto capacity(
        ) const -> size_type;

        [[nodiscard]] auto empty(
        ) const -> bool;

        template <typename Self>
        [[nodiscard]] auto data(
            this Self& self
        ) -> decltype(auto);

        template <typename Self>
        [[nodiscard]] auto begin(
            this Self& self
        ) -> decltype(auto);

        template <typename Self>
        [[nodiscard]] auto end(
            this Self& self
        ) -> decltype(auto);

        template <typename Self>
        auto operator[](
            this Self& self,
            size_type i
        ) -> decltype(auto);

        template <typename Self>
        [[nodiscard]] auto span(
            this Self& self
        ) -> decltype(auto);

    private:
        T* m_data = nullptr;
        size_type m_size = 0;
        size_type m_capacity = 0;
    };
}

template <typename T>
gse::linear_vector<T>::linear_vector(const size_type capacity)
    : m_data(static_cast<T*>(operator new(capacity * sizeof(T), std::align_val_t{ alignof(T) })))
    , m_capacity(capacity) {
}

template <typename T>
gse::linear_vector<T>::~linear_vector() {
    clear();
    ::operator delete(m_data, std::align_val_t{ alignof(T) });
}

template <typename T>
gse::linear_vector<T>::linear_vector(linear_vector&& other) noexcept
    : m_data(other.m_data)
    , m_size(other.m_size)
    , m_capacity(other.m_capacity) {
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
}

template <typename T>
auto gse::linear_vector<T>::operator=(linear_vector&& other) noexcept -> linear_vector& {
    if (this != &other) {
        clear();
        ::operator delete(m_data, std::align_val_t{ alignof(T) });
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }
    return *this;
}

template <typename T>
auto gse::linear_vector<T>::push_back(const T& value) -> reference {
    assert(m_size < m_capacity, std::source_location::current(), "linear_vector overflow (capacity: {})", m_capacity);
    new (m_data + m_size) T(value);
    return m_data[m_size++];
}

template <typename T>
auto gse::linear_vector<T>::push_back(T&& value) -> reference {
    assert(m_size < m_capacity, std::source_location::current(), "linear_vector overflow (capacity: {})", m_capacity);
    new (m_data + m_size) T(std::move(value));
    return m_data[m_size++];
}

template <typename T>
template <typename... Args>
auto gse::linear_vector<T>::emplace_back(Args&&... args) -> reference {
    assert(m_size < m_capacity, std::source_location::current(), "linear_vector overflow (capacity: {})", m_capacity);
    new (m_data + m_size) T(std::forward<Args>(args)...);
    return m_data[m_size++];
}

template <typename T>
auto gse::linear_vector<T>::reserve(const size_type new_capacity) -> void {
    if (new_capacity <= m_capacity) {
        return;
    }
    auto* new_data = static_cast<T*>(operator new(new_capacity * sizeof(T), std::align_val_t{ alignof(T) }));
    if (m_data && m_size > 0) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(new_data, m_data, m_size * sizeof(T));
        }
        else {
            for (size_type i = 0; i < m_size; ++i) {
                new (new_data + i) T(std::move(m_data[i]));
                m_data[i].~T();
            }
        }
    }
    ::operator delete(m_data, std::align_val_t{ alignof(T) });
    m_data = new_data;
    m_capacity = new_capacity;
}

template <typename T>
auto gse::linear_vector<T>::resize(const size_type n) -> void {
    assert(n <= m_capacity, std::source_location::current(), "linear_vector::resize exceeded capacity ({})", m_capacity);
    if (n > m_size) {
        if constexpr (!std::is_trivially_constructible_v<T>) {
            for (size_type i = m_size; i < n; ++i) {
                new (m_data + i) T();
            }
        }
    }
    else if (n < m_size) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = n; i < m_size; ++i) {
                m_data[i].~T();
            }
        }
    }
    m_size = n;
}

template <typename T>
auto gse::linear_vector<T>::resize(const size_type n, const T& val) -> void {
    assert(n <= m_capacity, std::source_location::current(), "linear_vector::resize exceeded capacity ({})", m_capacity);
    if (n > m_size) {
        for (size_type i = m_size; i < n; ++i) {
            new (m_data + i) T(val);
        }
    }
    else if (n < m_size) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = n; i < m_size; ++i) {
                m_data[i].~T();
            }
        }
    }
    m_size = n;
}

template <typename T>
auto gse::linear_vector<T>::assign(const size_type n, const T& val) -> void {
    assert(n <= m_capacity, std::source_location::current(), "linear_vector::assign exceeded capacity ({})", m_capacity);
    clear();
    for (size_type i = 0; i < n; ++i) {
        new (m_data + i) T(val);
    }
    m_size = n;
}

template <typename T>
template <std::input_iterator InputIt>
auto gse::linear_vector<T>::assign(InputIt first, const InputIt last) -> void {
    clear();
    for (; first != last; ++first) {
        assert(m_size < m_capacity, std::source_location::current(), "linear_vector::assign exceeded capacity ({})", m_capacity);
        new (m_data + m_size) T(*first);
        ++m_size;
    }
}

template <typename T>
auto gse::linear_vector<T>::pop_back() -> void {
    assert(m_size > 0, std::source_location::current(), "linear_vector::pop_back on empty vector");
    if constexpr (!std::is_trivially_destructible_v<T>) {
        m_data[m_size - 1].~T();
    }
    --m_size;
}

template <typename T>
auto gse::linear_vector<T>::clear() -> void {
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = 0; i < m_size; ++i) {
            m_data[i].~T();
        }
    }
    m_size = 0;
}

template <typename T>
auto gse::linear_vector<T>::size() const -> size_type {
    return m_size;
}

template <typename T>
auto gse::linear_vector<T>::capacity() const -> size_type {
    return m_capacity;
}

template <typename T>
auto gse::linear_vector<T>::empty() const -> bool {
    return m_size == 0;
}

template <typename T>
template <typename Self>
auto gse::linear_vector<T>::data(this Self& self) -> decltype(auto) {
    using ptr_t = std::conditional_t<std::is_const_v<Self>, const T*, T*>;
    return static_cast<ptr_t>(self.m_data);
}

template <typename T>
template <typename Self>
auto gse::linear_vector<T>::begin(this Self& self) -> decltype(auto) {
    return self.data();
}

template <typename T>
template <typename Self>
auto gse::linear_vector<T>::end(this Self& self) -> decltype(auto) {
    return self.data() + self.m_size;
}

template <typename T>
template <typename Self>
auto gse::linear_vector<T>::operator[](this Self& self, const size_type i) -> decltype(auto) {
    return self.data()[i];
}

template <typename T>
template <typename Self>
auto gse::linear_vector<T>::span(this Self& self) -> decltype(auto) {
    return std::span{ self.data(), self.m_size };
}

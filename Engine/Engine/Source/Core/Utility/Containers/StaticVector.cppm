export module gse.utility:static_vector;

import std;
import gse.assert;

export namespace gse {
    template <typename T, std::size_t N>
    class static_vector {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using const_reference = const T&;
        using iterator = T*;
        using const_iterator = const T*;

        static_vector() = default;

        ~static_vector();

        static_vector(
            const static_vector& other
        );

        auto operator=(
            const static_vector& other
        ) -> static_vector&;

        static_vector(
            static_vector&& other
        ) noexcept;

        auto operator=(
            static_vector&& other
        ) noexcept -> static_vector&;

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

        auto pop_back(
        ) -> void;

        auto clear(
        ) -> void;

        [[nodiscard]] auto size(
        ) const -> size_type;

        [[nodiscard]] static constexpr auto capacity(
        ) -> size_type;

        [[nodiscard]] auto empty(
        ) const -> bool;

        [[nodiscard]] auto full(
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
        [[nodiscard]] auto back(
            this Self& self
        ) -> decltype(auto);

        template <typename Self>
        [[nodiscard]] auto span(
            this Self& self
        ) -> decltype(auto);

    private:
        template <typename Self>
        auto slot(
            this Self& self,
            size_type i
        ) -> decltype(auto);

        alignas(T) std::byte m_storage[sizeof(T) * N]{};
        size_type m_size = 0;
    };
}

template <typename T, std::size_t N>
gse::static_vector<T, N>::~static_vector() {
    clear();
}

template <typename T, std::size_t N>
gse::static_vector<T, N>::static_vector(const static_vector& other) {
    for (size_type i = 0; i < other.m_size; ++i) {
        emplace_back(other[i]);
    }
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::operator=(const static_vector& other) -> static_vector& {
    if (this != &other) {
        clear();
        for (size_type i = 0; i < other.m_size; ++i) {
            emplace_back(other[i]);
        }
    }
    return *this;
}

template <typename T, std::size_t N>
gse::static_vector<T, N>::static_vector(static_vector&& other) noexcept {
    for (size_type i = 0; i < other.m_size; ++i) {
        emplace_back(std::move(other[i]));
    }
    other.clear();
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::operator=(static_vector&& other) noexcept -> static_vector& {
    if (this != &other) {
        clear();
        for (size_type i = 0; i < other.m_size; ++i) {
            emplace_back(std::move(other[i]));
        }
        other.clear();
    }
    return *this;
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::push_back(const T& value) -> reference {
    gse::assert(m_size < N, std::source_location::current(), "static_vector overflow (capacity: {})", N);
    new (slot(m_size)) T(value);
    return *slot(m_size++);
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::push_back(T&& value) -> reference {
    gse::assert(m_size < N, std::source_location::current(), "static_vector overflow (capacity: {})", N);
    new (slot(m_size)) T(std::move(value));
    return *slot(m_size++);
}

template <typename T, std::size_t N>
template <typename... Args>
auto gse::static_vector<T, N>::emplace_back(Args&&... args) -> reference {
    gse::assert(m_size < N, std::source_location::current(), "static_vector overflow (capacity: {})", N);
    new (slot(m_size)) T(std::forward<Args>(args)...);
    return *slot(m_size++);
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::pop_back() -> void {
    gse::assert(m_size > 0, std::source_location::current(), "pop_back called on empty static_vector");
    slot(--m_size)->~T();
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::clear() -> void {
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = 0; i < m_size; ++i) {
            slot(i)->~T();
        }
    }
    m_size = 0;
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::size() const -> size_type {
    return m_size;
}

template <typename T, std::size_t N>
constexpr auto gse::static_vector<T, N>::capacity() -> size_type {
    return N;
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::empty() const -> bool {
    return m_size == 0;
}

template <typename T, std::size_t N>
auto gse::static_vector<T, N>::full() const -> bool {
    return m_size == N;
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::data(this Self& self) -> decltype(auto) {
    return self.slot(0);
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::begin(this Self& self) -> decltype(auto) {
    return self.slot(0);
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::end(this Self& self) -> decltype(auto) {
    return self.slot(self.m_size);
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::operator[](this Self& self, const size_type i) -> decltype(auto) {
    return *self.slot(i);
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::back(this Self& self) -> decltype(auto) {
    gse::assert(self.m_size > 0, std::source_location::current(), "back called on empty static_vector");
    return *self.slot(self.m_size - 1);
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::span(this Self& self) -> decltype(auto) {
    return std::span{ self.slot(0), self.m_size };
}

template <typename T, std::size_t N>
template <typename Self>
auto gse::static_vector<T, N>::slot(this Self& self, const size_type i) -> decltype(auto) {
    using ptr_t = std::conditional_t<std::is_const_v<Self>, const T*, T*>;
    return std::launder(reinterpret_cast<ptr_t>(&self.m_storage[i * sizeof(T)]));
}

module gse.physics.math.vec;

import std;

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator==(const Derived& other) const -> bool {
    auto const& self = *static_cast<const Derived*>(this);
    for (int i = 0; i < N; ++i) {
        if (self.data[i] != other.data[i]) {
            return false;
        }
    }
    return true;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator!=(const Derived& other) const -> bool {
    return !(*this == other);
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator+(const Derived& other) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] + other.data[i];
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator-(const Derived& other) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] - other.data[i];
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator*(const T scalar) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] * scalar;
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator/(const T scalar) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] / scalar;
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator+=(const Derived& other) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] += other.data[i];
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator-=(const Derived& other) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] -= other.data[i];
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator*=(const T scalar) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] *= scalar;
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator/=(const T scalar) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] /= scalar;
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator-() const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = -self.data[i];
    }
    return result;
}

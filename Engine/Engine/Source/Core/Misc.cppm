export module gse.core:misc;

import std;

namespace gse {
    template <typename T>
    auto de_ref(
        T&& val
    ) -> decltype(auto);
}

export namespace gse {
    template <
        typename Container,
        typename T = std::remove_reference_t<decltype(**std::begin(std::declval<Container&>()))>,
        typename Ret,
        typename... Args
    >
    auto bulk_invoke(
        Container&& container,
        Ret(T::* func)(Args...) const,
        Args&&... args
    ) -> void;

    template <
        typename Container,
        typename T = std::remove_reference_t<decltype(**std::begin(std::declval<Container&>()))>,
        typename Ret,
        typename... Args
    >
    auto bulk_invoke(
        Container&& container,
        Ret(T::* func)(Args...),
        Args&&... args
    ) -> void;

    template <typename T>
    concept is_trivially_copyable = std::is_trivially_copyable_v<T>;

    template <typename T>
    concept contiguous_byte_source = requires(const T& c) {
        std::ranges::data(c);
        std::ranges::size(c);
    };

    auto scope(
        const std::function<void()>& in_scope
    ) -> void;

    template <is_trivially_copyable... Src>
    auto memcpy(
        std::byte* dest,
        const Src&... src
    ) -> void requires (!std::is_pointer_v<Src> && ...);

    template <contiguous_byte_source Container>
    auto memcpy(
        std::byte* dest,
        const Container& src
    ) -> void;

    template <is_trivially_copyable T>
    auto memcpy(
        T& dest,
        const std::byte* src
    ) -> void;

    template <is_trivially_copyable T, std::size_t N>
    auto memcpy(
        std::byte* dest,
        const T (&src)[N]
    ) -> void;

    auto memcpy(
        std::byte* dest,
        const void* src,
        std::size_t size
    ) -> void;
}

template <typename T>
auto gse::de_ref(T&& val) -> decltype(auto) {
    if constexpr (std::is_pointer_v<std::decay_t<T>>) {
        return *val;
    }
    else if constexpr (requires { val.operator*(); }) {
        return *val;
    }
    else {
        return std::forward<T>(val);
    }
}

template <
    typename Container,
    typename T,
    typename Ret,
    typename... Args
>
auto gse::bulk_invoke(Container&& container, Ret(T::* func)(Args...) const, Args&&... args) -> void {
    for (auto&& obj : container) {
        std::invoke(func, de_ref(obj), std::forward<Args>(args)...);
    }
}

template <
    typename Container,
    typename T,
    typename Ret,
    typename... Args
>
auto gse::bulk_invoke(Container&& container, Ret(T::* func)(Args...), Args&&... args) -> void {
    for (auto&& obj : container) {
        std::invoke(func, de_ref(obj), std::forward<Args>(args)...);
    }
}

auto gse::scope(const std::function<void()>& in_scope) -> void {
    in_scope();
}

template <gse::is_trivially_copyable... Src>
auto gse::memcpy(std::byte* dest, const Src&... src) -> void requires (!std::is_pointer_v<Src> && ...) {
    std::byte* out = dest;
    ((std::memcpy(out, std::addressof(src), sizeof(Src)), out += sizeof(Src)), ...);
}

template <gse::contiguous_byte_source Container>
auto gse::memcpy(std::byte* dest, const Container& src) -> void {
    using value_type = std::remove_cvref_t<decltype(*std::ranges::data(src))>;
    const std::size_t byte_size = std::ranges::size(src) * sizeof(value_type);
    std::memcpy(dest, std::ranges::data(src), byte_size);
}

template <gse::is_trivially_copyable T>
auto gse::memcpy(T& dest, const std::byte* src) -> void {
    std::memcpy(std::addressof(dest), src, sizeof(T));
}

template <gse::is_trivially_copyable T, std::size_t N>
auto gse::memcpy(std::byte* dest, const T (&src)[N]) -> void {
    std::memcpy(dest, src, sizeof(T) * N);
}

auto gse::memcpy(std::byte* dest, const void* src, const std::size_t size) -> void {
    std::memcpy(dest, src, size);
}

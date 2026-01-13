export module gse.physics.math:circle;

import std;

import gse.assert;

import :primitive_math_shared;

export namespace gse {
    template <is_vec2 T>
    class circle_t {
    public:
        using value_type = typename T::value_type;

        constexpr circle_t() = default;

        struct circle_params {
            T center;
            value_type radius;
		};

        constexpr circle_t(const circle_params& p);

        constexpr auto center() const -> const T&;
        constexpr auto radius() const -> value_type;

        constexpr auto contains(const T& point) const -> bool;
        constexpr auto intersects(const circle_t& other) const -> bool;
    private:
        T m_center;
        value_type m_radius{};
    };
}

template <gse::is_vec2 T>
constexpr gse::circle_t<T>::circle_t(const circle_params& p)
    : m_center(p.center), m_radius(p.radius)
{
    assert(radius >= 0, std::source_location::current(), "Circle radius cannot be negative");
}

template <gse::is_vec2 T>
constexpr auto gse::circle_t<T>::center() const -> const T& {
    return m_center;
}

template <gse::is_vec2 T>
constexpr auto gse::circle_t<T>::radius() const -> value_type {
    return m_radius;
}

template <gse::is_vec2 T>
constexpr auto gse::circle_t<T>::contains(const T& point) const -> bool {
    const T offset = point - m_center;
    const value_type dist_sq = (offset.x * offset.x) + (offset.y * offset.y);
    return dist_sq <= (m_radius * m_radius);
}

template <gse::is_vec2 T>
constexpr auto gse::circle_t<T>::intersects(const circle_t& other) const -> bool {
    const T offset = other.m_center - m_center;
    const value_type dist_sq = (offset.x * offset.x) + (offset.y * offset.y);
    const value_type combined_radii = m_radius + other.m_radius;
    return dist_sq <= (combined_radii * combined_radii);
}
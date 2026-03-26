export module gse.math:primitive_math;

import std;

import :circle;
import :rectangle;

export namespace gse {
    template <typename T>
    constexpr auto intersects(
        const circle_t<T>& circle, 
        const rect_t<T>& rect
    ) -> bool;

    template <typename T>
    constexpr auto intersects(
        const rect_t<T>& rect, 
        const circle_t<T>& circle
    ) -> bool;
}

template <typename T>
constexpr auto gse::intersects(const circle_t<T>& circle, const rect_t<T>& rect) -> bool {
    const typename T::value_type closest_x = std::clamp(circle.center().x, rect.left(), rect.right());
    const typename T::value_type closest_y = std::clamp(circle.center().y, rect.bottom(), rect.top());
    const T closest_point = { closest_x, closest_y };
    return contains(closest_point);
}

template <typename T>
constexpr auto gse::intersects(const rect_t<T>& rect, const circle_t<T>& circle) -> bool {
    return intersects(circle, rect);
}
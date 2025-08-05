export module gse.physics.math:rectangle;

import std;

import gse.assert;

import :primitive_math_shared;

export namespace gse {
    template <is_vec2 T>
    class rect_t {
    public:
        using value_type = typename T::value_type;

        struct min_max_params {
            T min;
            T max;
        };

        constexpr rect_t() = default;
        explicit constexpr rect_t(const min_max_params& p);

        static constexpr auto from_position_size(const T& top_left, const T& size) -> rect_t;
        static constexpr auto bounding_box(const rect_t& a, const rect_t& b) -> rect_t ;

        constexpr auto left() const -> value_type;
        constexpr auto right() const -> value_type;
        constexpr auto bottom() const -> value_type;
        constexpr auto top() const -> value_type;

        constexpr auto width() const -> value_type;
        constexpr auto height() const -> value_type;
        constexpr auto size() const -> T;
        constexpr auto center() const -> T;

        constexpr auto top_left() const -> T;
        constexpr auto bottom_left() const -> T;
        constexpr auto top_right() const -> T;
        constexpr auto bottom_right() const -> T;

        constexpr auto min() const -> const T&;
        constexpr auto max() const -> const T&;

        constexpr auto contains(const T& point) const -> bool;
        constexpr auto intersects(const rect_t& other) const -> bool;
        constexpr auto inset(const T& padding) const -> rect_t;
        constexpr auto intersection(const rect_t& other) const -> rect_t;
    private:
        T m_min;
        T m_max;
    };
}

template <gse::is_vec2 T>
constexpr gse::rect_t<T>::rect_t(const min_max_params& p)
    : m_min(std::min(p.min.x, p.max.x), std::min(p.min.y, p.max.y)),
    m_max(std::max(p.min.x, p.max.x), std::max(p.min.y, p.max.y)) {
    assert(m_min.x <= m_max.x && m_min.y <= m_max.y, "Rectangle invariant failed after construction");
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::from_position_size(const T& top_left, const T& size) -> rect_t {
    gse::assert(size.x >= 0 && size.y >= 0, "Rectangle size cannot be negative.");

    const auto bottom_left  = T{ top_left.x, top_left.y - size.y };
    const auto top_right    = T{ top_left.x + size.x, top_left.y };

    return gse::rect_t(min_max_params{ .min = bottom_left, .max = top_right });
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::bounding_box(const rect_t& a, const rect_t& b) -> rect_t {
	return gse::rect_t(min_max_params{
		.min = T{ std::min(a.m_min.x, b.m_min.x), std::min(a.m_min.y, b.m_min.y) },
		.max = T{ std::max(a.m_max.x, b.m_max.x), std::max(a.m_max.y, b.m_max.y) }
	});
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::left() const -> value_type {
    return m_min.x;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::right() const -> value_type {
    return m_max.x;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::bottom() const -> value_type {
    return m_min.y;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::top() const -> value_type {
    return m_max.y;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::width() const -> value_type {
    return m_max.x - m_min.x;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::height() const -> value_type {
    return m_max.y - m_min.y;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::size() const -> T {
    return m_max - m_min;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::center() const -> T {
    return m_min + (size() / static_cast<value_type>(2));
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::top_left() const -> T {
    return { m_min.x, m_max.y };
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::bottom_left() const -> T {
    return m_min;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::top_right() const -> T {
    return m_max;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::bottom_right() const -> T {
    return { m_max.x, m_min.y };
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::min() const -> const T& {
    return m_min;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::max() const -> const T& {
    return m_max;
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::contains(const T& point) const -> bool {
    return (point.x >= m_min.x && point.x <= m_max.x) &&
        (point.y >= m_min.y && point.y <= m_max.y);
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::intersects(const rect_t& other) const -> bool {
    return
		(m_min.x < other.m_max.x && m_max.x > other.m_min.x) &&
        (m_min.y < other.m_max.y && m_max.y > other.m_min.y);
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::inset(const T& padding) const -> rect_t {
    return rect_t(min_max_params{ 
        .min = m_min + padding,
    	.max = m_max - padding
    });
}

template <gse::is_vec2 T>
constexpr auto gse::rect_t<T>::intersection(const rect_t& other) const -> rect_t {
    const T new_min = { std::max(m_min.x, other.m_min.x), std::max(m_min.y, other.m_min.y) };
    const T new_max = { std::min(m_max.x, other.m_max.x), std::min(m_max.y, other.m_max.y) };

    if (new_min.x > new_max.x || new_min.y > new_max.y) {
        return rect_t();
    }
    return rect_t({ new_min, new_max });
}

template <gse::is_vec2 T>
struct std::formatter<gse::rect_t<T>> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const gse::rect_t<T>& rect, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "Rect(L:{}, B:{}, W:{}, H:{})", rect.left(), rect.bottom(), rect.width(), rect.height());
    }
};
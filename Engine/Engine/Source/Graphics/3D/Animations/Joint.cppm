export module gse.graphics:joint;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse {
    class joint : identifiable {
    public:
        struct params {
            std::string name;
            std::uint16_t parent_index;
            mat4 local_bind;
            mat4 inverse_bind;
        };

        explicit joint(
            const params& p
        );

        auto parent_index(
        ) const -> std::uint16_t;

        auto local_bind(
        ) const -> mat4;

        auto inverse_bind(
        ) const -> mat4;
    private:
        std::uint16_t m_parent_index = std::numeric_limits<std::uint16_t>::max();
        mat4 m_local_bind{};
        mat4 m_inverse_bind{};
    };
}

gse::joint::joint(const params& p)
    : identifiable(p.name),
      m_parent_index(p.parent_index),
      m_local_bind(p.local_bind),
      m_inverse_bind(p.inverse_bind) {
}

auto gse::joint::parent_index() const -> std::uint16_t {
    return m_parent_index;
}

auto gse::joint::local_bind() const -> mat4 {
    return m_local_bind;
}

auto gse::joint::inverse_bind() const -> mat4 {
    return m_inverse_bind;
}

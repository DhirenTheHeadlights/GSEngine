export module gse.graphics:joint;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

export namespace gse {
    class joint : public identifiable {
    public:
        struct params {
            std::string name;
            std::uint16_t parent_index;
            mat4f local_bind;
            mat4f inverse_bind;
        };

        explicit joint(
            const params& p
        );

        auto parent_index(
        ) const -> std::uint16_t;

        auto local_bind(
        ) const -> mat4f;

        auto inverse_bind(
        ) const -> mat4f;
    private:
        std::uint16_t m_parent_index = std::numeric_limits<std::uint16_t>::max();
        mat4f m_local_bind;
        mat4f m_inverse_bind;
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

auto gse::joint::local_bind() const -> mat4f {
    return m_local_bind;
}

auto gse::joint::inverse_bind() const -> mat4f {
    return m_inverse_bind;
}

export module gse.graphics:joint;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse {
    class joint : identifiable, identifiable_owned {
    public:
        explicit joint(
            const std::string& name
        );

        auto local_bind(
        ) const -> mat4;

        auto inverse_bind(
        ) const -> mat4;
    private:
        mat4 m_local_bind;
        mat4 m_inverse_bind;
    };
}

gse::joint::joint(const std::string& name) : identifiable(name) {}

auto gse::joint::local_bind() const -> mat4 {
    return m_local_bind;
}

auto gse::joint::inverse_bind() const -> mat4 {
    return m_inverse_bind;
}
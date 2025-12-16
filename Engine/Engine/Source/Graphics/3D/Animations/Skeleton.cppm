export module gse.graphics:skeleton;

import std;

import gse.utility;
import gse.physics.math;

import :joint;

namespace gse {
    class skeleton : identifiable {
    public:
	    skeleton(
            const std::string& tag,
            const std::vector<joint>& joints
        );

        auto joint_count(
        ) const -> std::uint16_t;
    private:
        std::vector<joint> m_joints;
    };

}

gse::skeleton::skeleton(const std::string& tag, const std::vector<joint>& joints): identifiable(tag), m_joints(joints) {}

auto gse::skeleton::joint_count() const -> std::uint16_t {
	return static_cast<std::uint16_t>(m_joints.size());
}


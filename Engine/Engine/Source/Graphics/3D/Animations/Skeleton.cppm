export module gse.graphics:skeleton;

import std;

import gse.utility;
import gse.physics.math;

import :joint;
import :rendering_context;

export namespace gse {
    class skeleton : identifiable {
    public:
        struct params {
            std::string name;
            std::vector<joint> joints;
        };

        explicit skeleton(
            const std::filesystem::path& path
        );

        explicit skeleton(
            const params& p
        );

        static auto compile(
        ) -> std::set<std::filesystem::path>;

        auto load(
            const renderer::context& ctx
        ) -> void;

        auto unload(
        ) -> void;

        auto joint_count(
        ) const -> std::uint16_t;

        auto joints(
        ) const -> std::span<const joint>;

        auto joint(
            std::uint16_t index
        ) const -> const joint&;
    private:
        std::vector<gse::joint> m_joints;
        std::filesystem::path m_baked_path;
    };
}

gse::skeleton::skeleton(const std::filesystem::path& path)
    : identifiable(path), m_baked_path(path) {
}

gse::skeleton::skeleton(const params& p)
    : identifiable(p.name), m_joints(p.joints) {
}

auto gse::skeleton::compile() -> std::set<std::filesystem::path> {
    return {};
}

auto gse::skeleton::load(const renderer::context& ctx) -> void {
    (void)ctx;
}

auto gse::skeleton::unload() -> void {
    m_joints.clear();
}

auto gse::skeleton::joint_count() const -> std::uint16_t {
    return static_cast<std::uint16_t>(m_joints.size());
}

auto gse::skeleton::joints() const -> std::span<const gse::joint> {
    return m_joints;
}

auto gse::skeleton::joint(const std::uint16_t index) const -> const gse::joint& {
    return m_joints[index];
}

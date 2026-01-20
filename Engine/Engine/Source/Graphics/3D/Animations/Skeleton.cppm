export module gse.graphics:skeleton;

import std;

import gse.utility;
import gse.physics.math;

import :joint;
import :rendering_context;

export namespace gse {
    class skeleton : public identifiable {
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
    const auto source_root = config::resource_path / "Skeletons";
    const auto baked_root = config::baked_resource_path / "Skeletons";

    if (!exists(source_root)) {
        return {};
    }

    if (!exists(baked_root)) {
        create_directories(baked_root);
    }

    std::set<std::filesystem::path> resources;

    for (const auto& entry : std::filesystem::directory_iterator(source_root)) {
        if (entry.path().extension() == ".gskel") {
            const auto baked_path = baked_root / entry.path().filename();
            resources.insert(baked_path);

            if (!exists(baked_path) || last_write_time(entry.path()) > last_write_time(baked_path)) {
                copy_file(entry.path(), baked_path, std::filesystem::copy_options::overwrite_existing);
            }
        }
    }

    return resources;
}

auto gse::skeleton::load(const renderer::context& ctx) -> void {
    (void)ctx;

    if (m_baked_path.empty() || !exists(m_baked_path)) {
        return;
    }

    std::ifstream file(m_baked_path, std::ios::binary);
    if (!file) {
        return;
    }

    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "GSKL", 4) != 0) {
        return;
    }

    std::uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    std::uint32_t joint_count;
    file.read(reinterpret_cast<char*>(&joint_count), sizeof(joint_count));

    m_joints.clear();
    m_joints.reserve(joint_count);

    for (std::uint32_t i = 0; i < joint_count; ++i) {
        std::uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));

        std::string name(name_len, '\0');
        file.read(name.data(), name_len);

        std::uint16_t parent_index;
        file.read(reinterpret_cast<char*>(&parent_index), sizeof(parent_index));

        mat4 local_bind;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float val;
                file.read(reinterpret_cast<char*>(&val), sizeof(val));
                local_bind[col][row] = val;
            }
        }

        mat4 inverse_bind;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float val;
                file.read(reinterpret_cast<char*>(&val), sizeof(val));
                inverse_bind[col][row] = val;
            }
        }

        m_joints.emplace_back(joint::params{
            .name = std::move(name),
            .parent_index = parent_index,
            .local_bind = local_bind,
            .inverse_bind = inverse_bind
        });
    }
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

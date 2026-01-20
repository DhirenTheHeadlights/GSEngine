export module gse.graphics:clip;

import std;

import gse.utility;
import gse.physics.math;
import :skeleton;

export namespace gse {
    struct joint_keyframe {
        time time;
        mat4 local_transform;
    };

    struct joint_track {
        std::uint16_t joint_index;
        std::vector<joint_keyframe> keys;
    };

    class clip_asset : public identifiable {
    public:
        struct params {
            std::string name;
            time length;
            bool loop;
            std::vector<joint_track> tracks;
        };

        explicit clip_asset(
            const std::filesystem::path& path
        );

        explicit clip_asset(
            params p
        );

        static auto compile(
        ) -> std::set<std::filesystem::path>;

        auto load(
            const renderer::context& ctx
        ) -> void;

        auto unload(
        ) -> void;

        auto length(
        ) const -> time;

        auto loop(
        ) const -> bool;

        auto tracks(
        ) const -> std::span<const joint_track>;
    private:
        time m_length{};
        bool m_loop = true;
        std::vector<joint_track> m_tracks;
        std::filesystem::path m_baked_path;
    };
}

gse::clip_asset::clip_asset(const std::filesystem::path& path)
    : identifiable(path), m_baked_path(path) {
}

gse::clip_asset::clip_asset(params p)
    : identifiable(p.name),
      m_length(p.length),
      m_loop(p.loop),
      m_tracks(std::move(p.tracks)) {
}

auto gse::clip_asset::compile() -> std::set<std::filesystem::path> {
    const auto source_root = config::resource_path / "Clips";
    const auto baked_root = config::baked_resource_path / "Clips";

    if (!exists(source_root)) {
        return {};
    }

    if (!exists(baked_root)) {
        create_directories(baked_root);
    }

    std::set<std::filesystem::path> resources;

    for (const auto& entry : std::filesystem::directory_iterator(source_root)) {
        if (entry.path().extension() == ".gclip") {
            const auto baked_path = baked_root / entry.path().filename();
            resources.insert(baked_path);

            if (!exists(baked_path) || last_write_time(entry.path()) > last_write_time(baked_path)) {
                copy_file(entry.path(), baked_path, std::filesystem::copy_options::overwrite_existing);
            }
        }
    }

    return resources;
}

auto gse::clip_asset::load(const renderer::context& ctx) -> void {
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
    if (std::memcmp(magic, "GCLP", 4) != 0) {
        return;
    }

    std::uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    std::uint32_t name_len;
    file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
    std::string name(name_len, '\0');
    file.read(name.data(), name_len);

    float length_seconds;
    file.read(reinterpret_cast<char*>(&length_seconds), sizeof(length_seconds));
    m_length = seconds(length_seconds);

    std::uint8_t loop_byte;
    file.read(reinterpret_cast<char*>(&loop_byte), sizeof(loop_byte));
    m_loop = loop_byte != 0;

    std::uint32_t track_count;
    file.read(reinterpret_cast<char*>(&track_count), sizeof(track_count));

    m_tracks.clear();
    m_tracks.reserve(track_count);

    for (std::uint32_t t = 0; t < track_count; ++t) {
        joint_track track;

        file.read(reinterpret_cast<char*>(&track.joint_index), sizeof(track.joint_index));

        std::uint32_t key_count;
        file.read(reinterpret_cast<char*>(&key_count), sizeof(key_count));

        track.keys.reserve(key_count);

        for (std::uint32_t k = 0; k < key_count; ++k) {
            float key_time_seconds;
            file.read(reinterpret_cast<char*>(&key_time_seconds), sizeof(key_time_seconds));

            mat4 local_transform;
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    float val;
                    file.read(reinterpret_cast<char*>(&val), sizeof(val));
                    local_transform[col][row] = val;
                }
            }

            track.keys.push_back(joint_keyframe{
                .time = seconds(key_time_seconds),
                .local_transform = local_transform
            });
        }

        m_tracks.push_back(std::move(track));
    }
}

auto gse::clip_asset::unload() -> void {
    m_tracks.clear();
}

auto gse::clip_asset::length() const -> time {
    return m_length;
}

auto gse::clip_asset::loop() const -> bool {
    return m_loop;
}

auto gse::clip_asset::tracks() const -> std::span<const joint_track> {
    return m_tracks;
}

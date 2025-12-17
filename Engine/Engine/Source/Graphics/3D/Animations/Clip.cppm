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

    class clip_asset : identifiable {
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
    return {};
}

auto gse::clip_asset::load(const renderer::context& ctx) -> void {
    (void)ctx;
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

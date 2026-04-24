export module gse.graphics:mp4_muxer;

import std;

import gse.math;
import gse.gpu;

export namespace gse::renderer::capture::mp4 {
    struct track_info {
        gpu::video_codec codec;
        vec2u extent;
    };

    [[nodiscard]] auto mux(
        std::span<const gpu::encoded_unit> units,
        const track_info& track,
        const std::filesystem::path& out
    ) -> bool;
}

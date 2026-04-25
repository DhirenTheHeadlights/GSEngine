export module gse.network:input_frame;

import std;

import :message;

export namespace gse::network {
    struct [[= network_message{}]] input_frame_header {
        std::uint32_t input_sequence = 0;
        std::uint32_t client_time_ms = 0;
        std::uint16_t action_word_count = 0;
        std::uint16_t axes1_count = 0;
        std::uint16_t axes2_count = 0;
        float camera_yaw = 0.f;
    };
}

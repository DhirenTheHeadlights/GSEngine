module gse.gpu:video_encoder_impl;

import std;

import gse.gpu_backend;
import gse.math;

import :video_encoder;

namespace gse::gpu {
	struct video_encoder_backend {};
}

gse::gpu::video_encoder::video_encoder() noexcept = default;

gse::gpu::video_encoder::~video_encoder() = default;

gse::gpu::video_encoder::video_encoder(video_encoder&& other) noexcept = default;

auto gse::gpu::video_encoder::operator=(video_encoder&& other) noexcept -> video_encoder& = default;

gse::gpu::video_encoder::video_encoder(std::unique_ptr<video_encoder_backend> impl) noexcept : m_impl(std::move(impl)) {
}

auto gse::gpu::video_encoder::set_bitrate(bitrate) -> void {}

auto gse::gpu::video_encoder::begin_capture(time) -> encode_source {
	return {};
}

auto gse::gpu::video_encoder::take_bitstream() -> std::optional<encoded_unit> {
	return std::nullopt;
}

auto gse::gpu::video_encoder::submit_ready() -> void {}

auto gse::gpu::video_encoder::stream_header() const -> std::span<const std::byte> {
	return {};
}

auto gse::gpu::video_encoder::codec() const -> video_codec {
	return video_codec::av1;
}

auto gse::gpu::video_encoder::extent() const -> vec2u {
	return {};
}

auto gse::gpu::video_encoder::valid() const -> bool {
	return false;
}

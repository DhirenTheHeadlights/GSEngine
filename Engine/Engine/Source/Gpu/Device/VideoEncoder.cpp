module gse.gpu:video_encoder_impl;

import std;

import gse.vulkan;
import :video_encoder;
import :video_backend;

gse::gpu::video_encoder::video_encoder() noexcept = default;

gse::gpu::video_encoder::~video_encoder() = default;

gse::gpu::video_encoder::video_encoder(video_encoder&& other) noexcept = default;

auto gse::gpu::video_encoder::operator=(video_encoder&& other) noexcept -> video_encoder& = default;

gse::gpu::video_encoder::video_encoder(std::unique_ptr<video_encoder_backend> impl) noexcept : m_impl(std::move(impl)) {
}

auto gse::gpu::video_encoder::encode_frame(const std::uint32_t frame_slot, const handle<gpu::image> y_plane, const handle<gpu::image> uv_plane) -> void {
	m_impl->enc.encode_frame(frame_slot, y_plane, uv_plane);
}

auto gse::gpu::video_encoder::wait(const std::uint32_t frame_slot) -> void {
	m_impl->enc.wait(frame_slot);
}

auto gse::gpu::video_encoder::read_bitstream(const std::uint32_t frame_slot) -> std::optional<encoded_unit> {
	return m_impl->enc.read_bitstream(frame_slot);
}

auto gse::gpu::video_encoder::stream_header() const -> std::span<const std::byte> {
	return m_impl->enc.stream_header();
}

auto gse::gpu::video_encoder::codec() const -> video_codec {
	return m_impl->enc.codec();
}

auto gse::gpu::video_encoder::extent() const -> vec2u {
	return m_impl->enc.extent();
}

auto gse::gpu::video_encoder::valid() const -> bool {
	return m_impl && m_impl->enc.valid();
}

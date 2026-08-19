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

auto gse::gpu::video_encoder::set_bitrate(const bitrate rate) -> void {
	m_impl->enc.set_bitrate(rate);
}

auto gse::gpu::video_encoder::begin_capture(const time pts) -> encode_source {
	return m_impl->enc.begin_capture(pts);
}

auto gse::gpu::video_encoder::take_bitstream() -> std::optional<encoded_unit> {
	return m_impl->enc.take_bitstream();
}

auto gse::gpu::video_encoder::submit_ready() -> void {
	m_impl->enc.submit_ready();
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

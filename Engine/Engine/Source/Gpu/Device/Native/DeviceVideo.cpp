module gse.gpu:device_video;

import gse.gpu_backend;
import std;

import :device;
import :device_contract;
import :video_backend;
import :video_encoder;

auto gse::gpu::device::make_video_encoder(const encode_desc& desc) -> std::optional<video_encoder> {
	if (!m_video_encode_enabled) {
		return std::nullopt;
	}
	auto backend = m_vt->make_video_encoder_backend(m_backend.get(), desc);
	if (!backend) {
		return std::nullopt;
	}
	return video_encoder(std::move(backend));
}

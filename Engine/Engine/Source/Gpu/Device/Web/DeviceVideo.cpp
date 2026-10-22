module gse.gpu:device_video;

import gse.gpu_backend;
import std;

import :device;
import :video_encoder;

auto gse::gpu::device::make_video_encoder(const encode_desc&) -> std::optional<video_encoder> {
	return std::nullopt;
}

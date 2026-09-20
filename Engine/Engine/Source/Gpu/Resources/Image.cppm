export module gse.gpu:image;

import gse.gpu_backend;

import :device;
import :sync_token;

export namespace gse::gpu {
	auto transition_image_to(
		device& dev,
		image& img
	) -> sync_token;
}
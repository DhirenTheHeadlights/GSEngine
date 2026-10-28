export module gse.gpu:video_backend;

import gse.vulkan;

export namespace gse::gpu {
	struct video_encoder_backend {
		vulkan::video_encoder enc;
	};
}

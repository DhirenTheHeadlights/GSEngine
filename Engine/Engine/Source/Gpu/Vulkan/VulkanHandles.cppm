export module gse.gpu:vulkan_handles;

import vulkan;

import :gpu_types;

export namespace gse::vulkan {
	struct pipeline_handle {
		vk::Pipeline pipeline = nullptr;
		vk::PipelineLayout layout = nullptr;
		gpu::bind_point point = gpu::bind_point::graphics;
	};
}

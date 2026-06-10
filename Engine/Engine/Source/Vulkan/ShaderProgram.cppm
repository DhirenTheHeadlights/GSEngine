export module gse.vulkan:shader_program;

import std;

import gse.gpu_backend;
import :shader_object;

export namespace gse::vulkan {
	struct shader_program_create_info {
		std::span<const shader_object_create_info> stages;
		std::optional<gpu::push_constant_range> push_constant_range;
		gpu::dynamic_pipeline_state state;
		bool is_compute = false;
		bool is_mesh = false;
	};
}

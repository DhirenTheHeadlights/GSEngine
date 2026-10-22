module gse.gpu_record:shader_compile_impl;

import gse.log;
import std;

import :pipeline_builder;
import :shader_compile;

auto gse::gpu::compile_compute_spirv(const shader_compile_inputs& inputs, const std::string_view) -> std::vector<std::uint32_t> {
	log::println(log::level::error, log::category::assets, "no shader compiler on this platform; cannot compile compute shader: {}", inputs.body_path);
	return {};
}

auto gse::gpu::compile_graphics_program(const graphics_entry_pod& pod, const std::string_view) -> compiled_graphics_program {
	log::println(log::level::error, log::category::assets, "no shader compiler on this platform; cannot compile graphics shader: {}", pod.body_path);
	return {};
}

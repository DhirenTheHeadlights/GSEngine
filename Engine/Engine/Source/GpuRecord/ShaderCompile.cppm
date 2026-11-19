module gse.gpu_record:shader_compile;

import std;

import gse.config;
import gse.gpu;
import gse.gpu_backend;
import gse.meta;

import :pipeline_builder;

namespace gse::gpu {
	struct parsed_body {
		std::vector<std::string> imports;
		std::string body;
	};

	struct graphics_stage_compile_result {
		stage_flag flag = stage_flag::vertex;
		graphics_stage_kind kind = graphics_stage_kind::vertex;
		std::string entry_point;
		std::vector<std::uint32_t> spirv;
	};

	struct compiled_graphics_program {
		std::vector<graphics_stage_compile_result> stages;
	};

	[[nodiscard]]
	auto compile_compute_spirv(
		const compute_entry_pod& pod,
		std::string_view wrapper_source
	) -> std::vector<std::uint32_t>;

	[[nodiscard]]
	auto compile_graphics_program(
		const graphics_entry_pod& pod,
		std::string_view wrapper_source
	) -> compiled_graphics_program;

	[[nodiscard]]
	auto wgsl_pack_file(
		std::string_view body_path,
		std::string_view entry_point,
		std::string_view wrapper_source
	) -> std::filesystem::path;

	[[nodiscard]]
	auto wgsl_pack_dir() -> std::filesystem::path;

	[[nodiscard]]
	auto shader_source_digest() -> std::uint64_t;
}

auto gse::gpu::wgsl_pack_dir() -> std::filesystem::path {
	return config::resource_path() / "Shaders" / "Wgsl";
}

auto gse::gpu::wgsl_pack_file(const std::string_view body_path, const std::string_view entry_point, const std::string_view wrapper_source) -> std::filesystem::path {
	return wgsl_pack_dir() / std::format("{:016x}.wgsl", hash_of(shader_source_digest(), body_path, entry_point, wrapper_source));
}

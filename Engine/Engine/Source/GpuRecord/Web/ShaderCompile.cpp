module gse.gpu_record:shader_compile_impl;

import gse.assert;
import gse.log;
import gse.time;
import std;

import :pipeline_builder;
import :shader_compile;

namespace gse::gpu {
	auto load_wgsl(
		std::string_view body_path,
		std::string_view entry_point,
		std::string_view wrapper_source
	) -> std::vector<std::uint32_t>;
}

auto gse::gpu::shader_source_digest() -> std::uint64_t {
	static const std::uint64_t digest = [] {
		const clock digest_clock;
		const auto stamp = wgsl_pack_dir() / "digest";
		std::ifstream in(stamp);
		std::string recorded;
		in >> recorded;
		std::uint64_t h = 0;
		const auto [end, ec] = std::from_chars(recorded.data(), recorded.data() + recorded.size(), h, 16);
		assert(!recorded.empty() && ec == std::errc{} && end == recorded.data() + recorded.size(), "wgsl pack: no shader source digest at {}; run the native build with Graphics.backend=webgpu to bake the pack", stamp.native_encoded_string());
		log::println(log::category::assets, "shader source digest: 1 files in {:.1f:ms}", digest_clock.elapsed());
		return h;
	}();
	return digest;
}

auto gse::gpu::load_wgsl(const std::string_view body_path, const std::string_view entry_point, const std::string_view wrapper_source) -> std::vector<std::uint32_t> {
	const auto file = wgsl_pack_file(body_path, entry_point, wrapper_source);
	std::ifstream in(file, std::ios::binary);
	assert(in.is_open(), "wgsl pack: no precompiled {} entry '{}' at {} for shader sources {:016x}; the sources changed or were never baked, so run the native build with Graphics.backend=webgpu to bake them", body_path, entry_point, file.native_encoded_string(), shader_source_digest());
	const std::string code((std::istreambuf_iterator(in)), std::istreambuf_iterator<char>());
	std::vector<std::uint32_t> words((code.size() + sizeof(std::uint32_t)) / sizeof(std::uint32_t), 0u);
	std::memcpy(words.data(), code.data(), code.size());
	log::println(log::level::info, log::category::assets, "loaded precompiled wgsl: {} entry '{}'", body_path, entry_point);
	return words;
}

auto gse::gpu::compile_compute_spirv(const shader_compile_inputs& inputs, const std::string_view wrapper_source) -> std::vector<std::uint32_t> {
	return load_wgsl(inputs.body_path, "main", wrapper_source);
}

auto gse::gpu::compile_graphics_program(const graphics_entry_pod& pod, const std::string_view wrapper_source) -> compiled_graphics_program {
	compiled_graphics_program result;
	result.stages.reserve(pod.stage_count);
	for (std::size_t i = 0; i < pod.stage_count; ++i) {
		const auto& stage_pod = pod.stages[i];
		result.stages.push_back({
			.flag = stage_pod.stage_flag_value,
			.kind = stage_pod.kind,
			.entry_point = std::string(stage_pod.entry_point),
			.spirv = load_wgsl(pod.body_path, stage_pod.entry_point, wrapper_source),
		});
	}
	return result;
}

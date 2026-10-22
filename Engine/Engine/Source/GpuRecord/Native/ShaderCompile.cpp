module gse.gpu_record:shader_compile_impl;

import gse.assert;
import gse.config;
import gse.core;
import gse.gpu;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.slang;
import gse.time;
import std;

import :pipeline_builder;
import :shader_compile;

namespace gse::gpu {
	struct owned_slang_session {
		Slang::ComPtr<slang::IGlobalSession> global;
		Slang::ComPtr<slang::ISession> session;
	};

	auto make_slang_session() -> owned_slang_session;

	auto shader_search_paths() -> const std::vector<std::string>&;

	auto log_slang_diagnostics(
		slang::IBlob* diagnostics
	) -> void;

	auto strip_unused_ray_tracing_extension(
		std::vector<std::uint32_t>& spirv
	) -> void;
}

auto gse::gpu::make_slang_session() -> owned_slang_session {
	owned_slang_session out;
	static Slang::ComPtr<slang::IGlobalSession> cached_global = [] {
		Slang::ComPtr<slang::IGlobalSession> g;
		createGlobalSession(g.writeRef());
		return g;
	}();
	if (!cached_global) {
		log::println(
			log::level::error,
			log::category::assets,
			"Failed to create Slang global session for pipeline builder"
		);
		return out;
	}
	out.global = cached_global;
	auto* global = out.global.get();

	const auto& sp_storage = shader_search_paths();
	std::vector<const char*> sp_c_strs;
	sp_c_strs.reserve(sp_storage.size());
	for (const auto& s : sp_storage) {
		sp_c_strs.push_back(s.c_str());
	}

	const bool use_dxil = active_backend == backend_kind::dx12;
	slang::TargetDesc target{
		.format = use_dxil ? slang_dxil : slang_spirv,
		.profile = global->findProfile(use_dxil ? "sm_6_6" : "spirv_1_5"),
		.floatingPointMode = use_dxil ? slang_floating_point_mode_precise : slang_floating_point_mode_default,
		.forceGLSLScalarBufferLayout = !use_dxil,
	};

	std::vector<slang::CompilerOptionEntry> compiler_options;
	if (!use_dxil) {
		compiler_options.push_back(slang::CompilerOptionEntry{
			.name = slang::CompilerOptionName::DebugInformation,
			.value = {
				.kind = slang::CompilerOptionValueKind::Int,
				.intValue0 = static_cast<std::int32_t>(slang_debug_info_level_standard),
			},
		});
		compiler_options.push_back(slang::CompilerOptionEntry{
			.name = slang::CompilerOptionName::Capability,
			.value = {
				.kind = slang::CompilerOptionValueKind::Int,
				.intValue0 = static_cast<std::int32_t>(global->findCapability("spvDescriptorHeapEXT")),
			},
		});
	}

	slang::SessionDesc sdesc{
		.targets = &target,
		.targetCount = 1,
		.defaultMatrixLayoutMode = slang_matrix_layout_column_major,
		.searchPaths = sp_c_strs.data(),
		.searchPathCount = static_cast<SlangInt>(sp_c_strs.size()),
		.compilerOptionEntries = compiler_options.data(),
		.compilerOptionEntryCount = static_cast<std::uint32_t>(compiler_options.size()),
	};

	if (slang_failed(global->createSession(sdesc, out.session.writeRef())) || !out.session) {
		log::println(log::level::error, log::category::assets, "Failed to create Slang session for pipeline builder");
		return owned_slang_session{};
	}
	return out;
}

auto gse::gpu::shader_search_paths() -> const std::vector<std::string>& {
	static const std::vector<std::string> paths = [] {
		const auto shader_root = config::resource_path() / "Shaders";
		std::vector<std::string> result;
		result.push_back(shader_root.native_encoded_string());
		for (const auto& e : std::filesystem::recursive_directory_iterator(shader_root)) {
			if (!e.is_directory()) {
				continue;
			}
			bool in_bodies = false;
			for (const auto& part : e.path()) {
				if (part == "Bodies") {
					in_bodies = true;
					break;
				}
			}
			if (in_bodies) {
				continue;
			}
			result.push_back(e.path().native_encoded_string());
		}
		return result;
	}();
	return paths;
}

auto gse::gpu::log_slang_diagnostics(slang::IBlob* diagnostics) -> void {
	if (!diagnostics || diagnostics->getBufferSize() == 0) {
		return;
	}
	const std::string message(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());
	const bool fatal = message.contains("error") || message.contains("fatal");
	if (fatal) {
		log::println(log::level::error, log::category::assets, "{}", message);
		return;
	}

	std::string kept;
	for (const auto line : std::views::split(std::string_view(message), '\n')) {
		const std::string_view text(line.begin(), line.end());
		if (text.contains("E41012")) {
			continue;
		}
		if (!kept.empty()) {
			kept += '\n';
		}
		kept += text;
	}
	if (kept.find_first_not_of(" \t\r\n") == std::string::npos) {
		return;
	}
	log::println(log::level::warning, log::category::assets, "{}", kept);
}

static std::mutex g_slang_compile_mutex;
static std::atomic<std::uint64_t> g_slang_module_counter{ 0 };

auto gse::gpu::strip_unused_ray_tracing_extension(std::vector<std::uint32_t>& spirv) -> void {
	if (spirv.size() < 5 || spirv[0] != 0x07230203u) {
		return;
	}

	constexpr std::uint16_t op_extension = 10;
	constexpr std::uint16_t op_capability = 17;
	constexpr std::uint32_t capability_ray_tracing = 4479;
	constexpr std::string_view ray_tracing_extension = "SPV_KHR_ray_tracing";

	std::size_t extension_begin = 0;
	std::size_t extension_count = 0;

	std::size_t i = 5;
	while (i < spirv.size()) {
		const std::uint32_t word = spirv[i];
		const auto count = static_cast<std::uint16_t>(word >> 16);
		const auto opcode = static_cast<std::uint16_t>(word & 0xFFFFu);
		if (count == 0 || i + count > spirv.size()) {
			return;
		}
		if (opcode == op_capability) {
			if (spirv[i + 1] == capability_ray_tracing) {
				return;
			}
		}
		else if (opcode == op_extension) {
			std::string name;
			bool done = false;
			for (std::size_t w = i + 1; w < i + count && !done; ++w) {
				const auto packed = spirv[w];
				for (int b = 0; b < 4; ++b) {
					const auto c = static_cast<char>((packed >> (b * 8)) & 0xFFu);
					if (c == '\0') {
						done = true;
						break;
					}
					name.push_back(c);
				}
			}
			if (name == ray_tracing_extension) {
				extension_begin = i;
				extension_count = count;
			}
		}
		else {
			break;
		}
		i += count;
	}

	if (extension_count != 0) {
		spirv.erase(spirv.begin() + static_cast<std::ptrdiff_t>(extension_begin), spirv.begin() + static_cast<std::ptrdiff_t>(extension_begin + extension_count));
	}
}

auto gse::gpu::compile_compute_spirv(const shader_compile_inputs& inputs, const std::string_view wrapper_source) -> std::vector<std::uint32_t> {
	const std::lock_guard _(g_slang_compile_mutex);
	const clock compile_clock;
	auto owned = make_slang_session();
	auto* session = owned.session.get();
	assert(session, "Slang session not available");

	log::println(log::level::info, log::category::assets, "compiling compute shader: {}", inputs.body_path);
	const std::string module_name = std::format("entry_{}_{}", inputs.body_path, g_slang_module_counter.fetch_add(1));
	std::string sanitized = module_name;
	std::ranges::replace(sanitized, '/', '_');
	std::ranges::replace(sanitized, '\\', '_');

	auto dump_wrapper_source = [&]() {
		log::println(
			log::level::error,
			log::category::assets,
			"Wrapper source for '{}' (body: {}):\n{}",
			sanitized,
			inputs.body_path,
			wrapper_source
		);
	};

	Slang::ComPtr<slang::IBlob> diags;
	const std::string source(wrapper_source);
	Slang::ComPtr mod(session->loadModuleFromSourceString(
		sanitized.c_str(),
		(sanitized + ".slang").c_str(),
		source.c_str(),
		diags.writeRef()
	));
	log_slang_diagnostics(diags.get());
	if (!mod) {
		dump_wrapper_source();
		assert(false, "Failed to compile shader wrapper module: {}", sanitized);
	}

	Slang::ComPtr<slang::IEntryPoint> cs_ep;
	const int ep_count = mod->getDefinedEntryPointCount();
	for (int i = 0; i < ep_count; ++i) {
		Slang::ComPtr<slang::IEntryPoint> ep;
		mod->getDefinedEntryPoint(i, ep.writeRef());
		if (!std::strcmp(ep->getFunctionReflection()->getName(), "main")) {
			cs_ep = ep;
			break;
		}
	}
	if (!cs_ep) {
		dump_wrapper_source();
		assert(false, "Compute entry point 'main' not found in wrapper module: {}", sanitized);
	}

	Slang::ComPtr<slang::IComponentType> composed;
	slang::IComponentType* parts[] = { cs_ep.get() };
	if (slang_failed(session->createCompositeComponentType(parts, 1, composed.writeRef(), diags.writeRef()))) {
		log_slang_diagnostics(diags.get());
		dump_wrapper_source();
		assert(false, "Failed to compose compute shader module: {}", sanitized);
	}

	Slang::ComPtr<slang::IComponentType> program;
	if (slang_failed(composed->link(program.writeRef(), diags.writeRef()))) {
		log_slang_diagnostics(diags.get());
		dump_wrapper_source();
		assert(false, "Failed to link compute shader module: {}", sanitized);
	}

	Slang::ComPtr<ISlangBlob> blob;
	if (slang_failed(program->getEntryPointCode(0, 0, blob.writeRef(), diags.writeRef())) || !blob) {
		log_slang_diagnostics(diags.get());
		dump_wrapper_source();
		assert(false, "Failed to get SPIR-V for compute shader: {}", sanitized);
	}

	const auto byte_size = blob->getBufferSize();
	std::vector<std::uint32_t> spirv(byte_size / sizeof(std::uint32_t));
	std::memcpy(spirv.data(), blob->getBufferPointer(), byte_size);
	strip_unused_ray_tracing_extension(spirv);
	log::println(
		log::level::info,
		log::category::assets,
		"compiled compute shader: {} in {:.1f:ms}",
		inputs.body_path,
		compile_clock.elapsed()
	);
	return spirv;
}

auto gse::gpu::compile_graphics_program(const graphics_entry_pod& pod, const std::string_view wrapper_source) -> compiled_graphics_program {
	const std::lock_guard _(g_slang_compile_mutex);
	const clock compile_clock;
	auto owned = make_slang_session();
	auto* session = owned.session.get();
	assert(session, "Slang session not available");

	log::println(log::level::info, log::category::assets, "compiling graphics shader: {}", pod.body_path);
	const std::string module_name = std::format("gfx_{}_{}", pod.body_path, g_slang_module_counter.fetch_add(1));
	std::string sanitized = module_name;
	std::ranges::replace(sanitized, '/', '_');
	std::ranges::replace(sanitized, '\\', '_');

	auto dump_wrapper_source = [&] {
		log::println(
			log::level::error,
			log::category::assets,
			"Wrapper source for '{}' (body: {}):\n{}",
			sanitized,
			pod.body_path,
			wrapper_source
		);
	};

	Slang::ComPtr<slang::IBlob> diags;
	const std::string source(wrapper_source);
	Slang::ComPtr mod(session->loadModuleFromSourceString(
		sanitized.c_str(),
		(sanitized + ".slang").c_str(),
		source.c_str(),
		diags.writeRef()
	));
	log_slang_diagnostics(diags.get());
	if (!mod) {
		dump_wrapper_source();
		assert(false, "Failed to compile graphics shader wrapper module: {}", sanitized);
	}

	compiled_graphics_program result;
	result.stages.reserve(pod.stage_count);

	std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points;
	entry_points.reserve(pod.stage_count);
	std::vector<std::size_t> ep_indices(pod.stage_count);

	for (std::size_t i = 0; i < pod.stage_count; ++i) {
		const auto& stage_pod = pod.stages[i];
		Slang::ComPtr<slang::IEntryPoint> ep;
		const int ep_count = mod->getDefinedEntryPointCount();
		for (int j = 0; j < ep_count; ++j) {
			Slang::ComPtr<slang::IEntryPoint> candidate;
			mod->getDefinedEntryPoint(j, candidate.writeRef());
			if (!std::strcmp(candidate->getFunctionReflection()->getName(), std::string(stage_pod.entry_point).c_str())) {
				ep = candidate;
				break;
			}
		}
		if (!ep) {
			dump_wrapper_source();
			assert(false, "Entry point '{}' not found in module: {}", stage_pod.entry_point, sanitized);
		}
		ep_indices[i] = entry_points.size();
		entry_points.push_back(std::move(ep));
	}

	std::vector<slang::IComponentType*> parts;
	parts.reserve(entry_points.size() + 1);
	parts.push_back(mod.get());
	for (const auto& ep : entry_points) {
		parts.push_back(ep.get());
	}

	Slang::ComPtr<slang::IComponentType> composed;
	if (slang_failed(session->createCompositeComponentType(parts.data(), static_cast<SlangInt>(parts.size()), composed.writeRef(), diags.writeRef()))) {
		log_slang_diagnostics(diags.get());
		dump_wrapper_source();
		assert(false, "Failed to compose graphics program in {}", sanitized);
	}

	Slang::ComPtr<slang::IComponentType> program;
	if (slang_failed(composed->link(program.writeRef(), diags.writeRef()))) {
		log_slang_diagnostics(diags.get());
		dump_wrapper_source();
		assert(false, "Failed to link graphics program in {}", sanitized);
	}

	for (std::size_t i = 0; i < pod.stage_count; ++i) {
		const auto& stage_pod = pod.stages[i];

		Slang::ComPtr<ISlangBlob> blob;
		if (slang_failed(program->getEntryPointCode(static_cast<SlangInt>(ep_indices[i]), 0, blob.writeRef(), diags.writeRef())) || !blob) {
			log_slang_diagnostics(diags.get());
			dump_wrapper_source();
			assert(false, "Failed to get SPIR-V for graphics entry point '{}' in {}", stage_pod.entry_point, sanitized);
		}

		const auto byte_size = blob->getBufferSize();
		std::vector<std::uint32_t> spirv(byte_size / sizeof(std::uint32_t));
		std::memcpy(spirv.data(), blob->getBufferPointer(), byte_size);
		strip_unused_ray_tracing_extension(spirv);

		result.stages.push_back({
			.flag = stage_pod.stage_flag_value,
			.kind = stage_pod.kind,
			.entry_point = std::string(stage_pod.entry_point),
			.spirv = std::move(spirv),
		});
	}

	log::println(
		log::level::info,
		log::category::assets,
		"compiled graphics shader: {} in {:.1f:ms}",
		pod.body_path,
		compile_clock.elapsed()
	);
	return result;
}

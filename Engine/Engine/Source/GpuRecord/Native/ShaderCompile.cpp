module gse.gpu_record:shader_compile_impl;

import gse.assert;
import gse.config;
import gse.core;
import gse.gpu;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.meta;
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

	auto code_words(
		ISlangBlob* blob
	) -> std::vector<std::uint32_t>;

	auto store_wgsl(
		ISlangBlob* blob,
		std::string_view body_path,
		std::string_view entry_point,
		std::string_view wrapper_source
	) -> void;

	auto assert_wgsl_struct_layouts(
		slang::IComponentType* program,
		std::vector<shaders::struct_layout> (*struct_layouts_fn)(),
		std::string_view body_path
	) -> void;
}

auto gse::gpu::assert_wgsl_struct_layouts(slang::IComponentType* program, std::vector<shaders::struct_layout> (*struct_layouts_fn)(), const std::string_view body_path) -> void {
	if (active_backend != backend_kind::webgpu || !struct_layouts_fn) {
		return;
	}
	const auto expected = struct_layouts_fn();
	std::unordered_set<std::string> checked;
	auto check = [&](this auto&& self, slang::TypeLayoutReflection* type) -> void {
		if (!type) {
			return;
		}
		switch (type->getKind()) {
			case slang::TypeReflection::Kind::Struct: {
				const char* raw_name = type->getName();
				const std::string name = raw_name ? raw_name : "";
				if (checked.insert(name).second) {
					if (const auto it = std::ranges::find(expected, name, &shaders::struct_layout::name); it != expected.end()) {
						assert(type->getSize() == it->size, "{}: WGSL struct {} is {} bytes, the C++ struct is {}", body_path, name, type->getSize(), it->size);
						assert(type->getFieldCount() == it->fields.size(), "{}: WGSL struct {} has {} fields, the planned layout has {}", body_path, name, type->getFieldCount(), it->fields.size());
						for (unsigned f = 0; f < type->getFieldCount(); ++f) {
							auto* field = type->getFieldByIndex(f);
							const auto& want = it->fields[f];
							assert(field->getName() == want.name, "{}: WGSL struct {} field {} is {}, the planned layout has {}", body_path, name, f, field->getName(), want.name);
							assert(field->getOffset() == want.offset, "{}: WGSL struct {} field {} sits at offset {}, the C++ offset is {}", body_path, name, want.name, field->getOffset(), want.offset);
						}
					}
				}
				for (unsigned f = 0; f < type->getFieldCount(); ++f) {
					self(type->getFieldByIndex(f)->getTypeLayout());
				}
				break;
			}
			case slang::TypeReflection::Kind::Array:
			case slang::TypeReflection::Kind::Resource:
			case slang::TypeReflection::Kind::ConstantBuffer:
			case slang::TypeReflection::Kind::ParameterBlock:
				self(type->getElementTypeLayout());
				break;
			default:
				break;
		}
	};
	auto* layout = program->getLayout();
	for (unsigned i = 0; i < layout->getParameterCount(); ++i) {
		check(layout->getParameterByIndex(i)->getTypeLayout());
	}
}

auto gse::gpu::code_words(ISlangBlob* blob) -> std::vector<std::uint32_t> {
	const auto byte_size = blob->getBufferSize();
	std::vector<std::uint32_t> words((byte_size + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t), 0u);
	std::memcpy(words.data(), blob->getBufferPointer(), byte_size);
	return words;
}

auto gse::gpu::shader_source_digest() -> std::uint64_t {
	static const std::uint64_t digest = [] {
		const clock digest_clock;
		const auto root = config::resource_path() / "Shaders";
		const auto pack = wgsl_pack_dir();
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
			if (entry.is_regular_file() && !entry.path().native().starts_with(pack.native())) {
				files.push_back(entry.path());
			}
		}
		std::ranges::sort(files);
		std::uint64_t h = 0;
		for (const auto& file : files) {
			std::ifstream in(file, std::ios::binary);
			const std::string bytes((std::istreambuf_iterator(in)), std::istreambuf_iterator<char>());
			h = hash_combine(h, hash_of(file.lexically_relative(root).generic_string(), bytes));
		}
		log::println(log::category::assets, "shader source digest: {} files in {:.1f:ms}", files.size(), digest_clock.elapsed());
		return h;
	}();
	return digest;
}

auto gse::gpu::store_wgsl(ISlangBlob* blob, const std::string_view body_path, const std::string_view entry_point, const std::string_view wrapper_source) -> void {
	if (active_backend != backend_kind::webgpu) {
		return;
	}
	static std::once_flag refreshed;
	std::call_once(refreshed, [] {
		const auto dir = wgsl_pack_dir();
		const auto stamp = dir / "digest";
		const auto current = std::format("{:016x}", shader_source_digest());
		std::ifstream in(stamp);
		std::string recorded;
		in >> recorded;
		if (recorded == current) {
			return;
		}
		std::error_code ec;
		std::filesystem::remove_all(dir, ec);
		std::filesystem::create_directories(dir, ec);
		std::ofstream(stamp, std::ios::trunc) << current;
		log::println(log::category::assets, "wgsl pack: shader sources changed ({} -> {}), cleared {}", recorded.empty() ? "none" : recorded, current, dir.native_encoded_string());
	});
	const auto file = wgsl_pack_file(body_path, entry_point, wrapper_source);
	std::error_code ec;
	std::filesystem::create_directories(file.parent_path(), ec);
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	out.write(static_cast<const char*>(blob->getBufferPointer()), static_cast<std::streamsize>(blob->getBufferSize()));
	if (!out) {
		log::println(log::level::warning, log::category::assets, "wgsl pack: could not write {} for {}", file.native_encoded_string(), body_path);
	}
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
	const bool use_wgsl = active_backend == backend_kind::webgpu;
	slang::TargetDesc target{
		.format = use_dxil ? slang_dxil : use_wgsl ? slang_wgsl : slang_spirv,
		.profile = global->findProfile(use_dxil ? "sm_6_6" : use_wgsl ? "wgsl" : "spirv_1_5"),
		.floatingPointMode = use_dxil ? slang_floating_point_mode_precise : slang_floating_point_mode_default,
		.forceGLSLScalarBufferLayout = !use_dxil && !use_wgsl,
	};

	std::vector<slang::CompilerOptionEntry> compiler_options;
	if (!use_dxil && !use_wgsl) {
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
	assert_wgsl_struct_layouts(program.get(), inputs.struct_layouts_fn, inputs.body_path);

	Slang::ComPtr<ISlangBlob> blob;
	if (slang_failed(program->getEntryPointCode(0, 0, blob.writeRef(), diags.writeRef())) || !blob) {
		log_slang_diagnostics(diags.get());
		dump_wrapper_source();
		assert(false, "Failed to get SPIR-V for compute shader: {}", sanitized);
	}

	store_wgsl(blob.get(), inputs.body_path, "main", wrapper_source);
	auto spirv = code_words(blob.get());
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
	assert_wgsl_struct_layouts(program.get(), pod.struct_layouts_fn, pod.body_path);

	for (std::size_t i = 0; i < pod.stage_count; ++i) {
		const auto& stage_pod = pod.stages[i];

		Slang::ComPtr<ISlangBlob> blob;
		if (slang_failed(program->getEntryPointCode(static_cast<SlangInt>(ep_indices[i]), 0, blob.writeRef(), diags.writeRef())) || !blob) {
			log_slang_diagnostics(diags.get());
			dump_wrapper_source();
			assert(false, "Failed to get SPIR-V for graphics entry point '{}' in {}", stage_pod.entry_point, sanitized);
		}

		store_wgsl(blob.get(), pod.body_path, stage_pod.entry_point, wrapper_source);
		auto spirv = code_words(blob.get());
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

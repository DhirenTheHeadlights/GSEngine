module gse.gpu_record:pipeline_builder_impl;

import std;
import gse.meta;
import gse.assert;
import gse.log;
import gse.core;
import gse.config;
import gse.slang;
import gse.math;

import gse.gpu_backend;
import gse.gpu;
import :pipeline_builder;

namespace gse::gpu {
	auto to_pipeline_stage(const stage_flag s) -> pipeline_stage_flag {
		switch (s) {
			case stage_flag::vertex:
				return pipeline_stage_flag::vertex_shader;
			case stage_flag::fragment:
				return pipeline_stage_flag::fragment_shader;
			case stage_flag::compute:
				return pipeline_stage_flag::compute_shader;
			case stage_flag::task:
				return pipeline_stage_flag::task_shader;
			case stage_flag::mesh:
				return pipeline_stage_flag::mesh_shader;
		}
		return pipeline_stage_flag::all_commands;
	}

	struct shader_param_decl {
		std::string name;
		std::string slang_type;
		std::string semantic;
		bool is_function_param = false;
	};

	struct shader_compile_inputs {
		std::vector<shader_param_decl> params;
		std::uint32_t threads_x = 1;
		std::uint32_t threads_y = 1;
		std::uint32_t threads_z = 1;
		std::string body_path;
		std::string inline_source;
		std::vector<std::string> helper_paths;
		std::vector<std::string> call_names;
		std::uint32_t push_constant_size = 0;
		std::string (
			*emit_push_constant_struct
		)() = nullptr;
		std::string (
			*emit_types
		)() = nullptr;
		std::string (
			*emit_bindings
		)() = nullptr;
		std::string (
			*emit_spec_decls
		)() = nullptr;
	};

	struct owned_slang_session {
		Slang::ComPtr<slang::IGlobalSession> global;
		Slang::ComPtr<slang::ISession> session;
	};

	auto make_slang_session() -> owned_slang_session;

	auto shader_search_paths() -> const std::vector<std::string>&;

	auto log_slang_diagnostics(
		slang::IBlob* diagnostics
	) -> void;

	struct parsed_body {
		std::vector<std::string> imports;
		std::string body;
	};

	auto parse_body_file(
		std::string_view body_source
	) -> parsed_body;

	auto load_body_file(
		std::string_view body_path
	) -> std::string;

	auto load_helper_file(
		std::string_view helper_path
	) -> std::string;

	auto inline_helpers(
		const std::vector<std::string>& helper_paths
	) -> std::string;

	auto build_compute_wrapper_source(
		const shader_compile_inputs& inputs,
		const parsed_body& parsed
	) -> std::string;

	[[nodiscard]]
	auto compile_compute_spirv(
		const shader_compile_inputs& inputs,
		std::string_view wrapper_source
	) -> std::vector<std::uint32_t>;

	auto strip_unused_ray_tracing_extension(
		std::vector<std::uint32_t>& spirv
	) -> void;

	struct graphics_stage_compile_result {
		stage_flag flag = stage_flag::vertex;
		graphics_stage_kind kind = graphics_stage_kind::vertex;
		std::string entry_point;
		std::vector<std::uint32_t> spirv;
	};

	struct compiled_graphics_program {
		std::vector<graphics_stage_compile_result> stages;
	};

	auto build_graphics_wrapper_source(
		const graphics_entry_pod& pod,
		const parsed_body& parsed
	) -> std::string;

	[[nodiscard]]
	auto compile_graphics_program(
		const graphics_entry_pod& pod,
		std::string_view wrapper_source
	) -> compiled_graphics_program;
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
		result.push_back(shader_root.string());
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
			result.push_back(e.path().string());
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
	log::println(log::level::error, log::category::assets, "{}", message);
}

auto gse::gpu::parse_body_file(const std::string_view body_source) -> parsed_body {
	parsed_body result;
	std::size_t cursor = 0;
	bool seen_non_import = false;

	while (cursor < body_source.size()) {
		const std::size_t line_end = body_source.find('\n', cursor);
		const std::size_t take = (line_end == std::string_view::npos) ? body_source.size() : line_end;
		std::string_view line = body_source.substr(cursor, take - cursor);

		std::string_view trimmed = line;
		while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
			trimmed.remove_prefix(1);
		}

		if (!seen_non_import && (trimmed.starts_with("import ") || trimmed.starts_with("__exported import "))) {
			result.imports.emplace_back(line);
		}
		else if (!seen_non_import && trimmed.empty()) {
		}
		else {
			seen_non_import = true;
			result.body.append(line);
			result.body.push_back('\n');
		}

		if (line_end == std::string_view::npos) {
			break;
		}
		cursor = line_end + 1;
	}

	return result;
}



auto gse::gpu::load_body_file(const std::string_view body_path) -> std::string {
	const auto full_path = config::resource_path() / "Shaders" / "Bodies" / (std::string(body_path) + ".slang");
	std::ifstream in(full_path, std::ios::binary);
	assert(in.is_open(), "Failed to open shader body: {}", full_path.display_string());

	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

auto gse::gpu::load_helper_file(const std::string_view helper_path) -> std::string {
	const auto full_path = config::resource_path() / "Shaders" / (std::string(helper_path) + ".slang");
	std::ifstream in(full_path, std::ios::binary);
	assert(in.is_open(), "Failed to open shader helper: {}", full_path.display_string());

	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

auto gse::gpu::inline_helpers(const std::vector<std::string>& helper_paths) -> std::string {
	std::string out;
	for (const auto& path : helper_paths) {
		const auto source = load_helper_file(path);
		const auto parsed = parse_body_file(source);
		out.append(parsed.body);
		out.push_back('\n');
	}
	return out;
}

auto gse::gpu::build_compute_wrapper_source(const shader_compile_inputs& inputs, const parsed_body& parsed) -> std::string {
	std::string out;

	for (const auto& imp : parsed.imports) {
		out.append(imp);
		out.push_back('\n');
	}
	out.push_back('\n');

	if (inputs.emit_types) {
		out.append(inputs.emit_types());
		out.push_back('\n');
	}

	if (inputs.emit_bindings) {
		out.append(inputs.emit_bindings());
		out.push_back('\n');
	}

	if (inputs.emit_push_constant_struct) {
		out.append(inputs.emit_push_constant_struct());
		out.push_back('\n');
	}

	if (inputs.emit_spec_decls) {
		out.append(inputs.emit_spec_decls());
		out.push_back('\n');
	}

	for (const auto& p : inputs.params) {
		if (p.is_function_param) {
			continue;
		}
		out.append("[[vk::push_constant]]\nConstantBuffer<");
		out.append(p.slang_type);
		out.push_back('>');
		out.push_back(' ');
		out.append(p.name);
		out.append(";\n\n");
	}

	if (!inputs.helper_paths.empty()) {
		out.append(inline_helpers(inputs.helper_paths));
		out.push_back('\n');
	}

	out.append(parsed.body);
	out.push_back('\n');

	out.append("[shader(\"compute\")]\n");
	out.append(std::format("[numthreads({}, {}, {})]\n", inputs.threads_x, inputs.threads_y, inputs.threads_z));
	out.append("void main(");

	bool first = true;
	for (const auto& p : inputs.params) {
		if (!p.is_function_param) {
			continue;
		}
		if (!first) {
			out.append(", ");
		}
		first = false;
		out.append(std::format("{} {} : {}", p.slang_type, p.name, p.semantic));
	}

	out.append(") {\n");
	for (const auto& name : inputs.call_names) {
		out.append("    ");
		out.append(name);
		out.append("(");
		bool first_arg = true;
		for (const auto& p : inputs.params) {
			if (!p.is_function_param) {
				continue;
			}
			if (!first_arg) {
				out.append(", ");
			}
			first_arg = false;
			out.append(p.name);
		}
		out.append(");\n");
	}
	out.append("}\n");
	return out;
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
	const std::lock_guard compile_lock(g_slang_compile_mutex);
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
	return spirv;
}

auto gse::gpu::build_graphics_wrapper_source(const graphics_entry_pod& pod, const parsed_body& parsed) -> std::string {
	std::string out;

	for (const auto& imp : parsed.imports) {
		out.append(imp);
		out.push_back('\n');
	}
	out.push_back('\n');

	if (pod.emit_types) {
		out.append(pod.emit_types());
		out.push_back('\n');
	}

	if (pod.emit_bindings) {
		out.append(pod.emit_bindings());
		out.push_back('\n');
	}

	if (pod.emit_push_constant_struct) {
		out.append(pod.emit_push_constant_struct());
		out.push_back('\n');
	}

	if (pod.emit_spec_decls) {
		out.append(pod.emit_spec_decls());
		out.push_back('\n');
	}

	if (pod.push_constant_size > 0) {
		out.append("[[vk::push_constant]]\nConstantBuffer<");
		out.append(pod.push_constant_type_name);
		out.append("> pc;\n\n");
	}

	if (pod.helper_count > 0) {
		std::vector<std::string> helper_paths;
		helper_paths.reserve(pod.helper_count);
		for (std::size_t i = 0; i < pod.helper_count; ++i) {
			helper_paths.emplace_back(pod.helper_paths[i]);
		}
		out.append(inline_helpers(helper_paths));
		out.push_back('\n');
	}

	out.append(parsed.body);
	return out;
}

auto gse::gpu::compile_graphics_program(const graphics_entry_pod& pod, const std::string_view wrapper_source) -> compiled_graphics_program {
	const std::lock_guard compile_lock(g_slang_compile_mutex);
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

	return result;
}

namespace gse::gpu {
	auto blend_preset_to_attachment_state(
		blend_preset preset
	) -> std::tuple<bool, color_blend_equation, color_component_flags>;

	auto next_stage_for(
		stage_flag current,
		std::span<const stage_flag> all_stages
	) -> stage_flags;
}

auto gse::gpu::blend_preset_to_attachment_state(const blend_preset preset) -> std::tuple<bool, color_blend_equation, color_component_flags> {
	constexpr auto all_components = color_component_flag::r | color_component_flag::g | color_component_flag::b | color_component_flag::a;
	switch (preset) {
		case blend_preset::none:
			return { false, color_blend_equation{}, all_components };
		case blend_preset::alpha:
			return {
				true,
				color_blend_equation{
					.src_color = blend_factor::src_alpha,
					.dst_color = blend_factor::one_minus_src_alpha,
					.color_op = blend_op::add,
					.src_alpha = blend_factor::one,
					.dst_alpha = blend_factor::one_minus_src_alpha,
					.alpha_op = blend_op::add,
				},
				all_components,
			};
		case blend_preset::alpha_premultiplied:
			return {
				true,
				color_blend_equation{
					.src_color = blend_factor::src_alpha,
					.dst_color = blend_factor::one_minus_src_alpha,
					.color_op = blend_op::add,
					.src_alpha = blend_factor::one,
					.dst_alpha = blend_factor::zero,
					.alpha_op = blend_op::add,
				},
				all_components,
			};
	}
	return { false, color_blend_equation{}, all_components };
}

auto gse::gpu::next_stage_for(const stage_flag current, const std::span<const stage_flag> all_stages) -> stage_flags {
	stage_flags result{};
	switch (current) {
		case stage_flag::task:
			for (const auto s : all_stages) {
				if (s == stage_flag::mesh) {
					result |= stage_flag::mesh;
				}
			}
			break;
		case stage_flag::vertex:
		case stage_flag::mesh:
			for (const auto s : all_stages) {
				if (s == stage_flag::fragment) {
					result |= stage_flag::fragment;
				}
			}
			break;
		case stage_flag::fragment:
		case stage_flag::compute:
			break;
	}
	return result;
}

auto gse::gpu::build_compute_program(device& dev, const compute_entry_pod& pod, const std::span<const std::byte> spec_data) -> shader_program {
	assert(pod.build_family_sets_fn, "bindings missing on compute entry");
	assert(
		spec_data.empty() || spec_data.size() == pod.spec_data_size,
		"spec_data size mismatch with entry's spec_constants<T>"
	);
	const auto family_sets = pod.build_family_sets_fn();

	shader_compile_inputs inputs;
	inputs.body_path = std::string(pod.body_path);
	inputs.inline_source = std::string(pod.body_source);
	inputs.threads_x = pod.threads_x;
	inputs.threads_y = pod.threads_y;
	inputs.threads_z = pod.threads_z;
	inputs.push_constant_size = pod.push_constant_size;
	inputs.emit_push_constant_struct = pod.emit_push_constant_struct;
	inputs.emit_types = pod.emit_types;
	inputs.emit_bindings = pod.emit_bindings;
	inputs.emit_spec_decls = pod.emit_spec_decls;
	inputs.helper_paths.reserve(pod.helper_count);

	for (std::size_t i = 0; i < pod.helper_count; ++i) {
		inputs.helper_paths.emplace_back(pod.helper_paths[i]);
	}

	inputs.call_names.reserve(pod.call_count);
	for (std::size_t i = 0; i < pod.call_count; ++i) {
		inputs.call_names.emplace_back(pod.call_names[i]);
	}

	inputs.params.reserve(pod.param_count);

	for (std::size_t i = 0; i < pod.param_count; ++i) {
		inputs.params.push_back({
			.name = std::string(pod.params[i].name),
			.slang_type = std::string(pod.params[i].slang_type),
			.semantic = std::string(pod.params[i].semantic),
			.is_function_param = pod.params[i].is_function_param,
		});
	}

	const auto body_source = inputs.inline_source.empty() ? load_body_file(inputs.body_path) : inputs.inline_source;
	const auto parsed = parse_body_file(body_source);
	const auto wrapper_source = build_compute_wrapper_source(inputs, parsed);
	const auto spirv = compile_compute_spirv(inputs, wrapper_source);

	std::optional<push_constant_range> push_range;
	if (pod.push_constant_size > 0) {
		push_range = push_constant_range{
			.stages = stage_flag::compute,
			.offset = 0,
			.size = pod.push_constant_size,
		};
	}

	std::vector<binding_use> pack_bindings;
	for (const auto& fs : family_sets) {
		for (const auto& b : fs.bindings) {
			pack_bindings.push_back({
				.set = fs.set_index,
				.slot = b.desc.binding,
				.count = b.desc.count,
				.access = b.desc.access,
				.type = b.desc.type,
				.stages = pipeline_stage_flag::compute_shader,
			});
		}
	}

	std::vector<specialization_entry> vk_spec_entries;
	if (pod.build_spec_entries_fn && !spec_data.empty()) {
		const auto entries = pod.build_spec_entries_fn();
		vk_spec_entries.reserve(entries.size());
		for (const auto& e : entries) {
			vk_spec_entries.push_back({
				.constant_id = e.constant_id,
				.offset = e.offset,
				.size = e.size,
			});
		}
	}

	const shader_object_create_info stage_info{
		.stage = stage_flag::compute,
		.spirv = spirv,
		.entry_point = "main",
		.next_stage = {},
		.required_subgroup_size = pod.required_subgroup_size != 0
			? std::optional<std::uint32_t>(pod.required_subgroup_size)
			: std::nullopt,
		.require_full_subgroups = pod.require_full_subgroups,
		.spec_entries = vk_spec_entries,
		.spec_data = spec_data,
	};

	const shader_program_create_info info{
		.stages = std::span(&stage_info, 1),
		.bindings = pack_bindings,
		.push_offset_start = pod.push_constant_size,
		.push_constant_range = push_range,
		.state = {},
		.is_compute = true,
		.is_mesh = false,
	};

	return dev.create_shader_program(info);
}

auto gse::gpu::build_graphics_program(device& dev, const graphics_entry_pod& pod, const std::span<const std::byte> spec_data) -> shader_program {
	assert(!pod.body_path.empty(), "body_path missing on graphics entry");
	assert(pod.stage_count > 0, "graphics entry has no stages");
	assert(pod.build_family_sets_fn, "bindings missing on graphics entry");
	assert(
		spec_data.empty() || spec_data.size() == pod.spec_data_size,
		"spec_data size mismatch with entry's spec_constants<T>"
	);
	const auto family_sets = pod.build_family_sets_fn();

	const std::string body_source = pod.body_source.empty() ? load_body_file(pod.body_path) : std::string(pod.body_source);
	const auto parsed = parse_body_file(body_source);
	const auto wrapper_source = build_graphics_wrapper_source(pod, parsed);
	auto program = compile_graphics_program(pod, wrapper_source);

	std::optional<push_constant_range> push_range;
	if (pod.push_constant_size > 0) {
		push_range = push_constant_range{
			.stages = stage_flags::from_bits(static_cast<std::uint8_t>(pod.push_constant_stages)),
			.offset = 0,
			.size = pod.push_constant_size,
		};
	}

	std::vector<stage_flag> all_stages;
	all_stages.reserve(program.stages.size());
	for (const auto& s : program.stages) {
		all_stages.push_back(s.flag);
	}

	bool is_mesh = false;
	for (auto& s : program.stages) {
		if (s.kind == graphics_stage_kind::amplification || s.kind == graphics_stage_kind::mesh) {
			is_mesh = true;
		}
	}

	pipeline_stage_flags all_pipeline_stages{};
	for (const auto s : all_stages) {
		all_pipeline_stages |= to_pipeline_stage(s);
	}

	std::vector<binding_use> pack_bindings;
	for (const auto& fs : family_sets) {
		for (const auto& b : fs.bindings) {
			pack_bindings.push_back({
				.set = fs.set_index,
				.slot = b.desc.binding,
				.count = b.desc.count,
				.access = b.desc.access,
				.type = b.desc.type,
				.stages = all_pipeline_stages,
			});
		}
	}

	std::vector<specialization_entry> vk_spec_entries;
	if (pod.build_spec_entries_fn && !spec_data.empty()) {
		const auto entries = pod.build_spec_entries_fn();
		vk_spec_entries.reserve(entries.size());
		for (const auto& e : entries) {
			vk_spec_entries.push_back({
				.constant_id = e.constant_id,
				.offset = e.offset,
				.size = e.size,
			});
		}
	}

	std::vector<shader_object_create_info> stage_infos;
	stage_infos.reserve(program.stages.size());
	for (auto& s : program.stages) {
		stage_infos.push_back({
			.stage = s.flag,
			.spirv = s.spirv,
			.entry_point = "main",
			.next_stage = next_stage_for(s.flag, all_stages),
			.spec_entries = vk_spec_entries,
			.spec_data = spec_data,
		});
	}

	const auto [blend_enable, blend_eq, write_mask] = blend_preset_to_attachment_state(pod.blend);

	dynamic_pipeline_state state{
		.topology = pod.topology_value,
		.polygon = pod.rasterization.polygon,
		.cull = pod.rasterization.cull,
		.front = front_face::counter_clockwise,
		.depth = pod.depth,
		.depth_bias_enable = pod.rasterization.depth_bias,
		.depth_bias_constant = pod.rasterization.depth_bias_constant,
		.depth_bias_clamp = pod.rasterization.depth_bias_clamp,
		.depth_bias_slope = pod.rasterization.depth_bias_slope,
	};

	state.blend_enables.assign(pod.color_count, static_cast<std::uint8_t>(blend_enable ? 1 : 0));
	state.blend_equations.assign(pod.color_count, blend_eq);
	state.color_write_masks.assign(pod.color_count, write_mask);

	std::array<color_format, 8> color_targets{};
	std::uint32_t color_target_count = 0;
	for (std::size_t i = 0; i < pod.color_count && i < color_targets.size(); ++i) {
		color_targets[color_target_count++] = pod.colors[i];
	}
	const depth_format depth_target = (pod.depth.test || pod.depth.write) ? pod.depth_fmt : depth_format::none;

	const shader_program_create_info info{
		.stages = stage_infos,
		.bindings = pack_bindings,
		.push_offset_start = pod.push_constant_size,
		.push_constant_range = push_range,
		.state = std::move(state),
		.color_targets = color_targets,
		.color_target_count = color_target_count,
		.depth_target = depth_target,
		.is_compute = false,
		.is_mesh = is_mesh,
	};

	return dev.create_shader_program(info);
}


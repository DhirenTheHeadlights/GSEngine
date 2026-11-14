module gse.gpu_record:pipeline_builder_impl;

import gse.assert;
import gse.concurrency;
import gse.config;
import gse.core;
import gse.gpu;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.meta;
import gse.time;
import std;

import :pipeline_builder;
import :shader_compile;

namespace gse::gpu {
	auto assert_push_data_fits(const device& dev, const std::uint32_t push_constant_size, const std::uint32_t binding_args_size, const std::string_view body_path) -> void {
		const auto limit = dev.max_push_data_size();
		const auto total = push_constant_size + binding_args_size;
		assert(
			limit == 0 || total <= limit,
			"{}: push constants ({} B) plus bindless descriptor indices ({} B) total {} B, over this device's push-data limit of {} B. "
			"Move the largest push-constant member into a storage buffer binding, or drop a binding.",
			body_path,
			push_constant_size,
			binding_args_size,
			total,
			limit
		);
	}

	struct system_value_info {
		char semantic[24] = {};
	};

	enum class system_value : std::uint8_t {
		dispatch_id [[= system_value_info{
			.semantic = "SV_DispatchThreadID",
		}]],
		group_id [[= system_value_info{
			.semantic = "SV_GroupID",
		}]],
		group_thread_id [[= system_value_info{
			.semantic = "SV_GroupThreadID",
		}]],
		group_index [[= system_value_info{
			.semantic = "SV_GroupIndex",
		}]],
	};

	auto parse_body_file(
		std::string_view body_source
	) -> parsed_body;

	auto read_shader_source(
		const std::filesystem::path& full_path
	) -> std::string;

	auto load_body_file(
		std::string_view body_path
	) -> std::string;

	auto load_helper_file(
		std::string_view helper_path
	) -> std::string;

	auto inline_helpers(
		std::string_view body_path,
		std::span<const std::string_view> helper_paths
	) -> std::string;

	auto wrapper_prelude(
		const entry_pod& pod,
		const parsed_body& parsed,
		std::string_view runtime_constants
	) -> std::string;

	auto build_compute_wrapper_source(
		const compute_entry_pod& pod,
		const parsed_body& parsed,
		std::string_view runtime_constants
	) -> std::string;

	auto build_graphics_wrapper_source(
		const graphics_entry_pod& pod,
		const parsed_body& parsed
	) -> std::string;

	auto push_range_of(
		const entry_pod& pod,
		stage_flags stages
	) -> std::optional<push_constant_range>;
}

auto gse::gpu::push_range_of(const entry_pod& pod, const stage_flags stages) -> std::optional<push_constant_range> {
	if (pod.push_constant_size == 0) {
		return std::nullopt;
	}
	return push_constant_range{
		.stages = stages,
		.offset = 0,
		.size = pod.push_constant_size,
	};
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



auto gse::gpu::read_shader_source(const std::filesystem::path& full_path) -> std::string {
	std::ifstream in(full_path, std::ios::binary);
	assert(in.is_open(), "Failed to open shader source: {}", full_path.generic_display_string());

	std::ostringstream ss;
	ss << in.rdbuf();
	std::string source = ss.str();

	constexpr std::string_view utf8_bom = "\xEF\xBB\xBF";
	if (source.starts_with(utf8_bom)) {
		log::println(
			log::level::warning,
			log::category::assets,
			"stripped a UTF-8 byte order mark from shader source: {}",
			full_path.generic_display_string()
		);
		source.erase(0, utf8_bom.size());
	}
	return source;
}

auto gse::gpu::load_body_file(const std::string_view body_path) -> std::string {
	return read_shader_source(config::shader_file(std::filesystem::path("Bodies") / (std::string(body_path) + ".slang")));
}

auto gse::gpu::load_helper_file(const std::string_view helper_path) -> std::string {
	return read_shader_source(config::shader_file(std::string(helper_path) + ".slang"));
}

auto gse::gpu::inline_helpers(const std::string_view body_path, const std::span<const std::string_view> helper_paths) -> std::string {
	log::println(log::category::render, "shader program {}: helpers {}", body_path, helper_paths);
	std::string out;
	for (const auto path : helper_paths) {
		const auto source = load_helper_file(path);
		const auto parsed = parse_body_file(source);
		out.append(parsed.body);
		out.push_back('\n');
	}
	return out;
}

auto gse::gpu::wrapper_prelude(const entry_pod& pod, const parsed_body& parsed, const std::string_view runtime_constants) -> std::string {
	std::string out;

	for (const auto& imp : parsed.imports) {
		out.append(imp);
		out.push_back('\n');
	}
	out.push_back('\n');

	out.append(pod.emit_types());

	if (!runtime_constants.empty()) {
		out.append(runtime_constants);
		out.push_back('\n');
	}

	out.append(pod.emit_bindings());
	if (pod.bindless_textures) {
		out.append(shaders::emit_bindless_texture_table());
	}
	out.push_back('\n');

	if (pod.push_constant_size > 0) {
		if (active_backend == backend_kind::webgpu) {
			out.append(std::format(
				"[[vk::binding(0, 1)]] StructuredBuffer<{0}> pc_buffer;\n"
				"property {0} pc {{ get {{ return pc_buffer[0]; }} }}\n\n",
				pod.push_constant_type_name
			));
		}
		else {
			out.append(std::format("[[vk::push_constant]]\nConstantBuffer<{}> pc;\n\n", pod.push_constant_type_name));
		}
	}

	if (pod.helper_count > 0) {
		out.append(inline_helpers(pod.body_path, std::span(pod.helper_paths).first(pod.helper_count)));
		out.push_back('\n');
	}

	return out;
}

auto gse::gpu::build_compute_wrapper_source(const compute_entry_pod& pod, const parsed_body& parsed, const std::string_view runtime_constants) -> std::string {
	std::string out = wrapper_prelude(pod, parsed, runtime_constants);
	out.append(parsed.body);
	out.push_back('\n');

	const auto call_name = pod.body_path.substr(pod.body_path.find_last_of('/') + 1);
	const auto signature = std::format("void {}(", call_name);
	const auto begin = parsed.body.find(signature);
	assert(begin != std::string::npos, "{}: compute body defines no entry function 'void {}(...)'", pod.body_path, call_name);
	const auto open = begin + signature.size();
	const auto close = parsed.body.find(')', open);
	assert(close != std::string::npos, "{}: entry function '{}' has an unterminated parameter list", pod.body_path, call_name);

	std::string declared;
	std::string forwarded;
	for (const auto part : std::views::split(std::string_view(parsed.body).substr(open, close - open), ',')) {
		std::string_view parameter(part.begin(), part.end());
		while (!parameter.empty() && (parameter.front() == ' ' || parameter.front() == '\t')) {
			parameter.remove_prefix(1);
		}
		while (!parameter.empty() && (parameter.back() == ' ' || parameter.back() == '\t')) {
			parameter.remove_suffix(1);
		}
		if (parameter.empty()) {
			continue;
		}
		const auto name = parameter.substr(parameter.find_last_of(' ') + 1);
		system_value value{};
		const bool known = enum_from_string(name, value);
		assert(known, "{}: entry parameter '{}' is not a compute system value; name it one of {}", pod.body_path, parameter, enum_values<system_value>());
		const system_value_info info = annotation_from_enum(value, system_value_info{});
		const std::string_view separator = declared.empty() ? "" : ", ";
		declared.append(std::format("{}{} : {}", separator, parameter, std::string_view(info.semantic)));
		forwarded.append(std::format("{}{}", separator, name));
	}

	out.append(std::format(
		"[shader(\"compute\")]\n[numthreads({}, {}, {})]\nvoid main({}) {{\n    {}({});\n}}\n",
		pod.threads_x,
		pod.threads_y,
		pod.threads_z,
		declared,
		call_name,
		forwarded
	));
	return out;
}

auto gse::gpu::build_graphics_wrapper_source(const graphics_entry_pod& pod, const parsed_body& parsed) -> std::string {
	std::string out = wrapper_prelude(pod, parsed, {});
	out.append(parsed.body);
	return out;
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
	constexpr color_component_flags all_components{ color_component_flag::r, color_component_flag::g, color_component_flag::b, color_component_flag::a };
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

auto gse::gpu::build_compute_program(device& dev, const compute_entry_pod& pod, const std::string_view runtime_constants) -> shader_program {
	assert(pod.emit_bindings, "bindings missing on entry");
	assert_push_data_fits(dev, pod.push_constant_size, pod.binding_args_size, pod.body_path);

	const auto parsed = parse_body_file(load_body_file(pod.body_path));
	const auto wrapper_source = build_compute_wrapper_source(pod, parsed, runtime_constants);
	const auto spirv = compile_compute_spirv(pod, wrapper_source);

	const shader_object_create_info stage_info{
		.stage = stage_flag::compute,
		.spirv = spirv,
		.entry_point = "main",
		.next_stage = {},
	};

	const auto binding_layout = pod.binding_layout_fn();
	const shader_program_create_info info{
		.label = pod.body_path,
		.stages = std::span(&stage_info, 1),
		.push_offset_start = pod.push_constant_size,
		.binding_args_size = pod.binding_args_size,
		.bindings = binding_layout,
		.push_constant_range = push_range_of(pod, stage_flag::compute),
		.state = {},
		.is_compute = true,
		.is_mesh = false,
	};

	return dev.create_shader_program(info);
}

gse::gpu::program_batch::program_batch(device& dev) : m_device(dev), m_group(trace_id<"gpu::program_batch">()) {}

gse::gpu::program_batch::~program_batch() = default;

auto gse::gpu::program_batch::add(shader_program& out, const compute_entry_pod& pod, const std::string_view runtime_constants) -> void {
	m_group.post(
		[&dev = m_device, &out, &pod, constants = std::string(runtime_constants)] {
			out = build_compute_program(dev, pod, constants);
		},
		trace_id<"gpu::program_batch::compute">()
	);
}

auto gse::gpu::program_batch::add(shader_program& out, const graphics_entry_pod& pod) -> void {
	m_group.post(
		[&dev = m_device, &out, &pod] {
			out = build_graphics_program(dev, pod);
		},
		trace_id<"gpu::program_batch::graphics">()
	);
}

auto gse::gpu::build_graphics_program(device& dev, const graphics_entry_pod& pod) -> shader_program {
	assert(!pod.body_path.empty(), "body_path missing on graphics entry");
	assert(pod.stage_count > 0, "graphics entry has no stages");
	assert(pod.emit_bindings, "bindings missing on entry");
	assert_push_data_fits(dev, pod.push_constant_size, pod.binding_args_size, pod.body_path);

	const auto parsed = parse_body_file(load_body_file(pod.body_path));
	const auto wrapper_source = build_graphics_wrapper_source(pod, parsed);
	auto program = compile_graphics_program(pod, wrapper_source);

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

	std::vector<shader_object_create_info> stage_infos;
	stage_infos.reserve(program.stages.size());
	for (auto& s : program.stages) {
		stage_infos.push_back({
			.stage = s.flag,
			.spirv = s.spirv,
			.entry_point = active_backend == backend_kind::webgpu ? std::string_view(s.entry_point) : std::string_view("main"),
			.next_stage = next_stage_for(s.flag, all_stages),
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
	const depth_format depth_target = (pod.depth.test || pod.depth.write) ? depth_format::d32_sfloat : depth_format::none;

	const auto binding_layout = pod.binding_layout_fn();
	const shader_program_create_info info{
		.label = pod.body_path,
		.stages = stage_infos,
		.push_offset_start = pod.push_constant_size,
		.binding_args_size = pod.binding_args_size,
		.bindings = binding_layout,
		.push_constant_range = push_range_of(pod, stage_flags::from_bits(static_cast<std::uint8_t>(pod.push_constant_stages))),
		.state = std::move(state),
		.color_targets = color_targets,
		.color_target_count = color_target_count,
		.depth_target = depth_target,
		.is_compute = false,
		.is_mesh = is_mesh,
	};

	return dev.create_shader_program(info);
}

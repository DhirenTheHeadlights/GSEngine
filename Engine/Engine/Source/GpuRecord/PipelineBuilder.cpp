module gse.gpu_record:pipeline_builder_impl;

import gse.assert;
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
		const std::vector<std::string>& helper_paths
	) -> std::string;

	auto build_compute_wrapper_source(
		const shader_compile_inputs& inputs,
		const parsed_body& parsed
	) -> std::string;

	auto build_graphics_wrapper_source(
		const graphics_entry_pod& pod,
		const parsed_body& parsed
	) -> std::string;
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
	return read_shader_source(config::resource_path() / "Shaders" / "Bodies" / (std::string(body_path) + ".slang"));
}

auto gse::gpu::load_helper_file(const std::string_view helper_path) -> std::string {
	return read_shader_source(config::resource_path() / "Shaders" / (std::string(helper_path) + ".slang"));
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

	if (!inputs.runtime_constants.empty()) {
		out.append(inputs.runtime_constants);
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

auto gse::gpu::build_compute_program(device& dev, const compute_entry_pod& pod, const std::span<const std::byte> spec_data, const std::string_view runtime_constants) -> shader_program {
	assert(
		spec_data.empty() || spec_data.size() == pod.spec_data_size,
		"spec_data size mismatch with entry's spec_constants<T>"
	);
	assert(pod.binding_args_size_fn, "bindings missing on entry");
	assert_push_data_fits(dev, pod.push_constant_size, pod.binding_args_size_fn(), pod.body_path);

	shader_compile_inputs inputs;
	inputs.body_path = std::string(pod.body_path);
	inputs.inline_source = std::string(pod.body_source);
	inputs.runtime_constants = std::string(runtime_constants);
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
		.push_offset_start = pod.push_constant_size,
		.binding_args_size = pod.binding_args_size_fn(),
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
	assert(
		spec_data.empty() || spec_data.size() == pod.spec_data_size,
		"spec_data size mismatch with entry's spec_constants<T>"
	);
	assert(pod.binding_args_size_fn, "bindings missing on entry");
	assert_push_data_fits(dev, pod.push_constant_size, pod.binding_args_size_fn(), pod.body_path);

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
		.push_offset_start = pod.push_constant_size,
		.binding_args_size = pod.binding_args_size_fn(),
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

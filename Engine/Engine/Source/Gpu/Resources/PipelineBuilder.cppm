export module gse.gpu:pipeline_builder;

import std;
import gse.std_meta;
import gse.meta;
import gse.assert;
import gse.log;
import gse.core;
import gse.config;
import gse.slang;

import :aliases;
import :handles;
import :types;
import :vulkan_pipeline;
import :vulkan_shader_module;
import :vulkan_device;
import :device;
import :bindless;
import :descriptor_heap;
import :descriptors;
import :shader_codegen;
import :shader_markers;
import :shader_registry;
import :spirv_reflect;

export namespace gse::gpu {
	struct compute_param_pod {
		std::string_view name;
		std::string_view slang_type;
		std::string_view semantic;
		bool is_function_param = false;
	};

	struct compute_entry_pod {
		std::string_view body_path;
		std::string_view body_source;
		std::string_view layout_name;
		std::uint32_t threads_x = 1;
		std::uint32_t threads_y = 1;
		std::uint32_t threads_z = 1;
		std::uint32_t push_constant_size = 0;
		std::array<compute_param_pod, 8> params{};
		std::size_t param_count = 0;
		std::array<std::string_view, 4> helper_paths{};
		std::size_t helper_count = 0;
		std::string (*emit_push_constant_struct)() = nullptr;
		std::string (*emit_types)() = nullptr;
		std::string (*emit_bindings)() = nullptr;
		std::vector<shaders::family_set> (*build_family_sets_fn)() = nullptr;
	};

	[[nodiscard]] auto build_compute_pipeline(
		device& dev,
		shader_registry& registry,
		bindless_texture_set& bindless,
		const compute_entry_pod& pod
	) -> pipeline;

	enum class graphics_stage_kind : std::uint8_t {
		vertex,
		fragment,
		amplification,
		mesh,
	};

	struct graphics_stage_pod {
		std::string_view entry_point;
		graphics_stage_kind kind = graphics_stage_kind::vertex;
		stage_flag stage_flag_value = stage_flag::vertex;
	};

	struct graphics_entry_pod {
		std::string_view body_path;
		std::string_view body_source;
		std::string_view layout_name;
		std::string_view push_constant_type_name;
		std::uint32_t push_constant_size = 0;
		std::uint32_t push_constant_stages = 0;
		std::array<graphics_stage_pod, 4> stages{};
		std::size_t stage_count = 0;
		std::array<std::string_view, 4> helper_paths{};
		std::size_t helper_count = 0;
		rasterization_state rasterization{};
		depth_state depth{};
		blend_preset blend = blend_preset::none;
		topology topology_value = topology::triangle_list;
		color_format color = color_format::swapchain;
		depth_format depth_fmt = depth_format::d32_sfloat;
		std::string (*emit_push_constant_struct)() = nullptr;
		std::string (*emit_types)() = nullptr;
		std::string (*emit_bindings)() = nullptr;
		std::vector<shaders::family_set> (*build_family_sets_fn)() = nullptr;
	};

	[[nodiscard]] auto build_graphics_pipeline(
		device& dev,
		shader_registry& registry,
		bindless_texture_set& bindless,
		const graphics_entry_pod& pod
	) -> pipeline;

	[[nodiscard]] inline auto allocate_descriptors(
		shader_registry& registry,
		descriptor_heap& heap,
		const compute_entry_pod& pod,
		const std::source_location& loc = std::source_location::current()
	) -> descriptor_region {
		return allocate_descriptors(registry, heap, pod.layout_name, loc);
	}

	[[nodiscard]] inline auto allocate_descriptors(
		shader_registry& registry,
		descriptor_heap& heap,
		const graphics_entry_pod& pod,
		const std::source_location& loc = std::source_location::current()
	) -> descriptor_region {
		return allocate_descriptors(registry, heap, pod.layout_name, loc);
	}

	[[nodiscard]] inline auto make_push_writer(
		shader_registry& registry,
		handle<vulkan::device> dev,
		descriptor_heap& heap,
		const compute_entry_pod& pod
	) -> descriptor_writer {
		return descriptor_writer(registry, dev, heap, pod.layout_name);
	}

	[[nodiscard]] inline auto make_push_writer(
		shader_registry& registry,
		handle<vulkan::device> dev,
		descriptor_heap& heap,
		const graphics_entry_pod& pod
	) -> descriptor_writer {
		return descriptor_writer(registry, dev, heap, pod.layout_name);
	}

	template <fixed_string V>
	struct body_path {
		static constexpr std::string_view value = V;
	};

	template <fixed_string V>
	struct body_inline {
		static constexpr std::string_view value = V;
	};

	template <fixed_string V>
	struct layout {
		static constexpr std::string_view value = V;
	};

	template <std::uint32_t X, std::uint32_t Y = 1, std::uint32_t Z = 1>
	struct threads {
		static constexpr std::uint32_t x = X;
		static constexpr std::uint32_t y = Y;
		static constexpr std::uint32_t z = Z;
	};

	template <typename T>
	struct push_constant {
		using type = T;
	};

	template <typename... Packs>
	struct bindings {};

	template <typename... Packs>
	struct types {};

	template <fixed_string... Paths>
	struct helpers {};

	template <is_system_value... Ts>
	struct system_values {};

	template <typename T>
	struct is_body_path : std::false_type {};

	template <fixed_string V>
	struct is_body_path<body_path<V>> : std::true_type {};

	template <typename T>
	struct is_body_inline : std::false_type {};

	template <fixed_string V>
	struct is_body_inline<body_inline<V>> : std::true_type {};

	template <typename T>
	struct is_layout : std::false_type {};

	template <fixed_string V>
	struct is_layout<layout<V>> : std::true_type {};

	template <typename T>
	struct is_threads : std::false_type {};

	template <std::uint32_t X, std::uint32_t Y, std::uint32_t Z>
	struct is_threads<threads<X, Y, Z>> : std::true_type {};

	template <typename T>
	struct is_push_constant : std::false_type {};

	template <typename T>
	struct is_push_constant<push_constant<T>> : std::true_type {};

	template <typename T>
	struct is_system_values : std::false_type {};

	template <is_system_value... Ts>
	struct is_system_values<system_values<Ts...>> : std::true_type {};

	template <typename T>
	struct is_bindings : std::false_type {};

	template <typename... Packs>
	struct is_bindings<bindings<Packs...>> : std::true_type {};

	template <typename T>
	struct is_types : std::false_type {};

	template <typename... Packs>
	struct is_types<types<Packs...>> : std::true_type {};

	template <typename T>
	struct is_helpers : std::false_type {};

	template <fixed_string... Paths>
	struct is_helpers<helpers<Paths...>> : std::true_type {};

	template <typename... Specs>
	struct compute_entry {
		static constexpr compute_entry_pod pod = []() consteval {
			compute_entry_pod e{};
			auto apply = [&]<typename Spec>() consteval {
				if constexpr (is_body_path<Spec>::value) {
					e.body_path = Spec::value;
				}
				else if constexpr (is_body_inline<Spec>::value) {
					e.body_source = Spec::value;
				}
				else if constexpr (is_layout<Spec>::value) {
					e.layout_name = Spec::value;
				}
				else if constexpr (is_threads<Spec>::value) {
					e.threads_x = Spec::x;
					e.threads_y = Spec::y;
					e.threads_z = Spec::z;
				}
				else if constexpr (is_push_constant<Spec>::value) {
					using T = typename Spec::type;
					e.push_constant_size = static_cast<std::uint32_t>(sizeof(T));
					e.params[e.param_count++] = {
						.name = "pc",
						.slang_type = shaders::slang_type<T>::name,
						.semantic = {},
						.is_function_param = false,
					};
					e.emit_push_constant_struct = +[]() -> std::string {
						return shaders::emit_slang_struct<T>();
					};
				}
				else if constexpr (is_system_values<Spec>::value) {
					[&]<is_system_value... SVs>(system_values<SVs...>) consteval {
						(([&]() consteval {
							 e.params[e.param_count++] = {
								 .name = default_sv_name<SVs>(),
								 .slang_type = system_value_type_name<SVs>(),
								 .semantic = system_value_semantic<SVs>(),
								 .is_function_param = true,
							 };
						 }()),
						 ...);
					}(Spec{});
				}
				else if constexpr (is_bindings<Spec>::value) {
					e.emit_bindings = +[]() -> std::string {
						std::string out;
						[&]<typename... Packs>(bindings<Packs...>) {
							((out.append(shaders::emit_pack_bindings<Packs>())), ...);
						}(Spec{});
						return out;
					};
					e.build_family_sets_fn = +[]() -> std::vector<shaders::family_set> {
						return []<typename... Packs>(bindings<Packs...>) {
							return shaders::build_combined_family_sets<Packs...>();
						}(Spec{});
					};
				}
				else if constexpr (is_types<Spec>::value) {
					e.emit_types = +[]() -> std::string {
						std::string out;
						[&]<typename... Packs>(types<Packs...>) {
							((out.append(shaders::emit_pack_types<Packs>())), ...);
						}(Spec{});
						return out;
					};
				}
				else if constexpr (is_helpers<Spec>::value) {
					[&]<fixed_string... Paths>(helpers<Paths...>) consteval {
						((e.helper_paths[e.helper_count++] = Paths), ...);
					}(Spec{});
				}
				else {
					static_assert(sizeof(Spec) == 0, "unknown compute_entry spec");
				}
			};
			(apply.template operator()<Specs>(), ...);
			return e;
		}();
	};

	template <fixed_string EntryName>
	struct vertex_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::vertex;
		static constexpr stage_flag flag = stage_flag::vertex;
	};

	template <fixed_string EntryName>
	struct fragment_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::fragment;
		static constexpr stage_flag flag = stage_flag::fragment;
	};

	template <fixed_string EntryName>
	struct amplification_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::amplification;
		static constexpr stage_flag flag = stage_flag::task;
	};

	template <fixed_string EntryName>
	struct mesh_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::mesh;
		static constexpr stage_flag flag = stage_flag::mesh;
	};

	template <polygon_mode Polygon = polygon_mode::fill, cull_mode Cull = cull_mode::back>
	struct rasterization {
		static constexpr polygon_mode polygon = Polygon;
		static constexpr cull_mode cull = Cull;
	};

	template <bool Test = true, bool Write = true, compare_op Op = compare_op::less>
	struct depth {
		static constexpr bool test = Test;
		static constexpr bool write = Write;
		static constexpr compare_op op = Op;
	};

	template <blend_preset B = blend_preset::none>
	struct blend {
		static constexpr blend_preset value = B;
	};

	template <topology T = topology::triangle_list>
	struct primitive_topology {
		static constexpr topology value = T;
	};

	template <color_format F = color_format::swapchain>
	struct color_target {
		static constexpr color_format value = F;
	};

	template <depth_format F = depth_format::d32_sfloat>
	struct depth_target {
		static constexpr depth_format value = F;
	};

	template <typename T>
	struct is_vertex_stage : std::false_type {};
	template <fixed_string N>
	struct is_vertex_stage<vertex_stage<N>> : std::true_type {};

	template <typename T>
	struct is_fragment_stage : std::false_type {};
	template <fixed_string N>
	struct is_fragment_stage<fragment_stage<N>> : std::true_type {};

	template <typename T>
	struct is_amplification_stage : std::false_type {};
	template <fixed_string N>
	struct is_amplification_stage<amplification_stage<N>> : std::true_type {};

	template <typename T>
	struct is_mesh_stage : std::false_type {};
	template <fixed_string N>
	struct is_mesh_stage<mesh_stage<N>> : std::true_type {};

	template <typename T>
	struct is_rasterization : std::false_type {};
	template <polygon_mode P, cull_mode C>
	struct is_rasterization<rasterization<P, C>> : std::true_type {};

	template <typename T>
	struct is_depth_spec : std::false_type {};
	template <bool Te, bool W, compare_op O>
	struct is_depth_spec<depth<Te, W, O>> : std::true_type {};

	template <typename T>
	struct is_blend_spec : std::false_type {};
	template <blend_preset B>
	struct is_blend_spec<blend<B>> : std::true_type {};

	template <typename T>
	struct is_primitive_topology : std::false_type {};
	template <topology Tv>
	struct is_primitive_topology<primitive_topology<Tv>> : std::true_type {};

	template <typename T>
	struct is_color_target : std::false_type {};
	template <color_format F>
	struct is_color_target<color_target<F>> : std::true_type {};

	template <typename T>
	struct is_depth_target : std::false_type {};
	template <depth_format F>
	struct is_depth_target<depth_target<F>> : std::true_type {};

	template <typename... Specs>
	struct graphics_entry {
		static constexpr graphics_entry_pod pod = []() consteval {
			graphics_entry_pod e{};
			auto apply = [&]<typename Spec>() consteval {
				if constexpr (is_body_path<Spec>::value) {
					e.body_path = Spec::value;
				}
				else if constexpr (is_body_inline<Spec>::value) {
					e.body_source = Spec::value;
				}
				else if constexpr (is_layout<Spec>::value) {
					e.layout_name = Spec::value;
				}
				else if constexpr (is_push_constant<Spec>::value) {
					using T = typename Spec::type;
					e.push_constant_size = static_cast<std::uint32_t>(sizeof(T));
					e.push_constant_type_name = shaders::slang_type<T>::name;
					e.emit_push_constant_struct = +[]() -> std::string {
						return shaders::emit_slang_struct<T>();
					};
				}
				else if constexpr (is_vertex_stage<Spec>::value || is_fragment_stage<Spec>::value ||
								   is_amplification_stage<Spec>::value || is_mesh_stage<Spec>::value) {
					e.stages[e.stage_count++] = {
						.entry_point = Spec::entry_point,
						.kind = Spec::kind,
						.stage_flag_value = Spec::flag,
					};
					e.push_constant_stages |= static_cast<std::uint32_t>(Spec::flag);
				}
				else if constexpr (is_rasterization<Spec>::value) {
					e.rasterization.polygon = Spec::polygon;
					e.rasterization.cull = Spec::cull;
				}
				else if constexpr (is_depth_spec<Spec>::value) {
					e.depth.test = Spec::test;
					e.depth.write = Spec::write;
					e.depth.compare = Spec::op;
				}
				else if constexpr (is_blend_spec<Spec>::value) {
					e.blend = Spec::value;
				}
				else if constexpr (is_primitive_topology<Spec>::value) {
					e.topology_value = Spec::value;
				}
				else if constexpr (is_color_target<Spec>::value) {
					e.color = Spec::value;
				}
				else if constexpr (is_depth_target<Spec>::value) {
					e.depth_fmt = Spec::value;
				}
				else if constexpr (is_bindings<Spec>::value) {
					e.emit_bindings = +[]() -> std::string {
						std::string out;
						[&]<typename... Packs>(bindings<Packs...>) {
							((out.append(shaders::emit_pack_bindings<Packs>())), ...);
						}(Spec{});
						return out;
					};
					e.build_family_sets_fn = +[]() -> std::vector<shaders::family_set> {
						return []<typename... Packs>(bindings<Packs...>) {
							return shaders::build_combined_family_sets<Packs...>();
						}(Spec{});
					};
				}
				else if constexpr (is_types<Spec>::value) {
					e.emit_types = +[]() -> std::string {
						std::string out;
						[&]<typename... Packs>(types<Packs...>) {
							((out.append(shaders::emit_pack_types<Packs>())), ...);
						}(Spec{});
						return out;
					};
				}
				else if constexpr (is_helpers<Spec>::value) {
					[&]<fixed_string... Paths>(helpers<Paths...>) consteval {
						((e.helper_paths[e.helper_count++] = Paths), ...);
					}(Spec{});
				}
				else {
					static_assert(sizeof(Spec) == 0, "unknown graphics_entry spec");
				}
			};
			(apply.template operator()<Specs>(), ...);
			return e;
		}();
	};
}

namespace gse::gpu {
	auto lookup_descriptor_type(const family_layout& family, const std::uint32_t set, const std::uint32_t slot)
		-> descriptor_type {
		for (const auto& fs : family.sets) {
			if (static_cast<std::uint32_t>(fs.type) != set) {
				continue;
			}
			for (const auto& b : fs.bindings) {
				if (b.desc.binding == slot) {
					return b.desc.type;
				}
			}
		}
		return descriptor_type::storage_buffer;
	}

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
		std::string layout_name;
		std::string body_path;
		std::string inline_source;
		std::vector<std::string> helper_paths;
		std::uint32_t push_constant_size = 0;
		std::string (*emit_push_constant_struct)() = nullptr;
		std::string (*emit_types)() = nullptr;
		std::string (*emit_bindings)() = nullptr;
	};

	struct owned_slang_session {
		Slang::ComPtr<slang::IGlobalSession> global;
		Slang::ComPtr<slang::ISession> session;
	};

	auto make_slang_session() -> owned_slang_session;

	auto log_slang_diagnostics(slang::IBlob* diagnostics) -> void;

	struct parsed_body {
		std::vector<std::string> imports;
		std::string preamble;
		std::string body;
	};

	auto parse_body_file(std::string_view body_source) -> parsed_body;

	auto load_body_file(std::string_view body_path) -> std::string;

	auto load_helper_file(std::string_view helper_path) -> std::string;

	auto inline_helpers(const std::vector<std::string>& helper_paths) -> std::string;

	auto build_compute_wrapper_source(const shader_compile_inputs& inputs, const parsed_body& parsed) -> std::string;

	[[nodiscard]] auto compile_compute_spirv(const shader_compile_inputs& inputs, std::string_view wrapper_source)
		-> std::vector<std::uint32_t>;

	[[nodiscard]] auto create_compute_pipeline_from_spirv(
		device& dev,
		shader_registry& registry,
		bindless_texture_set& bindless,
		std::span<const std::uint32_t> spirv,
		std::string_view layout_name,
		std::uint32_t push_constant_size
	) -> pipeline;

	[[nodiscard]] auto build_compute_pipeline_impl(
		device& dev,
		shader_registry& registry,
		bindless_texture_set& bindless,
		const shader_compile_inputs& inputs
	) -> pipeline;

	struct graphics_stage_compile_result {
		stage_flag flag = stage_flag::vertex;
		graphics_stage_kind kind = graphics_stage_kind::vertex;
		std::string entry_point;
		std::vector<std::uint32_t> spirv;
	};

	struct compiled_graphics_program {
		std::vector<graphics_stage_compile_result> stages;
		std::vector<vertex_attribute_desc> vertex_attributes;
		std::vector<vertex_binding_desc> vertex_bindings;
	};

	auto build_graphics_wrapper_source(const graphics_entry_pod& pod, const parsed_body& parsed) -> std::string;

	[[nodiscard]] auto compile_graphics_program(const graphics_entry_pod& pod, std::string_view wrapper_source)
		-> compiled_graphics_program;
}

auto gse::gpu::make_slang_session() -> owned_slang_session {
	owned_slang_session out;
	if (slang_failed(createGlobalSession(out.global.writeRef())) || !out.global) {
		log::println(
			log::level::error,
			log::category::assets,
			"Failed to create Slang global session for pipeline builder"
		);
		return out;
	}
	auto* global = out.global.get();

	const auto shader_root = config::resource_path / "Shaders";

	auto contains_bodies = [](const std::filesystem::path& p) -> bool {
		for (const auto& part : p) {
			if (part == "Bodies") {
				return true;
			}
		}
		return false;
	};

	std::vector<std::string> sp_storage;
	sp_storage.push_back(shader_root.string());
	for (const auto& e : std::filesystem::recursive_directory_iterator(shader_root)) {
		if (!e.is_directory()) {
			continue;
		}
		if (contains_bodies(e.path())) {
			continue;
		}
		sp_storage.push_back(e.path().string());
	}

	std::vector<const char*> sp_c_strs;
	sp_c_strs.reserve(sp_storage.size());
	for (auto& s : sp_storage) {
		sp_c_strs.push_back(s.c_str());
	}

	slang::TargetDesc target{
		.format = slang_spirv,
		.profile = global->findProfile("spirv_1_5"),
		.forceGLSLScalarBufferLayout = true,
	};

	slang::SessionDesc sdesc{
		.targets = &target,
		.targetCount = 1,
		.defaultMatrixLayoutMode = slang_matrix_layout_column_major,
		.searchPaths = sp_c_strs.data(),
		.searchPathCount = static_cast<SlangInt>(sp_c_strs.size()),
	};

	if (slang_failed(global->createSession(sdesc, out.session.writeRef())) || !out.session) {
		log::println(log::level::error, log::category::assets, "Failed to create Slang session for pipeline builder");
		return owned_slang_session{};
	}
	return out;
}

auto gse::gpu::log_slang_diagnostics(slang::IBlob* diagnostics) -> void {
	if (!diagnostics || diagnostics->getBufferSize() == 0) {
		return;
	}
	const std::string message(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());
	log::println(log::level::error, log::category::assets, "{}", message);
}

auto gse::gpu::parse_body_file(const std::string_view body_source) -> parsed_body {
	constexpr std::string_view entry_marker = "// entry:";

	parsed_body result;
	std::string pre_entry;
	std::string post_entry;
	std::string* current_section = &pre_entry;
	std::size_t cursor = 0;
	bool seen_non_import = false;
	bool found_marker = false;

	while (cursor < body_source.size()) {
		const std::size_t line_end = body_source.find('\n', cursor);
		const std::size_t take = (line_end == std::string_view::npos) ? body_source.size() : line_end;
		std::string_view line = body_source.substr(cursor, take - cursor);

		std::string_view trimmed = line;
		while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
			trimmed.remove_prefix(1);
		}

		if (trimmed.starts_with(entry_marker)) {
			found_marker = true;
			current_section = &post_entry;
		}
		else if (!seen_non_import && (trimmed.starts_with("import ") || trimmed.starts_with("__exported import "))) {
			result.imports.emplace_back(line);
		}
		else if (!seen_non_import && trimmed.empty()) {
		}
		else {
			seen_non_import = true;
			current_section->append(line);
			current_section->push_back('\n');
		}

		if (line_end == std::string_view::npos) {
			break;
		}
		cursor = line_end + 1;
	}

	if (found_marker) {
		result.preamble = std::move(pre_entry);
		result.body = std::move(post_entry);
	}
	else {
		result.body = std::move(pre_entry);
	}
	return result;
}

auto gse::gpu::load_body_file(const std::string_view body_path) -> std::string {
	const auto full_path = config::resource_path / "Shaders" / "Bodies" / (std::string(body_path) + ".slang");
	std::ifstream in(full_path, std::ios::binary);
	assert(in.is_open(), "Failed to open shader body: {}", full_path.string());

	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

auto gse::gpu::load_helper_file(const std::string_view helper_path) -> std::string {
	const auto full_path = config::resource_path / "Shaders" / (std::string(helper_path) + ".slang");
	std::ifstream in(full_path, std::ios::binary);
	assert(in.is_open(), "Failed to open shader helper: {}", full_path.string());

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

auto gse::gpu::build_compute_wrapper_source(const shader_compile_inputs& inputs, const parsed_body& parsed)
	-> std::string {
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

	if (!inputs.helper_paths.empty()) {
		out.append(inline_helpers(inputs.helper_paths));
		out.push_back('\n');
	}

	if (!parsed.preamble.empty()) {
		out.append(parsed.preamble);
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
	out.append(parsed.body);
	out.append("}\n");
	return out;
}

auto gse::gpu::compile_compute_spirv(const shader_compile_inputs& inputs, const std::string_view wrapper_source)
	-> std::vector<std::uint32_t> {
	auto owned = make_slang_session();
	auto* session = owned.session.get();
	assert(session, "Slang session not available");

	const std::string module_name = std::format("entry_{}", inputs.body_path);
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
	return spirv;
}

auto gse::gpu::create_compute_pipeline_from_spirv(
	device& dev,
	shader_registry& registry,
	bindless_texture_set& bindless,
	const std::span<const std::uint32_t> spirv,
	const std::string_view layout_name,
	const std::uint32_t push_constant_size
) -> pipeline {
	const auto* family = registry.find_family(layout_name);
	assert(family, "Shader family layout not registered: {}", layout_name);

	auto layouts = family->layout_handles;
	constexpr auto bindless_idx = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	std::vector<std::uint32_t> auto_bound_sets;
	if (layouts.size() > bindless_idx) {
		layouts[bindless_idx] = bindless.layout_handle();
		auto_bound_sets.push_back(bindless_idx);
	}

	std::optional<push_constant_range> push_range;
	if (push_constant_size > 0) {
		push_range = push_constant_range{
			.stages = stage_flag::compute,
			.offset = 0,
			.size = push_constant_size,
		};
	}

	auto module_handle = shader_module::create(dev.vulkan_device(), spirv);
	const shader_stage_create_info compute_stage{
		.stage = stage_flag::compute,
		.module_handle = module_handle.handle().value,
		.entry_point = "main",
	};

	std::vector<binding_use> active_bindings;
	for (const auto& use : used_bindings(spirv)) {
		active_bindings.push_back(
			{
				.set = use.set,
				.slot = use.slot,
				.access = use.access,
				.type = lookup_descriptor_type(*family, use.set, use.slot),
				.stages = gpu::pipeline_stage_flag::compute_shader,
			}
		);
	}

	vulkan::compute_pipeline_create_info info{
		.stage = compute_stage,
		.set_layouts = layouts,
		.push_constant_range = push_range,
		.auto_bound_sets = auto_bound_sets,
		.active_bindings = active_bindings,
	};

	return vulkan::pipeline::create_compute(dev.vulkan_device(), info);
}

auto gse::gpu::build_compute_pipeline_impl(
	device& dev,
	shader_registry& registry,
	bindless_texture_set& bindless,
	const shader_compile_inputs& inputs
) -> pipeline {
	assert(!inputs.body_path.empty(), "shader_fn annotation missing on compute entry");
	assert(!inputs.layout_name.empty(), "shader_layout annotation missing on compute entry");

	const auto body_source = inputs.inline_source.empty() ? load_body_file(inputs.body_path) : inputs.inline_source;
	const auto parsed = parse_body_file(body_source);
	const auto wrapper_source = build_compute_wrapper_source(inputs, parsed);

	const auto spirv = compile_compute_spirv(inputs, wrapper_source);
	return create_compute_pipeline_from_spirv(
		dev,
		registry,
		bindless,
		spirv,
		inputs.layout_name,
		inputs.push_constant_size
	);
}

auto gse::gpu::build_compute_pipeline(
	device& dev,
	shader_registry& registry,
	bindless_texture_set& bindless,
	const compute_entry_pod& pod
) -> pipeline {
	assert(pod.build_family_sets_fn, "bindings missing on compute entry");
	registry.register_family(std::string(pod.layout_name), pod.build_family_sets_fn());

	shader_compile_inputs inputs;
	inputs.body_path = std::string(pod.body_path);
	inputs.inline_source = std::string(pod.body_source);
	inputs.layout_name = std::string(pod.layout_name);
	inputs.threads_x = pod.threads_x;
	inputs.threads_y = pod.threads_y;
	inputs.threads_z = pod.threads_z;
	inputs.push_constant_size = pod.push_constant_size;
	inputs.emit_push_constant_struct = pod.emit_push_constant_struct;
	inputs.emit_types = pod.emit_types;
	inputs.emit_bindings = pod.emit_bindings;
	inputs.helper_paths.reserve(pod.helper_count);
	for (std::size_t i = 0; i < pod.helper_count; ++i) {
		inputs.helper_paths.emplace_back(pod.helper_paths[i]);
	}
	inputs.params.reserve(pod.param_count);
	for (std::size_t i = 0; i < pod.param_count; ++i) {
		inputs.params.push_back(
			{
				.name = std::string(pod.params[i].name),
				.slang_type = std::string(pod.params[i].slang_type),
				.semantic = std::string(pod.params[i].semantic),
				.is_function_param = pod.params[i].is_function_param,
			}
		);
	}
	return build_compute_pipeline_impl(dev, registry, bindless, inputs);
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

	if (pod.helper_count > 0) {
		std::vector<std::string> helper_paths;
		helper_paths.reserve(pod.helper_count);
		for (std::size_t i = 0; i < pod.helper_count; ++i) {
			helper_paths.emplace_back(pod.helper_paths[i]);
		}
		out.append(inline_helpers(helper_paths));
		out.push_back('\n');
	}

	if (!parsed.preamble.empty()) {
		out.append(parsed.preamble);
		out.push_back('\n');
	}

	if (pod.push_constant_size > 0) {
		out.append("[[vk::push_constant]]\nConstantBuffer<");
		out.append(pod.push_constant_type_name);
		out.append("> pc;\n\n");
	}

	out.append(parsed.body);
	return out;
}

namespace gse::gpu {
	auto to_vertex_format_from_slang(slang::TypeReflection* ty) -> vertex_format {
		if (!ty) {
			return vertex_format::r32_sfloat;
		}
		const auto scalar = ty->getScalarType();
		const auto rows = ty->getRowCount();
		const auto cols = ty->getColumnCount();
		const auto elements = (rows == 0 ? 1u : rows) * (cols == 0 ? 1u : cols);
		using k = slang::TypeReflection::ScalarType;
		if (scalar == k::Float32) {
			switch (elements) {
				case 1:
					return vertex_format::r32_sfloat;
				case 2:
					return vertex_format::r32g32_sfloat;
				case 3:
					return vertex_format::r32g32b32_sfloat;
				case 4:
					return vertex_format::r32g32b32a32_sfloat;
				default:
					return vertex_format::r32g32b32_sfloat;
			}
		}
		if (scalar == k::Int32) {
			switch (elements) {
				case 1:
					return vertex_format::r32_sint;
				case 2:
					return vertex_format::r32g32_sint;
				case 3:
					return vertex_format::r32g32b32_sint;
				case 4:
					return vertex_format::r32g32b32a32_sint;
				default:
					return vertex_format::r32g32b32_sint;
			}
		}
		if (scalar == k::UInt32) {
			switch (elements) {
				case 1:
					return vertex_format::r32_uint;
				case 2:
					return vertex_format::r32g32_uint;
				case 3:
					return vertex_format::r32g32b32_uint;
				case 4:
					return vertex_format::r32g32b32a32_uint;
				default:
					return vertex_format::r32g32b32_uint;
			}
		}
		return vertex_format::r32_sfloat;
	}

	auto byte_size_of_vertex_format(vertex_format fmt) -> std::uint32_t {
		switch (fmt) {
			case vertex_format::r32_sfloat:
			case vertex_format::r32_sint:
			case vertex_format::r32_uint:
				return 4;
			case vertex_format::r32g32_sfloat:
			case vertex_format::r32g32_sint:
			case vertex_format::r32g32_uint:
				return 8;
			case vertex_format::r32g32b32_sfloat:
			case vertex_format::r32g32b32_sint:
			case vertex_format::r32g32b32_uint:
				return 12;
			case vertex_format::r32g32b32a32_sfloat:
			case vertex_format::r32g32b32a32_sint:
			case vertex_format::r32g32b32a32_uint:
				return 16;
		}
		return 4;
	}
}

auto gse::gpu::compile_graphics_program(const graphics_entry_pod& pod, const std::string_view wrapper_source)
	-> compiled_graphics_program {
	auto owned = make_slang_session();
	auto* session = owned.session.get();
	assert(session, "Slang session not available");

	const std::string module_name = std::format("gfx_{}", pod.body_path);
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
			if (!std::strcmp(
					candidate->getFunctionReflection()->getName(),
					std::string(stage_pod.entry_point).c_str()
				)) {
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
	if (slang_failed(session->createCompositeComponentType(
			parts.data(),
			static_cast<SlangInt>(parts.size()),
			composed.writeRef(),
			diags.writeRef()
		))) {
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
		if (slang_failed(
				program->getEntryPointCode(static_cast<SlangInt>(ep_indices[i]), 0, blob.writeRef(), diags.writeRef())
			) ||
			!blob) {
			log_slang_diagnostics(diags.get());
			dump_wrapper_source();
			assert(false, "Failed to get SPIR-V for graphics entry point '{}' in {}", stage_pod.entry_point, sanitized);
		}

		const auto byte_size = blob->getBufferSize();
		std::vector<std::uint32_t> spirv(byte_size / sizeof(std::uint32_t));
		std::memcpy(spirv.data(), blob->getBufferPointer(), byte_size);

		result.stages.push_back(
			{
				.flag = stage_pod.stage_flag_value,
				.kind = stage_pod.kind,
				.entry_point = std::string(stage_pod.entry_point),
				.spirv = std::move(spirv),
			}
		);

		if (stage_pod.kind == graphics_stage_kind::vertex) {
			auto* layout_reflection = program->getLayout();
			if (layout_reflection &&
				static_cast<std::size_t>(layout_reflection->getEntryPointCount()) > ep_indices[i]) {
				auto* ep_layout = layout_reflection->getEntryPointByIndex(static_cast<SlangInt>(ep_indices[i]));
				auto* scope_vl = ep_layout ? ep_layout->getVarLayout() : nullptr;
				auto* scope_tl = scope_vl ? scope_vl->getTypeLayout() : nullptr;
				if (scope_tl && scope_tl->getKind() == slang::TypeReflection::Kind::Struct) {
					std::uint32_t offset = 0;
					std::uint32_t next_location = 0;
					for (int j = 0; j < scope_tl->getFieldCount(); ++j) {
						auto* vl = scope_tl->getFieldByIndex(j);
						if (!vl) {
							continue;
						}
						if (const char* sem = vl->getSemanticName()) {
							if (!std::strncmp(sem, "SV_", 3)) {
								continue;
							}
						}
						auto* tl = vl->getTypeLayout();
						if (!tl) {
							continue;
						}
						const auto fmt = to_vertex_format_from_slang(tl->getType());
						const auto attr_size = byte_size_of_vertex_format(fmt);
						result.vertex_attributes.push_back(
							{
								.location = next_location++,
								.binding = 0,
								.format = fmt,
								.offset = offset,
							}
						);
						offset += attr_size;
					}
					if (offset > 0) {
						result.vertex_bindings.push_back(
							{
								.binding = 0,
								.stride = offset,
								.per_instance = false,
							}
						);
					}
				}
			}
		}
	}

	return result;
}

auto gse::gpu::build_graphics_pipeline(
	device& dev,
	shader_registry& registry,
	bindless_texture_set& bindless,
	const graphics_entry_pod& pod
) -> pipeline {
	assert(!pod.body_path.empty(), "body_path missing on graphics entry");
	assert(!pod.layout_name.empty(), "layout missing on graphics entry");
	assert(pod.stage_count > 0, "graphics entry has no stages");
	assert(pod.build_family_sets_fn, "bindings missing on graphics entry");
	registry.register_family(std::string(pod.layout_name), pod.build_family_sets_fn());

	const std::string body_source =
		pod.body_source.empty() ? load_body_file(pod.body_path) : std::string(pod.body_source);
	const auto parsed = parse_body_file(body_source);
	const auto wrapper_source = build_graphics_wrapper_source(pod, parsed);

	auto program = compile_graphics_program(pod, wrapper_source);

	const auto* family = registry.find_family(pod.layout_name);
	assert(family, "Shader family layout not registered: {}", pod.layout_name);

	auto layouts = family->layout_handles;
	constexpr auto bindless_idx = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	std::vector<std::uint32_t> auto_bound_sets;
	if (layouts.size() > bindless_idx) {
		layouts[bindless_idx] = bindless.layout_handle();
		auto_bound_sets.push_back(bindless_idx);
	}

	std::optional<push_constant_range> push_range;
	if (pod.push_constant_size > 0) {
		push_range = push_constant_range{
			.stages = stage_flags::from_bits(static_cast<std::uint8_t>(pod.push_constant_stages)),
			.offset = 0,
			.size = pod.push_constant_size,
		};
	}

	std::vector<shader_module> modules;
	modules.reserve(program.stages.size());
	std::vector<shader_stage_create_info> stage_infos;
	stage_infos.reserve(program.stages.size());
	bool is_mesh = false;
	std::vector<binding_use> active_bindings;

	for (auto& s : program.stages) {
		auto module_handle = shader_module::create(dev.vulkan_device(), s.spirv);
		stage_infos.push_back(
			{
				.stage = s.flag,
				.module_handle = module_handle.handle().value,
				.entry_point = "main",
			}
		);
		const auto stage_pipeline = to_pipeline_stage(s.flag);
		for (const auto& use : used_bindings(s.spirv)) {
			const auto it = std::ranges::find_if(active_bindings, [&](const binding_use& existing) {
				return existing.set == use.set && existing.slot == use.slot;
			});
			if (it == active_bindings.end()) {
				active_bindings.push_back(
					{
						.set = use.set,
						.slot = use.slot,
						.access = use.access,
						.type = lookup_descriptor_type(*family, use.set, use.slot),
						.stages = stage_pipeline,
					}
				);
			}
			else {
				it->stages |= stage_pipeline;
				if (use.access == descriptor_access::read_write) {
					it->access = descriptor_access::read_write;
				}
			}
		}
		modules.push_back(std::move(module_handle));
		if (s.kind == graphics_stage_kind::amplification || s.kind == graphics_stage_kind::mesh) {
			is_mesh = true;
		}
	}

	image_format color_format_value = image_format::r8g8b8a8_unorm;
	if (pod.color == color_format::swapchain) {
		color_format_value = dev.surface_format();
	}

	const bool has_color = pod.color != color_format::none;
	const bool has_depth = pod.depth_fmt != depth_format::none;

	vulkan::graphics_pipeline_create_info info{
		.stages = stage_infos,
		.vertex_bindings = program.vertex_bindings,
		.vertex_attributes = program.vertex_attributes,
		.set_layouts = layouts,
		.push_constant_range = push_range,
		.rasterization = pod.rasterization,
		.depth = pod.depth,
		.blend = pod.blend,
		.topology = pod.topology_value,
		.color_format = color_format_value,
		.depth_format = image_format::d32_sfloat,
		.has_color = has_color,
		.has_depth = has_depth,
		.is_mesh_pipeline = is_mesh,
		.auto_bound_sets = auto_bound_sets,
		.active_bindings = active_bindings,
	};

	return vulkan::pipeline::create_graphics(dev.vulkan_device(), info);
}

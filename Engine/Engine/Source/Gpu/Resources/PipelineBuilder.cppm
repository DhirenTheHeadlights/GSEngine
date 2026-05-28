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
import gse.vulkan;
import :device;
import :bindless;
import :shader_codegen;
import :shader_markers;

export namespace gse::gpu {
	struct combined_sampler_arg {
		bindless_slot image;
		bindless_slot sampler;
	};

	template <typename T>
	constexpr auto descriptor_type_v = shaders::descriptor_type_of<T>();

	template <typename T>
	constexpr auto descriptor_count_v = shaders::descriptor_count_of<T>();

	consteval auto binding_arg_type(
		std::meta::info t
	) -> std::meta::info;

	template <typename Pack>
	struct binding_args_aggregate {
		struct type;

		consteval {
			struct sortable {
				std::meta::info t;
				std::uint32_t set;
				std::uint32_t slot;
			};
			std::vector<sortable> entries;
			constexpr auto pack_types = []<typename... Ts>(type_pack<Ts...>) {
				return std::array{ ^^Ts... };
			}(Pack{});
			for (const auto t : pack_types) {
				const auto count = std::meta::extract<std::uint32_t>(std::meta::substitute(
					^^descriptor_count_v,
					{
						t }
				));
				if (count > 1) {
					continue;
				}
				const auto bt = shaders::find_binding_type(t);
				const auto targs = std::meta::template_arguments_of(bt);
				const auto set = std::meta::extract<std::uint32_t>(targs[0]);
				const auto slot = std::meta::extract<std::uint32_t>(targs[1]);
				entries.push_back({ t, set, slot });
			}
			std::ranges::sort(
				entries,
				[](const sortable& a, const sortable& b) {
					if (a.set != b.set) {
						return a.set < b.set;
					}
					return a.slot < b.slot;
				}
			);
			std::vector<std::meta::info> members;
			for (const auto& e : entries) {
				members.push_back(std::meta::data_member_spec(
					binding_arg_type(e.t),
					{
						.name = std::meta::identifier_of(e.t),
					}
				));
			}
			std::meta::define_aggregate(^^type, members);
		}
	};

	template <typename Pack>
	using binding_args = typename binding_args_aggregate<Pack>::type;

	template <typename... Packs>
	struct bindings;

	template <typename T>
	struct push_constant;

	template <typename Entry>
	consteval auto entry_bindings_pack_info() -> std::meta::info {
		for (const auto a : std::meta::template_arguments_of(^^Entry)) {
			if (std::meta::has_template_arguments(a) && std::meta::template_of(a) == ^^bindings) {
				return std::meta::template_arguments_of(a)[0];
			}
		}
		return {};
	}

	template <typename Entry>
	consteval auto entry_push_constants_info() -> std::meta::info {
		for (const auto a : std::meta::template_arguments_of(^^Entry)) {
			if (std::meta::has_template_arguments(a) && std::meta::template_of(a) == ^^push_constant) {
				return std::meta::template_arguments_of(a)[0];
			}
		}
		return {};
	}

	template <typename Entry>
	using entry_bindings_pack_t = [:entry_bindings_pack_info<Entry>():];

	template <typename Entry>
	using entry_push_constants_t = [:entry_push_constants_info<Entry>():];

	struct compute_param_pod {
		std::string_view name;
		std::string_view slang_type;
		std::string_view semantic;
		bool is_function_param = false;
	};

	struct compute_entry_pod {
		std::string_view body_path;
		std::string_view body_source;
		std::uint32_t threads_x = 1;
		std::uint32_t threads_y = 1;
		std::uint32_t threads_z = 1;
		std::uint32_t push_constant_size = 0;
		std::uint32_t required_subgroup_size = 0;
		bool require_full_subgroups = false;
		std::array<compute_param_pod, 8> params{};
		std::size_t param_count = 0;
		std::array<std::string_view, 4> helper_paths{};
		std::size_t helper_count = 0;
		std::array<std::string_view, 8> call_names{};
		std::size_t call_count = 0;
		std::uint32_t spec_data_size = 0;
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
		std::vector<shaders::spec_constant_entry> (
			*build_spec_entries_fn
		)() = nullptr;
		std::vector<shaders::family_set> (
			*build_family_sets_fn
		)() = nullptr;
	};

	[[nodiscard]]
	auto build_compute_program(
		device& dev,
		bindless_heaps& heaps,
		const compute_entry_pod& pod,
		std::span<const std::byte> spec_data = {}
	) -> shader_program;

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
		static constexpr std::size_t max_color_targets = 8;

		std::string_view body_path;
		std::string_view body_source;
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
		std::array<color_format, max_color_targets> colors{};
		std::size_t color_count = 1;
		depth_format depth_fmt = depth_format::d32_sfloat;
		std::uint32_t spec_data_size = 0;
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
		std::vector<shaders::spec_constant_entry> (
			*build_spec_entries_fn
		)() = nullptr;
		std::vector<shaders::family_set> (
			*build_family_sets_fn
		)() = nullptr;
	};

	[[nodiscard]]
	auto build_graphics_program(
		device& dev,
		bindless_heaps& heaps,
		const graphics_entry_pod& pod,
		std::span<const std::byte> spec_data = {}
	) -> shader_program;

	template <typename T>
	auto as_spec_data(
		const T& value
	) -> std::span<const std::byte>;

	template <fixed_string V>
	struct body_path {
		static constexpr std::string_view value = V;
	};

	template <fixed_string V>
	struct body_inline {
		static constexpr std::string_view value = V;
	};

	template <std::uint32_t X, std::uint32_t Y = 1, std::uint32_t Z = 1>
	struct threads {
		static constexpr std::uint32_t x = X;
		static constexpr std::uint32_t y = Y;
		static constexpr std::uint32_t z = Z;
	};

	template <std::uint32_t N>
	struct required_subgroup_size {
		static constexpr std::uint32_t value = N;
	};

	struct full_subgroups {};

	template <typename T>
	struct push_constant {
		using type = T;
	};

	template <typename T>
	struct spec_constants {
		using type = T;
	};

	template <typename... Packs>
	struct bindings {};

	template <typename... Packs>
	struct types {};

	template <fixed_string... Paths>
	struct helpers {};

	template <fixed_string... Names>
	struct calls {};

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
	struct is_threads : std::false_type {};

	template <std::uint32_t X, std::uint32_t Y, std::uint32_t Z>
	struct is_threads<threads<X, Y, Z>> : std::true_type {};

	template <typename T>
	struct is_required_subgroup_size : std::false_type {};

	template <std::uint32_t N>
	struct is_required_subgroup_size<required_subgroup_size<N>> : std::true_type {};

	template <typename T>
	struct is_full_subgroups : std::false_type {};

	template <>
	struct is_full_subgroups<full_subgroups> : std::true_type {};

	template <typename T>
	struct is_push_constant : std::false_type {};

	template <typename T>
	struct is_push_constant<push_constant<T>> : std::true_type {};

	template <typename T>
	struct is_spec_constants : std::false_type {};

	template <typename T>
	struct is_spec_constants<spec_constants<T>> : std::true_type {};

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

	template <typename T>
	struct is_calls : std::false_type {};

	template <fixed_string... Names>
	struct is_calls<calls<Names...>> : std::true_type {};

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
				else if constexpr (is_threads<Spec>::value) {
					e.threads_x = Spec::x;
					e.threads_y = Spec::y;
					e.threads_z = Spec::z;
				}
				else if constexpr (is_required_subgroup_size<Spec>::value) {
					e.required_subgroup_size = Spec::value;
				}
				else if constexpr (is_full_subgroups<Spec>::value) {
					e.require_full_subgroups = true;
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
				else if constexpr (is_spec_constants<Spec>::value) {
					using T = typename Spec::type;
					e.spec_data_size = static_cast<std::uint32_t>(sizeof(T));
					e.emit_spec_decls = +[]() -> std::string {
						return shaders::emit_slang_specialization_constants<T>();
					};
					e.build_spec_entries_fn = +[]() -> std::vector<shaders::spec_constant_entry> {
						return shaders::build_spec_constant_entries<T>();
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
				else if constexpr (is_calls<Spec>::value) {
					[&]<fixed_string... Names>(calls<Names...>) consteval {
						((e.call_names[e.call_count++] = Names), ...);
					}(Spec{});
				}
				else {
					static_assert(sizeof(Spec) == 0, "unknown compute_entry spec");
				}
			};
			(apply.template operator()<Specs>(), ...);
			if (e.call_count == 0 && !e.body_path.empty()) {
				const auto slash = e.body_path.find_last_of('/');
				e.call_names[e.call_count++] =
					(slash == std::string_view::npos) ? e.body_path : e.body_path.substr(slash + 1);
			}
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

	template <color_format... Fs>
	struct color_targets {
		static constexpr std::array<color_format, sizeof...(Fs)> values{ Fs... };
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
	struct is_color_targets : std::false_type {};
	template <color_format... Fs>
	struct is_color_targets<color_targets<Fs...>> : std::true_type {};

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
				else if constexpr (is_push_constant<Spec>::value) {
					using T = typename Spec::type;
					e.push_constant_size = static_cast<std::uint32_t>(sizeof(T));
					e.push_constant_type_name = shaders::slang_type<T>::name;
					e.emit_push_constant_struct = +[]() -> std::string {
						return shaders::emit_slang_struct<T>();
					};
				}
				else if constexpr (is_spec_constants<Spec>::value) {
					using T = typename Spec::type;
					e.spec_data_size = static_cast<std::uint32_t>(sizeof(T));
					e.emit_spec_decls = +[]() -> std::string {
						return shaders::emit_slang_specialization_constants<T>();
					};
					e.build_spec_entries_fn = +[]() -> std::vector<shaders::spec_constant_entry> {
						return shaders::build_spec_constant_entries<T>();
					};
				}
				else if constexpr (is_vertex_stage<Spec>::value || is_fragment_stage<Spec>::value || is_amplification_stage<Spec>::value || is_mesh_stage<Spec>::value) {
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
				else if constexpr (is_color_targets<Spec>::value) {
					constexpr auto vals = Spec::values;
					if constexpr (vals.size() == 1 && vals[0] == color_format::none) {
						e.color_count = 0;
					}
					else {
						for (std::size_t i = 0; i < vals.size(); ++i) {
							e.colors[i] = vals[i];
						}
						e.color_count = vals.size();
					}
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
		log::println(
			log::level::error,
			log::category::assets,
			"Failed to create Slang session for pipeline builder"
		);
		return owned_slang_session{};
	}
	return out;
}

auto gse::gpu::log_slang_diagnostics(slang::IBlob* diagnostics) -> void {
	if (!diagnostics || diagnostics->getBufferSize() == 0) {
		return;
	}
	const std::string message(
		static_cast<const char*>(diagnostics->getBufferPointer()),
		diagnostics->getBufferSize()
	);
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

consteval auto gse::gpu::binding_arg_type(const std::meta::info t) -> std::meta::info {
	const auto dt = std::meta::extract<descriptor_type>(std::meta::substitute(
		^^descriptor_type_v,
		{
			t }
	));
	if (dt == descriptor_type::combined_image_sampler) {
		return ^^combined_sampler_arg;
	}
	return ^^bindless_slot;
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
	out.append(std::format(
		"[numthreads({}, {}, {})]\n",
		inputs.threads_x,
		inputs.threads_y,
		inputs.threads_z
	));
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

auto gse::gpu::compile_compute_spirv(const shader_compile_inputs& inputs, const std::string_view wrapper_source) -> std::vector<std::uint32_t> {
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
			assert(
				false,
				"Failed to get SPIR-V for graphics entry point '{}' in {}",
				stage_pod.entry_point,
				sanitized
			);
		}

		const auto byte_size = blob->getBufferSize();
		std::vector<std::uint32_t> spirv(byte_size / sizeof(std::uint32_t));
		std::memcpy(spirv.data(), blob->getBufferPointer(), byte_size);

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

auto gse::gpu::build_compute_program(device& dev, bindless_heaps& heaps, const compute_entry_pod& pod, const std::span<const std::byte> spec_data) -> shader_program {
	assert(pod.build_family_sets_fn, "bindings missing on compute entry");
	assert(spec_data.empty() || spec_data.size() == pod.spec_data_size, "spec_data size mismatch with entry's spec_constants<T>");
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

	const auto bindless_mappings = vulkan::build_bindless_mappings(
		pack_bindings,
		heaps,
		pod.push_constant_size
	);

	std::vector<vulkan::specialization_entry> vk_spec_entries;
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

	const vulkan::shader_object_create_info stage_info{
		.stage = stage_flag::compute,
		.spirv = spirv,
		.entry_point = "main",
		.next_stage = {},
		.required_subgroup_size = pod.required_subgroup_size != 0
			? std::optional<std::uint32_t>(pod.required_subgroup_size)
			: std::nullopt,
		.require_full_subgroups = pod.require_full_subgroups,
		.bindless_mappings = bindless_mappings.mappings,
		.spec_entries = vk_spec_entries,
		.spec_data = spec_data,
	};

	const vulkan::shader_program_create_info info{
		.stages = std::span(
			&stage_info,
			1
		),
		.push_constant_range = push_range,
		.state = {},
		.is_compute = true,
		.is_mesh = false,
	};

	return dev.create_shader_program(info);
}

auto gse::gpu::build_graphics_program(device& dev, bindless_heaps& heaps, const graphics_entry_pod& pod, const std::span<const std::byte> spec_data) -> shader_program {
	assert(!pod.body_path.empty(), "body_path missing on graphics entry");
	assert(pod.stage_count > 0, "graphics entry has no stages");
	assert(pod.build_family_sets_fn, "bindings missing on graphics entry");
	assert(spec_data.empty() || spec_data.size() == pod.spec_data_size, "spec_data size mismatch with entry's spec_constants<T>");
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

	const auto bindless_mappings = vulkan::build_bindless_mappings(
		pack_bindings,
		heaps,
		pod.push_constant_size
	);

	std::vector<vulkan::specialization_entry> vk_spec_entries;
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

	std::vector<vulkan::shader_object_create_info> stage_infos;
	stage_infos.reserve(program.stages.size());
	for (auto& s : program.stages) {
		stage_infos.push_back({
			.stage = s.flag,
			.spirv = s.spirv,
			.entry_point = "main",
			.next_stage = next_stage_for(
				s.flag,
				all_stages
			),
			.bindless_mappings = bindless_mappings.mappings,
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

	const vulkan::shader_program_create_info info{
		.stages = stage_infos,
		.push_constant_range = push_range,
		.state = std::move(state),
		.is_compute = false,
		.is_mesh = is_mesh,
	};

	return dev.create_shader_program(info);
}

template <typename T>
auto gse::gpu::as_spec_data(const T& value) -> std::span<const std::byte> {
	return std::span<const std::byte>(reinterpret_cast<const std::byte*>(&value), sizeof(T));
}

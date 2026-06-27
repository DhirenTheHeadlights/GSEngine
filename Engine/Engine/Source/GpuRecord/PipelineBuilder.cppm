export module gse.gpu_record:pipeline_builder;

import std;
import gse.meta;
import gse.assert;
import gse.log;
import gse.core;
import gse.config;
import gse.slang;

import gse.gpu_backend;
import gse.gpu;
import gse.math;
import gse.containers;

export namespace gse::gpu {
	struct combined_sampler_arg {
		bindless_slot image;
		bindless_slot sampler;
	};

	template <typename T>
	constexpr auto descriptor_type_v = shaders::descriptor_type_of<T>();

	template <typename T>
	constexpr auto descriptor_count_v = shaders::descriptor_count_of<T>();

	template <typename T>
	constexpr auto descriptor_access_v = shaders::descriptor_access_of<T>();

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
		static_assert(
			shaders::push_constant_layout_is_portable<T>(),
			"push-constant struct straddles a 16-byte boundary: its DX12 cbuffer layout will not match the C++/Vulkan scalar layout, so fields read garbage on DX12. Reorder members so no vec/mat crosses a 16-byte boundary (e.g. place a scalar right after a vec3 to fill the block)."
		);
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
				e.call_names[e.call_count++] = (slash == std::string_view::npos) ? e.body_path : e.body_path.substr(slash + 1);
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

template <typename T>
auto gse::gpu::as_spec_data(const T& value) -> std::span<const std::byte> {
	return std::span<const std::byte>(reinterpret_cast<const std::byte*>(&value), sizeof(T));
}

export module gse.gpu_record:pipeline_builder;

import std;
import gse.meta;
import gse.assert;
import gse.log;
import gse.core;
import gse.config;

import gse.gpu_backend;
import gse.gpu;
import gse.math;
import gse.containers;

export namespace gse::gpu {
	template <typename T>
	consteval auto members_are_bindings() -> bool;

	template <typename T>
	concept is_binding_set = std::is_aggregate_v<T> && members_are_bindings<T>();

	template <is_binding_set... Sets>
	struct binding_args_aggregate {
		struct type;

		consteval {
			std::vector<std::meta::info> members;
			for (const auto set : std::array<std::meta::info, sizeof...(Sets)>{ ^^Sets... }) {
				for (const auto m : std::meta::nonstatic_data_members_of(set, std::meta::access_context::unchecked())) {
					members.push_back(std::meta::data_member_spec(
						std::meta::type_of(m),
						{
							.name = std::meta::identifier_of(m),
						}
					));
				}
			}
			std::meta::define_aggregate(^^type, members);
		}
	};

	template <is_binding_set... Sets>
	using binding_args = typename binding_args_aggregate<Sets...>::type;

	template <is_binding_set... Sets>
	struct bindings {
		using args = binding_args<Sets...>;
	};

	template <typename T>
	struct push_constant;

	template <typename Entry>
	consteval auto entry_bindings_info() -> std::meta::info {
		for (const auto a : std::meta::template_arguments_of(^^Entry)) {
			if (std::meta::has_template_arguments(a) && std::meta::template_of(a) == ^^bindings) {
				return a;
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
	using entry_binding_args = typename [:entry_bindings_info<Entry>():]::args;

	template <typename Entry>
	using entry_push_constants_t = [:entry_push_constants_info<Entry>():];

	template <typename Args>
	auto binding_layout_of() -> std::vector<binding_layout_entry>;

	template <typename Spec>
	auto collect_spec_struct_layouts(
		std::vector<shaders::struct_layout>& out
	) -> void;

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
		std::array<std::string_view, 8> helper_paths{};
		std::size_t helper_count = 0;
		std::array<std::string_view, 8> call_names{};
		std::size_t call_count = 0;
		std::uint32_t spec_data_size = 0;
		std::uint32_t binding_args_size = 0;
		bool bindless_textures = false;
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
		std::vector<specialization_entry> (
			*build_spec_entries_fn
		)() = nullptr;
		std::vector<binding_layout_entry> (
			*binding_layout_fn
		)() = nullptr;
		std::vector<shaders::struct_layout> (
			*struct_layouts_fn
		)() = nullptr;
	};

	[[nodiscard]]
	auto build_compute_program(
		device& dev,
		const compute_entry_pod& pod,
		std::span<const std::byte> spec_data = {},
		std::string_view runtime_constants = {}
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
		std::array<std::string_view, 12> helper_paths{};
		std::size_t helper_count = 0;
		rasterization_state rasterization{};
		depth_state depth{};
		blend_preset blend = blend_preset::none;
		topology topology_value = topology::triangle_list;
		std::array<color_format, max_color_targets> colors{};
		std::size_t color_count = 1;
		depth_format depth_fmt = depth_format::d32_sfloat;
		std::uint32_t spec_data_size = 0;
		std::uint32_t binding_args_size = 0;
		bool bindless_textures = false;
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
		std::vector<specialization_entry> (
			*build_spec_entries_fn
		)() = nullptr;
		std::vector<binding_layout_entry> (
			*binding_layout_fn
		)() = nullptr;
		std::vector<shaders::struct_layout> (
			*struct_layouts_fn
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

	struct bindless_textures {};

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

	template <is_binding_set... Sets>
	struct is_bindings<bindings<Sets...>> : std::true_type {};

	template <typename T>
	struct is_bindless_textures : std::false_type {};

	template <>
	struct is_bindless_textures<bindless_textures> : std::true_type {};

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

	template <typename Spec>
	concept is_common_entry_spec = is_body_path<Spec>::value
		|| is_body_inline<Spec>::value
		|| is_spec_constants<Spec>::value
		|| is_bindings<Spec>::value
		|| is_bindless_textures<Spec>::value
		|| is_types<Spec>::value
		|| is_helpers<Spec>::value;

	template <is_common_entry_spec Spec, typename Pod>
	consteval auto apply_common_entry_spec(
		Pod& e
	) -> void;

	template <typename... Specs>
	struct compute_entry {
		static constexpr compute_entry_pod pod = []() consteval {
			compute_entry_pod e{};
			auto apply = [&]<typename Spec>() consteval {
				if constexpr (is_common_entry_spec<Spec>) {
					apply_common_entry_spec<Spec>(e);
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
					e.emit_push_constant_struct = &shaders::emit_slang_struct<T>;
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
			e.struct_layouts_fn = +[]() -> std::vector<shaders::struct_layout> {
				std::vector<shaders::struct_layout> out;
				(collect_spec_struct_layouts<Specs>(out), ...);
				return out;
			};
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

	template <bool Test = true, bool Write = true, compare_op Op = compare_op::greater>
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
				if constexpr (is_common_entry_spec<Spec>) {
					apply_common_entry_spec<Spec>(e);
				}
				else if constexpr (is_push_constant<Spec>::value) {
					using T = typename Spec::type;
					e.push_constant_size = static_cast<std::uint32_t>(sizeof(T));
					e.push_constant_type_name = shaders::slang_type<T>::name;
					e.emit_push_constant_struct = &shaders::emit_slang_struct<T>;
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
				else {
					static_assert(sizeof(Spec) == 0, "unknown graphics_entry spec");
				}
			};
			(apply.template operator()<Specs>(), ...);
			e.struct_layouts_fn = +[]() -> std::vector<shaders::struct_layout> {
				std::vector<shaders::struct_layout> out;
				(collect_spec_struct_layouts<Specs>(out), ...);
				return out;
			};
			return e;
		}();
	};
}

template <typename T>
consteval auto gse::gpu::members_are_bindings() -> bool {
	bool all = true;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		if constexpr (!shaders::is_shader_binding<member_t>) {
			all = false;
		}
	}
	return all;
}

template <gse::gpu::is_common_entry_spec Spec, typename Pod>
consteval auto gse::gpu::apply_common_entry_spec(Pod& e) -> void {
	if constexpr (is_body_path<Spec>::value) {
		e.body_path = Spec::value;
	}
	else if constexpr (is_body_inline<Spec>::value) {
		e.body_source = Spec::value;
	}
	else if constexpr (is_spec_constants<Spec>::value) {
		using T = typename Spec::type;
		e.spec_data_size = static_cast<std::uint32_t>(sizeof(T));
		e.emit_spec_decls = &shaders::emit_slang_specialization_constants<T>;
		e.build_spec_entries_fn = &shaders::build_spec_constant_entries<T>;
	}
	else if constexpr (is_bindings<Spec>::value) {
		using args = typename Spec::args;
		e.binding_args_size = static_cast<std::uint32_t>(sizeof(args));
		e.emit_bindings = &shaders::emit_bindings<args>;
		e.binding_layout_fn = &binding_layout_of<args>;
	}
	else if constexpr (is_bindless_textures<Spec>::value) {
		e.bindless_textures = true;
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
}

template <typename Args>
auto gse::gpu::binding_layout_of() -> std::vector<binding_layout_entry> {
	std::vector<binding_layout_entry> out;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Args, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr auto kind = shaders::binding_info_v<member_t>.kind;
		auto offset = static_cast<std::uint32_t>(std::meta::offset_of(m).bytes);
		if constexpr (kind == binding_kind::acceleration_structure) {
			offset += static_cast<std::uint32_t>(std::meta::offset_of(^^acceleration_structure_arg::slot).bytes);
		}
		auto storage_format = image_format::undefined;
		if constexpr (kind == binding_kind::storage_texture_2d || kind == binding_kind::storage_texture_3d) {
			storage_format = shaders::storage_format_of<member_t>();
		}
		out.push_back({
			.kind = kind,
			.args_offset = offset,
			.storage_format = storage_format,
		});
	}
	return out;
}

template <typename Spec>
auto gse::gpu::collect_spec_struct_layouts(std::vector<shaders::struct_layout>& out) -> void {
	if constexpr (is_push_constant<Spec>::value) {
		using T = typename Spec::type;
		out.push_back(shaders::wgsl_struct_layout<T>(shaders::slang_type<T>::name));
	}
	else if constexpr (is_types<Spec>::value) {
		[&]<typename... Packs>(types<Packs...>) {
			(shaders::emit_pack_struct_layouts<Packs>(out), ...);
		}(Spec{});
	}
	else if constexpr (is_bindings<Spec>::value) {
		shaders::emit_binding_struct_layouts<typename Spec::args>(out);
	}
}

template <typename T>
auto gse::gpu::as_spec_data(const T& value) -> std::span<const std::byte> {
	return std::span<const std::byte>(reinterpret_cast<const std::byte*>(&value), sizeof(T));
}
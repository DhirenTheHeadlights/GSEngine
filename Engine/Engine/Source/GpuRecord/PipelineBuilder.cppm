export module gse.gpu_record:pipeline_builder;

import std;
import gse.meta;
import gse.assert;
import gse.log;
import gse.core;
import gse.config;
import gse.concurrency;

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

	template <typename... Specs>
	struct specs {};

	consteval auto append_flat_specs(std::vector<std::meta::info>& out, const std::meta::info spec) -> void {
		const auto entity = std::meta::dealias(spec);
		if (!specializes(entity, ^^specs)) {
			out.push_back(entity);
			return;
		}
		for (const auto inner : std::meta::template_arguments_of(entity)) {
			append_flat_specs(out, inner);
		}
	}

	consteval auto flat_specs(const std::meta::info holder) -> std::vector<std::meta::info> {
		std::vector<std::meta::info> out;
		for (const auto spec : std::meta::template_arguments_of(std::meta::dealias(holder))) {
			append_flat_specs(out, spec);
		}
		return out;
	}

	template <typename Entry>
	consteval auto entry_bindings_info() -> std::meta::info {
		for (const auto spec : flat_specs(^^Entry)) {
			if (specializes(spec, ^^bindings)) {
				return spec;
			}
		}
		return {};
	}

	template <typename Entry>
	consteval auto entry_push_constants_info() -> std::meta::info {
		for (const auto spec : flat_specs(^^Entry)) {
			if (specializes(spec, ^^push_constant)) {
				return std::meta::template_arguments_of(spec)[0];
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

	template <typename Spec, typename Visitor>
	auto visit_spec_types(
		Visitor& visit,
		std::vector<std::string_view>& seen
	) -> void;

	struct entry_pod {
		std::string_view body_path;
		std::string_view push_constant_type_name;
		std::uint32_t push_constant_size = 0;
		std::uint32_t binding_args_size = 0;
		bool bindless_textures = false;
		std::array<std::string_view, 12> helper_paths{};
		std::size_t helper_count = 0;
		std::string (
			*emit_types
		)() = nullptr;
		std::string (
			*emit_bindings
		)() = nullptr;
		std::vector<binding_layout_entry> (
			*binding_layout_fn
		)() = nullptr;
		std::vector<shaders::struct_layout> (
			*struct_layouts_fn
		)() = nullptr;
	};

	struct compute_entry_pod : entry_pod {
		std::uint32_t threads_x = 1;
		std::uint32_t threads_y = 1;
		std::uint32_t threads_z = 1;
	};

	[[nodiscard]]
	auto build_compute_program(
		device& dev,
		const compute_entry_pod& pod,
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

	struct graphics_entry_pod : entry_pod {
		static constexpr std::size_t max_color_targets = 8;

		std::uint32_t push_constant_stages = 0;
		std::array<graphics_stage_pod, 4> stages{};
		std::size_t stage_count = 0;
		rasterization_state rasterization{};
		depth_state depth{};
		blend_preset blend = blend_preset::none;
		topology topology_value = topology::triangle_list;
		std::array<color_format, max_color_targets> colors{};
		std::size_t color_count = 1;
	};

	[[nodiscard]]
	auto build_graphics_program(
		device& dev,
		const graphics_entry_pod& pod
	) -> shader_program;

	class program_batch : non_copyable, non_movable {
	public:
		explicit program_batch(
			device& dev
		);

		~program_batch();

		auto add(
			shader_program& out,
			const compute_entry_pod& pod,
			std::string_view runtime_constants = {}
		) -> void;

		auto add(
			shader_program& out,
			const graphics_entry_pod& pod
		) -> void;

	private:
		device& m_device;
		task::group m_group;
	};

	template <fixed_string V>
	struct body_path {
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
		static_assert(
			shaders::push_constant_layout_is_portable<T>(),
			"push-constant struct straddles a 16-byte boundary: its DX12 cbuffer layout will not match the C++/Vulkan scalar layout, so fields read garbage on DX12. Reorder members so no vec/mat crosses a 16-byte boundary (e.g. place a scalar right after a vec3 to fill the block)."
		);
		using type = T;
	};

	struct bindless_textures {};

	template <typename... Packs>
	struct types {};

	template <fixed_string... Paths>
	struct helpers {};

	template <fixed_string EntryName = "vs_main">
	struct vertex_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::vertex;
		static constexpr stage_flag flag = stage_flag::vertex;
	};

	template <fixed_string EntryName = "fs_main">
	struct fragment_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::fragment;
		static constexpr stage_flag flag = stage_flag::fragment;
	};

	template <fixed_string EntryName = "as_main">
	struct amplification_stage {
		static constexpr std::string_view entry_point = EntryName;
		static constexpr graphics_stage_kind kind = graphics_stage_kind::amplification;
		static constexpr stage_flag flag = stage_flag::task;
	};

	template <fixed_string EntryName = "ms_main">
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

	using fullscreen_pass = specs<
		helpers<"Screen/screen_pass">,
		vertex_stage<>,
		fragment_stage<>,
		rasterization<polygon_mode::fill, cull_mode::none>,
		depth<false, false>
	>;

	template <typename Spec>
	concept is_stage_spec = specializes(^^Spec, ^^vertex_stage)
		|| specializes(^^Spec, ^^fragment_stage)
		|| specializes(^^Spec, ^^amplification_stage)
		|| specializes(^^Spec, ^^mesh_stage);

	template <typename Spec, typename Pod>
	consteval auto apply_entry_spec(
		Pod& e
	) -> void;

	template <typename Pod, typename... Specs>
	consteval auto make_entry_pod() -> Pod;

	template <typename... Specs>
	struct compute_entry {
		static constexpr compute_entry_pod pod = make_entry_pod<compute_entry_pod, Specs...>();
	};

	template <typename... Specs>
	struct graphics_entry {
		static constexpr graphics_entry_pod pod = make_entry_pod<graphics_entry_pod, Specs...>();
	};

	template <typename Entry>
	auto group_count(
		vec3u extent
	) -> vec3u;

	template <typename Entry>
	auto group_count(
		vec2u extent
	) -> vec3u;

	template <typename Entry>
	auto group_count(
		std::uint32_t count
	) -> vec3u;
}

template <typename Entry>
auto gse::gpu::group_count(const vec3u extent) -> vec3u {
	constexpr auto& pod = Entry::pod;
	return {
		(extent.x() + pod.threads_x - 1) / pod.threads_x,
		(extent.y() + pod.threads_y - 1) / pod.threads_y,
		(extent.z() + pod.threads_z - 1) / pod.threads_z,
	};
}

template <typename Entry>
auto gse::gpu::group_count(const vec2u extent) -> vec3u {
	return group_count<Entry>(vec3u{ extent.x(), extent.y(), 1u });
}

template <typename Entry>
auto gse::gpu::group_count(const std::uint32_t count) -> vec3u {
	return group_count<Entry>(vec3u{ count, 1u, 1u });
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

template <typename Spec, typename Pod>
consteval auto gse::gpu::apply_entry_spec(Pod& e) -> void {
	constexpr bool compute = std::same_as<Pod, compute_entry_pod>;
	constexpr bool graphics = std::same_as<Pod, graphics_entry_pod>;
	if constexpr (specializes(^^Spec, ^^body_path)) {
		e.body_path = Spec::value;
	}
	else if constexpr (specializes(^^Spec, ^^bindings)) {
		using args = typename Spec::args;
		e.binding_args_size = static_cast<std::uint32_t>(sizeof(args));
		e.emit_bindings = &shaders::emit_bindings<args>;
		e.binding_layout_fn = &binding_layout_of<args>;
	}
	else if constexpr (std::same_as<Spec, bindless_textures>) {
		e.bindless_textures = true;
	}
	else if constexpr (specializes(^^Spec, ^^types)) {}
	else if constexpr (specializes(^^Spec, ^^helpers)) {
		[&]<fixed_string... Paths>(helpers<Paths...>) consteval {
			((e.helper_paths[e.helper_count++] = Paths), ...);
		}(Spec{});
	}
	else if constexpr (specializes(^^Spec, ^^push_constant)) {
		using T = typename Spec::type;
		e.push_constant_size = static_cast<std::uint32_t>(sizeof(T));
		e.push_constant_type_name = shaders::slang_type<T>::name;
	}
	else if constexpr (compute && specializes(^^Spec, ^^threads)) {
		e.threads_x = Spec::x;
		e.threads_y = Spec::y;
		e.threads_z = Spec::z;
	}
	else if constexpr (graphics && is_stage_spec<Spec>) {
		e.stages[e.stage_count++] = {
			.entry_point = Spec::entry_point,
			.kind = Spec::kind,
			.stage_flag_value = Spec::flag,
		};
		e.push_constant_stages |= static_cast<std::uint32_t>(Spec::flag);
	}
	else if constexpr (graphics && specializes(^^Spec, ^^rasterization)) {
		e.rasterization.polygon = Spec::polygon;
		e.rasterization.cull = Spec::cull;
	}
	else if constexpr (graphics && specializes(^^Spec, ^^depth)) {
		e.depth.test = Spec::test;
		e.depth.write = Spec::write;
		e.depth.compare = Spec::op;
	}
	else if constexpr (graphics && specializes(^^Spec, ^^blend)) {
		e.blend = Spec::value;
	}
	else if constexpr (graphics && specializes(^^Spec, ^^primitive_topology)) {
		e.topology_value = Spec::value;
	}
	else if constexpr (graphics && specializes(^^Spec, ^^color_targets)) {
		constexpr auto vals = Spec::values;
		for (std::size_t i = 0; i < vals.size(); ++i) {
			e.colors[i] = vals[i];
		}
		e.color_count = vals.size();
	}
	else {
		static_assert(sizeof(Spec) == 0, "spec is not valid for this kind of shader entry");
	}
}

template <typename Pod, typename... Specs>
consteval auto gse::gpu::make_entry_pod() -> Pod {
	Pod e{};
	template for (constexpr auto spec : std::define_static_array(flat_specs(^^specs<Specs...>))) {
		using spec_t = [:spec:];
		apply_entry_spec<spec_t, Pod>(e);
	}
	e.emit_types = +[]() -> std::string {
		std::string out;
		std::vector<std::string_view> seen;
		auto emit = [&]<typename T>() {
			shaders::emit_user_type<T>(out);
		};
		template for (constexpr auto spec : std::define_static_array(flat_specs(^^specs<Specs...>))) {
			using spec_t = [:spec:];
			visit_spec_types<spec_t>(emit, seen);
		}
		return out;
	};
	e.struct_layouts_fn = +[]() -> std::vector<shaders::struct_layout> {
		std::vector<shaders::struct_layout> out;
		std::vector<std::string_view> seen;
		auto collect = [&]<typename T>() {
			shaders::collect_struct_layout<T>(out);
		};
		template for (constexpr auto spec : std::define_static_array(flat_specs(^^specs<Specs...>))) {
			using spec_t = [:spec:];
			visit_spec_types<spec_t>(collect, seen);
		}
		return out;
	};
	return e;
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

template <typename Spec, typename Visitor>
auto gse::gpu::visit_spec_types(Visitor& visit, std::vector<std::string_view>& seen) -> void {
	if constexpr (specializes(^^Spec, ^^types)) {
		[&]<typename... Roots>(types<Roots...>) {
			(shaders::visit_shader_types<Roots>(visit, seen), ...);
		}(Spec{});
	}
	else if constexpr (specializes(^^Spec, ^^bindings)) {
		shaders::visit_shader_types<typename Spec::args>(visit, seen);
	}
	else if constexpr (specializes(^^Spec, ^^push_constant)) {
		shaders::visit_shader_types<typename Spec::type>(visit, seen);
	}
}

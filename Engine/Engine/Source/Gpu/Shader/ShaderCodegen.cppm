export module gse.gpu:shader_codegen;

import std;

import :backend_state;
import gse.assert;
import gse.math;
import gse.meta;
import gse.containers;
import gse.gpu_backend;

export namespace gse::gpu {
	struct dispatch_thread_id {
		std::uint32_t x = 0;
		std::uint32_t y = 0;
		std::uint32_t z = 0;
	};

	struct group_id {
		std::uint32_t x = 0;
		std::uint32_t y = 0;
		std::uint32_t z = 0;
	};

	struct group_thread_id {
		std::uint32_t x = 0;
		std::uint32_t y = 0;
		std::uint32_t z = 0;
	};

	struct group_index {
		std::uint32_t value = 0;
	};

	template <typename T>
	concept is_system_value = std::is_same_v<T, dispatch_thread_id> || std::is_same_v<T, group_id> ||
		std::is_same_v<T, group_thread_id> || std::is_same_v<T, group_index>;

	template <is_system_value T>
	consteval auto system_value_semantic() -> std::string_view;

	template <is_system_value T>
	consteval auto system_value_type_name() -> std::string_view;

	template <is_system_value T>
	consteval auto default_sv_name() -> std::string_view;

	struct acceleration_structure_arg {
		std::uint32_t address_lo = 0;
		std::uint32_t address_hi = 0;
		bindless_slot slot;
	};

	constexpr auto make_acceleration_structure_arg(
		device_address address,
		bindless_slot slot
	) -> acceleration_structure_arg;
}

template <gse::gpu::is_system_value T>
consteval auto gse::gpu::system_value_semantic() -> std::string_view {
	if constexpr (std::is_same_v<T, dispatch_thread_id>) {
		return "SV_DispatchThreadID";
	}
	else if constexpr (std::is_same_v<T, group_id>) {
		return "SV_GroupID";
	}
	else if constexpr (std::is_same_v<T, group_thread_id>) {
		return "SV_GroupThreadID";
	}
	else if constexpr (std::is_same_v<T, group_index>) {
		return "SV_GroupIndex";
	}
}

template <gse::gpu::is_system_value T>
consteval auto gse::gpu::system_value_type_name() -> std::string_view {
	if constexpr (std::is_same_v<T, group_index>) {
		return "uint";
	}
	else {
		return "uint3";
	}
}

template <gse::gpu::is_system_value T>
consteval auto gse::gpu::default_sv_name() -> std::string_view {
	if constexpr (std::is_same_v<T, dispatch_thread_id>) {
		return "dispatch_id";
	}
	else if constexpr (std::is_same_v<T, group_id>) {
		return "group_id";
	}
	else if constexpr (std::is_same_v<T, group_thread_id>) {
		return "group_thread_id";
	}
	else if constexpr (std::is_same_v<T, group_index>) {
		return "group_index";
	}
}

export namespace gse::shaders {
	struct shader_struct_tag {};
	struct shader_enum_tag {};
	struct shader_constant_block_tag {};

	constexpr shader_struct_tag shader_struct{};
	constexpr shader_enum_tag shader_enum{};
	constexpr shader_constant_block_tag shader_constant_block{};

	struct binding_form_info {
		gpu::binding_kind kind = gpu::binding_kind::read_only_storage_buffer;
		gpu::access_flag read = gpu::access_flag::none;
		gpu::access_flag write = gpu::access_flag::none;
	};

	enum class binding_form : std::uint8_t {
		ssbo_readonly [[= binding_form_info{
			.kind = gpu::binding_kind::read_only_storage_buffer,
			.read = gpu::access_flag::shader_storage_read,
		}]],
		ssbo_readwrite [[= binding_form_info{
			.kind = gpu::binding_kind::storage_buffer,
			.read = gpu::access_flag::shader_storage_read,
			.write = gpu::access_flag::shader_storage_write,
		}]],
		uniform_block [[= binding_form_info{
			.kind = gpu::binding_kind::read_only_storage_buffer,
			.read = gpu::access_flag::shader_storage_read,
		}]],
		byte_address_buffer [[= binding_form_info{
			.kind = gpu::binding_kind::read_only_storage_buffer,
			.read = gpu::access_flag::shader_storage_read,
		}]],
		rw_byte_address_buffer [[= binding_form_info{
			.kind = gpu::binding_kind::storage_buffer,
			.read = gpu::access_flag::shader_storage_read,
			.write = gpu::access_flag::shader_storage_write,
		}]],
		texture2d [[= binding_form_info{
			.kind = gpu::binding_kind::sampled_texture_2d,
			.read = gpu::access_flag::shader_sampled_read,
		}]],
		texture3d [[= binding_form_info{
			.kind = gpu::binding_kind::sampled_texture_3d,
			.read = gpu::access_flag::shader_sampled_read,
		}]],
		storage_image [[= binding_form_info{
			.kind = gpu::binding_kind::storage_texture_2d,
			.read = gpu::access_flag::shader_storage_read,
			.write = gpu::access_flag::shader_storage_write,
		}]],
		storage_image_3d [[= binding_form_info{
			.kind = gpu::binding_kind::storage_texture_3d,
			.read = gpu::access_flag::shader_storage_read,
			.write = gpu::access_flag::shader_storage_write,
		}]],
		sampler_state [[= binding_form_info{
			.kind = gpu::binding_kind::sampler,
		}]],
		tlas [[= binding_form_info{
			.kind = gpu::binding_kind::acceleration_structure,
		}]],
	};

	template <binding_form Form>
	struct slot_binding : gpu::bindless_slot {
		static constexpr binding_form form = Form;

		constexpr slot_binding() = default;

		constexpr slot_binding(
			gpu::bindless_slot slot
		);
	};

	template <typename T>
	struct ssbo_readonly : slot_binding<binding_form::ssbo_readonly> {
		using element = T;
		using slot_binding::slot_binding;
	};

	template <typename T>
	struct ssbo_readwrite : slot_binding<binding_form::ssbo_readwrite> {
		using element = T;
		using slot_binding::slot_binding;
	};

	template <typename T>
	struct uniform_block : slot_binding<binding_form::uniform_block> {
		using element = T;
		using slot_binding::slot_binding;
	};

	struct byte_address_buffer : slot_binding<binding_form::byte_address_buffer> {
		using slot_binding::slot_binding;
	};

	struct rw_byte_address_buffer : slot_binding<binding_form::rw_byte_address_buffer> {
		using slot_binding::slot_binding;
	};

	template <typename T>
	struct texture2d : slot_binding<binding_form::texture2d> {
		using element = T;
		using slot_binding::slot_binding;
	};

	template <typename T>
	struct texture3d : slot_binding<binding_form::texture3d> {
		using element = T;
		using slot_binding::slot_binding;
	};

	template <typename T>
	struct storage_image : slot_binding<binding_form::storage_image> {
		using element = T;
		using slot_binding::slot_binding;
	};

	template <typename T>
	struct storage_image_3d : slot_binding<binding_form::storage_image_3d> {
		using element = T;
		using slot_binding::slot_binding;
	};

	struct sampler_state : slot_binding<binding_form::sampler_state> {
		using slot_binding::slot_binding;
	};

	struct tlas : gpu::acceleration_structure_arg {
		static constexpr binding_form form = binding_form::tlas;

		constexpr tlas() = default;

		constexpr tlas(
			gpu::acceleration_structure_arg arg
		);
	};

	struct atomic_u32 {
		std::uint32_t value = 0;
	};

	template <typename T>
	struct slang_type;

	template <typename T>
	concept has_slang_type = requires {
		{ slang_type<T>::name } -> std::convertible_to<std::string_view>; 
	};

	template <typename T>
	concept is_shader_struct = has_annotation<shader_struct_tag>(^^T);

	template <typename T>
	concept is_shader_enum = std::is_enum_v<T> && has_annotation<shader_enum_tag>(^^T);

	template <typename T>
	concept is_shader_constant_block = has_annotation<shader_constant_block_tag>(^^T);

	template <typename T>
	concept is_shader_binding = requires {
		{ T::form } -> std::convertible_to<binding_form>;
	};

	template <is_shader_binding T>
	constexpr binding_form_info binding_info_v = annotation_from_enum(T::form, binding_form_info{});

	template <typename T>
	concept is_shader_user_type = is_shader_struct<T> || is_shader_enum<T> || is_shader_constant_block<T>;

	template <is_shader_struct T>
	auto emit_slang_struct() -> std::string;

	template <is_shader_enum E>
	auto emit_slang_enum() -> std::string;

	template <is_shader_constant_block T>
	auto emit_slang_constants() -> std::string;

	template <typename T>
	auto emit_slang_constants(
		const T& values
	) -> std::string;

	template <typename T>
	auto emit_slang_specialization_constants() -> std::string;

	template <typename T>
	auto build_spec_constant_entries() -> std::vector<gpu::specialization_entry>;

	template <is_shader_binding T>
	auto emit_slang_binding(
		std::string_view name
	) -> std::string;

	template <typename T>
	consteval auto push_constant_layout_is_portable() -> bool;

	template <typename Pack>
	auto emit_pack_types() -> std::string;

	template <typename Args>
	auto emit_bindings() -> std::string;

	auto emit_bindless_texture_table() -> std::string;

	struct struct_field_layout {
		std::string name;
		std::uint32_t offset = 0;
	};

	struct struct_layout {
		std::string name;
		std::uint32_t size = 0;
		std::vector<struct_field_layout> fields;
	};

	template <is_shader_binding T>
	consteval auto storage_format_of() -> gpu::image_format;

	template <typename T>
	auto wgsl_struct_layout(
		std::string_view name
	) -> struct_layout;

	template <typename Pack>
	auto emit_pack_struct_layouts(
		std::vector<struct_layout>& out
	) -> void;

	template <typename Args>
	auto emit_binding_struct_layouts(
		std::vector<struct_layout>& out
	) -> void;
}

constexpr auto gse::gpu::make_acceleration_structure_arg(const device_address address, const bindless_slot slot) -> acceleration_structure_arg {
	return {
		.address_lo = static_cast<std::uint32_t>(address),
		.address_hi = static_cast<std::uint32_t>(address >> 32),
		.slot = slot,
	};
}

template <gse::shaders::binding_form Form>
constexpr gse::shaders::slot_binding<Form>::slot_binding(const gpu::bindless_slot slot) : gpu::bindless_slot(slot) {}

constexpr gse::shaders::tlas::tlas(const gpu::acceleration_structure_arg arg) : gpu::acceleration_structure_arg(arg) {}

template <>
struct gse::shaders::slang_type<bool> {
	static constexpr std::string_view name = "bool";
};

template <>
struct gse::shaders::slang_type<float> {
	static constexpr std::string_view name = "float";
};

template <>
struct gse::shaders::slang_type<std::int32_t> {
	static constexpr std::string_view name = "int";
};

template <>
struct gse::shaders::slang_type<std::uint32_t> {
	static constexpr std::string_view name = "uint";
};

template <>
struct gse::shaders::slang_type<gse::shaders::atomic_u32> {
	static constexpr std::string_view name = "Atomic<uint>";
};

template <>
struct gse::shaders::slang_type<std::uint64_t> {
	static constexpr std::string_view name = "uint64_t";
};

template <>
struct gse::shaders::slang_type<std::int64_t> {
	static constexpr std::string_view name = "int64_t";
};

template <gse::internal::is_quantity Q>
struct gse::shaders::slang_type<Q> {
	static constexpr std::string_view name = slang_type<typename Q::value_type>::name;
};

template <gse::shaders::is_shader_user_type T>
struct gse::shaders::slang_type<T> {
	static constexpr std::string_view name = std::meta::identifier_of(^^T);
};

namespace gse::shaders {
	template <has_slang_type T, std::size_t N>
	struct vec_name_storage {
		static constexpr auto value = [] consteval {
			constexpr auto prefix = slang_type<T>::name;
			std::array<char, prefix.size() + 1> arr{};
			for (std::size_t i = 0; i < prefix.size(); ++i) {
				arr[i] = prefix[i];
			}
			arr[prefix.size()] = static_cast<char>('0' + N);
			return arr;
		}();
	};

	template <has_slang_type T, std::size_t Cols, std::size_t Rows>
	struct mat_name_storage {
		static constexpr auto value = [] consteval {
			constexpr auto prefix = slang_type<T>::name;
			std::array<char, prefix.size() + 3> arr{};
			for (std::size_t i = 0; i < prefix.size(); ++i) {
				arr[i] = prefix[i];
			}
			arr[prefix.size()] = static_cast<char>('0' + Cols);
			arr[prefix.size() + 1] = 'x';
			arr[prefix.size() + 2] = static_cast<char>('0' + Rows);
			return arr;
		}();
	};
}

template <gse::shaders::has_slang_type T, std::size_t N>
requires(N >= 2 && N <= 4)
struct gse::shaders::slang_type<gse::vec<T, N>> {
	static constexpr std::string_view name{ vec_name_storage<T, N>::value.data(), vec_name_storage<T, N>::value.size() };
};

template <gse::shaders::has_slang_type T, std::size_t Cols, std::size_t Rows>
requires(Cols >= 2 && Cols <= 4 && Rows >= 2 && Rows <= 4)
struct gse::shaders::slang_type<gse::mat<T, Cols, Rows>> {
	static constexpr std::string_view name{ mat_name_storage<T, Cols, Rows>::value.data(),
		mat_name_storage<T, Cols, Rows>::value.size() };
};

template <typename ColSpec, typename RowSpec, gse::shaders::has_slang_type T>
struct gse::shaders::slang_type<gse::mixed_mat<ColSpec, RowSpec, T>> : slang_type<mat<T, ColSpec::size, ColSpec::size>> {};

template <gse::shaders::has_slang_type T>
struct gse::shaders::slang_type<gse::quat_t<T>> {
	static constexpr std::string_view name{ vec_name_storage<T, 4>::value.data(), vec_name_storage<T, 4>::value.size() };
};

namespace gse::shaders {
	struct wgsl_type_layout {
		std::uint32_t size = 0;
		std::uint32_t align = 0;
		bool splittable = false;
	};

	constexpr std::size_t wgsl_max_members = 256;

	struct wgsl_struct_plan {
		std::uint32_t size = 0;
		std::uint32_t align = 4;
		bool ok = true;
		std::array<bool, wgsl_max_members> split{};
		std::array<std::uint32_t, wgsl_max_members> pad_before{};
		std::uint32_t tail_pad = 0;
	};

	template <typename T>
	struct wgsl_shape {
		static constexpr std::uint32_t components = 0;
		static constexpr std::uint32_t rows = 0;
		using scalar = T;
	};

	template <typename T, std::size_t N>
	struct wgsl_shape<vec<T, N>> {
		static constexpr std::uint32_t components = static_cast<std::uint32_t>(N);
		static constexpr std::uint32_t rows = 0;
		using scalar = T;
	};

	template <typename T>
	struct wgsl_shape<quat_t<T>> {
		static constexpr std::uint32_t components = 4;
		static constexpr std::uint32_t rows = 0;
		using scalar = T;
	};

	template <typename T, std::size_t Cols, std::size_t Rows>
	struct wgsl_shape<mat<T, Cols, Rows>> {
		static constexpr std::uint32_t components = static_cast<std::uint32_t>(Cols * Rows);
		static constexpr std::uint32_t rows = static_cast<std::uint32_t>(Rows);
		using scalar = T;
	};

	template <typename ColSpec, typename RowSpec, typename T>
	struct wgsl_shape<mixed_mat<ColSpec, RowSpec, T>> : wgsl_shape<mat<T, ColSpec::size, ColSpec::size>> {};

	constexpr auto wgsl_round_up(
		std::uint32_t value,
		std::uint32_t alignment
	) -> std::uint32_t;

	template <typename T>
	consteval auto wgsl_layout_of() -> wgsl_type_layout;

	template <typename T>
	consteval auto wgsl_plan_pass(
		bool force
	) -> wgsl_struct_plan;

	template <typename T>
	consteval auto wgsl_plan() -> wgsl_struct_plan;

	template <typename T>
	auto emit_wgsl_struct(
		std::string_view name
	) -> std::string;

	template <typename T>
	auto emit_wgsl_split_member(
		std::string& out,
		std::string_view name
	) -> void;

	auto slang_storage_format_name(
		gpu::image_format format
	) -> std::string_view;

	template <is_shader_binding T>
	auto emit_wgsl_binding(
		std::string_view name,
		std::uint32_t binding
	) -> std::string;
}

constexpr auto gse::shaders::wgsl_round_up(const std::uint32_t value, const std::uint32_t alignment) -> std::uint32_t {
	return (value + alignment - 1) / alignment * alignment;
}

template <typename T>
consteval auto gse::shaders::wgsl_layout_of() -> wgsl_type_layout {
	if constexpr (std::is_array_v<T>) {
		constexpr auto element = wgsl_layout_of<std::remove_extent_t<T>>();
		return {
			.size = static_cast<std::uint32_t>(std::extent_v<T>) * wgsl_round_up(element.size, element.align),
			.align = element.align,
		};
	}
	else if constexpr (is_shader_struct<T>) {
		constexpr auto plan = wgsl_plan<T>();
		return { .size = plan.size, .align = plan.align };
	}
	else if constexpr (wgsl_shape<T>::components == 0) {
		return { .size = static_cast<std::uint32_t>(sizeof(T)), .align = static_cast<std::uint32_t>(sizeof(T)) };
	}
	else {
		using shape = wgsl_shape<T>;
		constexpr auto scalar_size = static_cast<std::uint32_t>(sizeof(typename shape::scalar));
		if constexpr (shape::rows == 0) {
			constexpr auto align = shape::components == 2 ? 2 * scalar_size : 4 * scalar_size;
			return { .size = shape::components * scalar_size, .align = align, .splittable = true };
		}
		else {
			constexpr auto columns = shape::components / shape::rows;
			constexpr auto column_align = shape::rows == 2 ? 2 * scalar_size : 4 * scalar_size;
			return {
				.size = columns * wgsl_round_up(shape::rows * scalar_size, column_align),
				.align = column_align,
				.splittable = columns == shape::rows,
			};
		}
	}
}

template <typename T>
consteval auto gse::shaders::wgsl_plan_pass(const bool force) -> wgsl_struct_plan {
	wgsl_struct_plan plan;
	std::uint32_t cursor = 0;
	std::size_t index = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr auto layout = wgsl_layout_of<member_t>();
		const auto offset = static_cast<std::uint32_t>(std::meta::offset_of(m).bytes);
		const auto size = static_cast<std::uint32_t>(sizeof(member_t));
		const bool fits = layout.size == size && offset % layout.align == 0 && offset >= cursor;
		const bool split = layout.splittable && (!fits || (force && layout.align > 4));
		if ((!fits && !split) || offset < cursor || (offset - cursor) % 4 != 0 || index >= wgsl_max_members) {
			plan.ok = false;
		}
		else {
			plan.split[index] = split;
			plan.pad_before[index] = (offset - cursor) / 4;
		}
		if (split) {
			using scalar_t = typename wgsl_shape<member_t>::scalar;
			plan.align = std::max(plan.align, static_cast<std::uint32_t>(sizeof(scalar_t)));
			cursor = offset + wgsl_shape<member_t>::components * static_cast<std::uint32_t>(sizeof(scalar_t));
		}
		else {
			plan.align = std::max(plan.align, layout.align);
			cursor = offset + layout.size;
		}
		++index;
	}
	constexpr auto total = static_cast<std::uint32_t>(sizeof(T));
	if (cursor > total || (total - cursor) % 4 != 0) {
		plan.ok = false;
	}
	else {
		plan.tail_pad = (total - cursor) / 4;
	}
	plan.size = wgsl_round_up(total, plan.align);
	if (plan.size != total) {
		plan.ok = false;
	}
	return plan;
}

template <typename T>
consteval auto gse::shaders::wgsl_plan() -> wgsl_struct_plan {
	if (const auto relaxed = wgsl_plan_pass<T>(false); relaxed.ok) {
		return relaxed;
	}
	return wgsl_plan_pass<T>(true);
}

template <typename T>
auto gse::shaders::emit_wgsl_split_member(std::string& out, const std::string_view name) -> void {
	using shape = wgsl_shape<T>;
	constexpr auto scalar_name = slang_type<typename shape::scalar>::name;
	constexpr auto type_name = slang_type<T>::name;
	for (std::uint32_t k = 0; k < shape::components; ++k) {
		out += std::format("    public {} {}_{};\n", scalar_name, name, k);
	}
	std::string get;
	std::string set;
	if constexpr (shape::rows == 0) {
		constexpr std::string_view swizzle = "xyzw";
		for (std::uint32_t k = 0; k < shape::components; ++k) {
			get += std::format("{}{}_{}", k == 0 ? "" : ", ", name, k);
			set += std::format("{}_{} = newValue.{}; ", name, k, swizzle[k]);
		}
	}
	else {
		constexpr auto columns = shape::components / shape::rows;
		for (std::uint32_t r = 0; r < shape::rows; ++r) {
			for (std::uint32_t c = 0; c < columns; ++c) {
				get += std::format("{}{}_{}", r == 0 && c == 0 ? "" : ", ", name, c * shape::rows + r);
				set += std::format("{}_{} = newValue[{}][{}]; ", name, c * shape::rows + r, r, c);
			}
		}
	}
	out += std::format("    public property {0} {1} {{ get {{ return {0}({2}); }} set {{ {3}}} }}\n", type_name, name, get, set);
}

template <typename T>
auto gse::shaders::emit_wgsl_struct(const std::string_view name) -> std::string {
	constexpr auto plan = wgsl_plan<T>();
	assert(plan.ok, "{}: no WGSL layout matches the C++ layout of this shader struct (a member that is not a vec, quat or square matrix sits where WGSL alignment cannot place it)", name);
	std::string out = std::format("public struct {} {{\n", name);
	std::uint32_t pad = 0;
	std::size_t index = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		for (std::uint32_t p = 0; p < plan.pad_before[index]; ++p) {
			out += std::format("    public uint _wgsl_pad{};\n", pad++);
		}
		if (plan.split[index]) {
			if constexpr (wgsl_shape<member_t>::components != 0) {
				emit_wgsl_split_member<member_t>(out, std::meta::identifier_of(m));
			}
		}
		else if constexpr (std::is_array_v<member_t>) {
			out += std::format(
				"    public {} {}[{}];\n",
				slang_type<std::remove_extent_t<member_t>>::name,
				std::meta::identifier_of(m),
				std::extent_v<member_t>
			);
		}
		else {
			out += std::format("    public {} {};\n", slang_type<member_t>::name, std::meta::identifier_of(m));
		}
		++index;
	}
	for (std::uint32_t p = 0; p < plan.tail_pad; ++p) {
		out += std::format("    public uint _wgsl_pad{};\n", pad++);
	}
	out += "};\n";
	return out;
}

template <typename T>
auto gse::shaders::wgsl_struct_layout(const std::string_view name) -> struct_layout {
	constexpr auto plan = wgsl_plan<T>();
	struct_layout layout{ .name = std::string(name), .size = static_cast<std::uint32_t>(sizeof(T)) };
	std::uint32_t pad = 0;
	std::uint32_t cursor = 0;
	std::size_t index = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		const auto offset = static_cast<std::uint32_t>(std::meta::offset_of(m).bytes);
		for (std::uint32_t p = 0; p < plan.pad_before[index]; ++p) {
			layout.fields.push_back({ .name = std::format("_wgsl_pad{}", pad++), .offset = cursor + p * 4 });
		}
		if (plan.split[index]) {
			if constexpr (wgsl_shape<member_t>::components != 0) {
				constexpr auto scalar_size = static_cast<std::uint32_t>(sizeof(typename wgsl_shape<member_t>::scalar));
				for (std::uint32_t k = 0; k < wgsl_shape<member_t>::components; ++k) {
					layout.fields.push_back({ .name = std::format("{}_{}", std::meta::identifier_of(m), k), .offset = offset + k * scalar_size });
				}
				cursor = offset + wgsl_shape<member_t>::components * scalar_size;
			}
		}
		else {
			layout.fields.push_back({ .name = std::string(std::meta::identifier_of(m)), .offset = offset });
			cursor = offset + static_cast<std::uint32_t>(sizeof(member_t));
		}
		++index;
	}
	for (std::uint32_t p = 0; p < plan.tail_pad; ++p) {
		layout.fields.push_back({ .name = std::format("_wgsl_pad{}", pad++), .offset = cursor + p * 4 });
	}
	return layout;
}

template <gse::shaders::is_shader_struct T>
auto gse::shaders::emit_slang_struct() -> std::string {
	if (gpu::active_backend == gpu::backend_kind::webgpu) {
		return emit_wgsl_struct<T>(std::meta::identifier_of(^^T));
	}
	std::string out = std::format("public struct {} {{\n", std::meta::identifier_of(^^T));
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		if constexpr (std::is_array_v<member_t>) {
			using element_t = std::remove_extent_t<member_t>;
			constexpr auto extent = std::extent_v<member_t>;
			out += std::format(
				"    public {} {}[{}];\n",
				slang_type<element_t>::name,
				std::meta::identifier_of(m),
				extent
			);
		}
		else {
			out += std::format("    public {} {};\n", slang_type<member_t>::name, std::meta::identifier_of(m));
		}
	}
	out += "};\n";
	return out;
}

template <gse::shaders::is_shader_enum E>
auto gse::shaders::emit_slang_enum() -> std::string {
	std::string out = std::format("public enum class {} {{\n", std::meta::identifier_of(^^E));
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
		out += std::format(
			"    {} = {},\n", std::meta::identifier_of(e),
			static_cast<std::underlying_type_t<E>>([:e:])
		);
	}
	out += "};\n";
	return out;
}

namespace gse::shaders {
	template <typename T>
	auto format_slang_literal(
		const T& v
	) -> std::string;
}

template <typename T>
auto gse::shaders::format_slang_literal(const T& v) -> std::string {
	if constexpr (internal::is_quantity<T>) {
		return format_slang_literal(v.template as<typename T::default_unit>());
	}
	else if constexpr (is_vec<T>) {
		std::string out = std::format("{}(", slang_type<T>::name);
		for (std::size_t i = 0; i < T::extent; ++i) {
			if (i > 0) {
				out += ", ";
			}
			out += format_slang_literal(v[i]);
		}
		out += ")";
		return out;
	}
	else {
		return std::format("{}", v);
	}
}

template <gse::shaders::is_shader_constant_block T>
auto gse::shaders::emit_slang_constants() -> std::string {
	return emit_slang_constants(T{});
}

template <typename T>
auto gse::shaders::emit_slang_constants(const T& values) -> std::string {
	std::string out;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		out += std::format(
			"public static const {} {} = {};\n",
			slang_type<member_t>::name,
			std::meta::identifier_of(m),
			format_slang_literal(values.[:m:])
		);
	}
	return out;
}

template <typename T>
auto gse::shaders::emit_slang_specialization_constants() -> std::string {
	std::string out;
	std::uint32_t idx = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr member_t v = T{}.[:m:];
		out += std::format(
			"[[vk::constant_id({})]] public const {} {} = {};\n",
			idx,
			slang_type<member_t>::name,
			std::meta::identifier_of(m),
			format_slang_literal(v)
		);
		++idx;
	}
	return out;
}

template <typename T>
auto gse::shaders::build_spec_constant_entries() -> std::vector<gpu::specialization_entry> {
	std::vector<gpu::specialization_entry> entries;
	std::uint32_t idx = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		entries.push_back({
			.constant_id = idx,
			.offset = static_cast<std::uint32_t>(std::meta::offset_of(m).bytes),
			.size = static_cast<std::uint32_t>(sizeof(member_t)),
		});
		++idx;
	}
	return entries;
}

template <gse::shaders::is_shader_binding T>
auto gse::shaders::emit_slang_binding(const std::string_view name) -> std::string {
	const auto read_index = gpu::active_backend == gpu::backend_kind::dx12
		? std::format("({}_idx + {})", name, gpu::dx12_buffer_read_view_offset)
		: std::format("{}_idx", name);
	if constexpr (T::form == binding_form::texture2d) {
		using element_t = typename T::element;
		return std::format("public uniform uint {1}_idx;\npublic property Texture2D<{0}> {1} {{ get {{ return (Texture2D<{0}>.Handle){1}_idx; }} }}\n", slang_type<element_t>::name, name);
	}
	else if constexpr (T::form == binding_form::texture3d) {
		using element_t = typename T::element;
		return std::format("public uniform uint {1}_idx;\npublic property Texture3D<{0}> {1} {{ get {{ return (Texture3D<{0}>.Handle){1}_idx; }} }}\n", slang_type<element_t>::name, name);
	}
	else if constexpr (T::form == binding_form::sampler_state) {
		return std::format("public uniform uint {0}_idx;\npublic property SamplerState {0} {{ get {{ return (SamplerState.Handle){0}_idx; }} }}\n", name);
	}
	else if constexpr (T::form == binding_form::tlas) {
		if (gpu::active_backend == gpu::backend_kind::vulkan) {
			return std::format(
				"public uniform uint {0}_address_lo;\n"
				"public uniform uint {0}_address_hi;\n"
				"public uniform uint {0}_idx;\n"
				"public property RaytracingAccelerationStructure {0} {{ get {{ return (RaytracingAccelerationStructure)(((uint64_t){0}_address_hi << 32) | (uint64_t){0}_address_lo); }} }}\n",
				name
			);
		}
		return std::format(
			"public uniform uint {0}_address_lo;\n"
			"public uniform uint {0}_address_hi;\n"
			"public uniform uint {0}_idx;\n"
			"public property RaytracingAccelerationStructure {0} {{ get {{ return (RaytracingAccelerationStructure.Handle){0}_idx; }} }}\n",
			name
		);
	}
	else if constexpr (T::form == binding_form::byte_address_buffer) {
		return std::format(
			"public uniform uint {0}_idx;\npublic property ByteAddressBuffer {0} {{ get {{ return (ByteAddressBuffer.Handle){1}; }} }}\n",
			name,
			read_index
		);
	}
	else if constexpr (T::form == binding_form::rw_byte_address_buffer) {
		return std::format(
			"public uniform uint {0}_idx;\npublic property RWByteAddressBuffer {0} {{ get {{ return (RWByteAddressBuffer.Handle){0}_idx; }} }}\n",
			name
		);
	}
	else if constexpr (T::form == binding_form::storage_image) {
		using element_t = typename T::element;
		return std::format(
			"public uniform uint {1}_idx;\npublic property RWTexture2D<{0}> {1} {{ get {{ return (RWTexture2D<{0}>.Handle){1}_idx; }} }}\n",
			slang_type<element_t>::name,
			name
		);
	}
	else if constexpr (T::form == binding_form::storage_image_3d) {
		using element_t = typename T::element;
		return std::format(
			"public uniform uint {1}_idx;\npublic property RWTexture3D<{0}> {1} {{ get {{ return (RWTexture3D<{0}>.Handle){1}_idx; }} }}\n",
			slang_type<element_t>::name,
			name
		);
	}
	else if constexpr (T::form == binding_form::ssbo_readonly) {
		using element_t = typename T::element;
		return std::format(
			"public uniform uint {0}_idx;\npublic property StructuredBuffer<{1}> {0} {{ get {{ return (StructuredBuffer<{1}>.Handle){2}; }} }}\n",
			name,
			slang_type<element_t>::name,
			read_index
		);
	}
	else if constexpr (T::form == binding_form::ssbo_readwrite) {
		using element_t = typename T::element;
		return std::format(
			"public uniform uint {0}_idx;\npublic property RWStructuredBuffer<{1}> {0} {{ get {{ return (RWStructuredBuffer<{1}>.Handle){0}_idx; }} }}\n",
			name,
			slang_type<element_t>::name
		);
	}
	else {
		using element_t = typename T::element;
		std::string out = std::format("public struct {}_ubo_data {{\n", name);
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^element_t, std::meta::access_context::unchecked()))) {
			using member_t = [:std::meta::type_of(m):];
			if constexpr (std::is_array_v<member_t>) {
				using elem_t = std::remove_extent_t<member_t>;
				constexpr auto extent = std::extent_v<member_t>;
				out += std::format(
					"    public {} {}[{}];\n",
					slang_type<elem_t>::name,
					std::meta::identifier_of(m),
					extent
				);
			}
			else {
				out += std::format("    public {} {};\n", slang_type<member_t>::name, std::meta::identifier_of(m));
			}
		}
		out += "};\n";
		out += std::format("public uniform uint {0}_idx;\npublic property {0}_ubo_data {0} {{ get {{ return ((StructuredBuffer<{0}_ubo_data>.Handle){1})[0]; }} }}\n", name, read_index);
		return out;
	}
}

template <gse::shaders::is_shader_binding T>
consteval auto gse::shaders::storage_format_of() -> gpu::image_format {
	if constexpr (std::is_same_v<typename T::element, vec4f>) {
		return gpu::image_format::r16g16b16a16_sfloat;
	}
	else {
		return gpu::image_format::undefined;
	}
}

auto gse::shaders::slang_storage_format_name(const gpu::image_format format) -> std::string_view {
	switch (format) {
		case gpu::image_format::r16g16b16a16_sfloat: return "rgba16f";
		case gpu::image_format::r16g16_sfloat: return "rg16f";
		case gpu::image_format::r8g8b8a8_unorm: return "rgba8";
		case gpu::image_format::r8g8_unorm: return "rg8";
		case gpu::image_format::r8_unorm: return "r8";
		case gpu::image_format::b10g11r11_ufloat: return "r11f_g11f_b10f";
		case gpu::image_format::b8g8r8a8_unorm: return "bgra8";
		default: return {};
	}
}

template <gse::shaders::is_shader_binding T>
auto gse::shaders::emit_wgsl_binding(const std::string_view name, const std::uint32_t binding) -> std::string {
	if constexpr (T::form == binding_form::texture2d) {
		return std::format("[[vk::binding({0}, 0)]] public Texture2D<{1}> {2};\n", binding, slang_type<typename T::element>::name, name);
	}
	else if constexpr (T::form == binding_form::texture3d) {
		return std::format("[[vk::binding({0}, 0)]] public Texture3D<{1}> {2};\n", binding, slang_type<typename T::element>::name, name);
	}
	else if constexpr (T::form == binding_form::sampler_state) {
		return std::format("[[vk::binding({0}, 0)]] public SamplerState {1};\n", binding, name);
	}
	else if constexpr (T::form == binding_form::byte_address_buffer) {
		return std::format("[[vk::binding({0}, 0)]] public ByteAddressBuffer {1};\n", binding, name);
	}
	else if constexpr (T::form == binding_form::rw_byte_address_buffer) {
		return std::format("[[vk::binding({0}, 0)]] public RWByteAddressBuffer {1};\n", binding, name);
	}
	else if constexpr (T::form == binding_form::storage_image || T::form == binding_form::storage_image_3d) {
		constexpr bool volume = T::form == binding_form::storage_image_3d;
		constexpr auto format = storage_format_of<T>();
		const auto format_name = slang_storage_format_name(format);
		assert(!format_name.empty(), "{}: storage image element type has no WGSL texel format", name);
		return std::format(
			"[[vk::binding({0}, 0)]] [format(\"{1}\")] public {2}<{3}> {4}_storage;\n"
			"public struct {4}_view {{ public __subscript({5} i) -> {3} {{ set {{ {4}_storage.Store(i, newValue); }} }} public void GetDimensions({6}) {{ {4}_storage.GetDimensions({7}); }} }}\n"
			"static {4}_view {4};\n",
			binding,
			format_name,
			volume ? "WTexture3D" : "WTexture2D",
			slang_type<typename T::element>::name,
			name,
			volume ? "uint3" : "uint2",
			volume ? "out uint w, out uint h, out uint d" : "out uint w, out uint h",
			volume ? "w, h, d" : "w, h"
		);
	}
	else if constexpr (T::form == binding_form::ssbo_readonly) {
		return std::format("[[vk::binding({0}, 0)]] public StructuredBuffer<{1}> {2};\n", binding, slang_type<typename T::element>::name, name);
	}
	else if constexpr (T::form == binding_form::ssbo_readwrite) {
		return std::format("[[vk::binding({0}, 0)]] public RWStructuredBuffer<{1}> {2};\n", binding, slang_type<typename T::element>::name, name);
	}
	else if constexpr (T::form == binding_form::uniform_block) {
		const auto data_name = std::format("{}_ubo_data", name);
		return emit_wgsl_struct<typename T::element>(data_name) + std::format(
			"[[vk::binding({0}, 0)]] public StructuredBuffer<{1}> {2}_buffer;\n"
			"public property {1} {2} {{ get {{ return {2}_buffer[0]; }} }}\n",
			binding,
			data_name,
			name
		);
	}
	else {
		assert(false, "{}: ray-tracing acceleration structures have no WGSL form", name);
		return {};
	}
}

template <typename T>
consteval auto gse::shaders::push_constant_layout_is_portable() -> bool {
	std::size_t scalar_off = 0;
	std::size_t cbuffer_off = 0;
	bool portable = true;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr auto size = sizeof(member_t);
		constexpr auto align = alignof(member_t);
		scalar_off = (scalar_off + align - 1) & ~(align - 1);
		cbuffer_off = (cbuffer_off + align - 1) & ~(align - 1);
		if constexpr (is_mat<member_t> || sizeof(member_t) > 16) {
			cbuffer_off = (cbuffer_off + 15) & ~std::size_t{ 15 };
		}
		else if constexpr (is_vec<member_t>) {
			if constexpr (member_t::extent == 4) {
				cbuffer_off = (cbuffer_off + 15) & ~std::size_t{ 15 };
			}
			else if (cbuffer_off / 16 != (cbuffer_off + size - 1) / 16) {
				cbuffer_off = (cbuffer_off + 15) & ~std::size_t{ 15 };
			}
		}
		if (scalar_off != cbuffer_off) {
			portable = false;
		}
		scalar_off += size;
		cbuffer_off += size;
	}
	return portable;
}

namespace gse::shaders {
	template <typename T>
	auto emit_one_user_type(std::string& out) -> void {
		if constexpr (is_shader_enum<T>) {
			out.append(emit_slang_enum<T>());
			out.push_back('\n');
		}
		else if constexpr (is_shader_struct<T>) {
			out.append(emit_slang_struct<T>());
			out.push_back('\n');
		}
		else if constexpr (is_shader_constant_block<T>) {
			out.append(emit_slang_constants<T>());
			out.push_back('\n');
		}
	}

	template <typename T>
	auto collect_one_struct_layout(std::vector<struct_layout>& out) -> void {
		if constexpr (is_shader_struct<T>) {
			out.push_back(wgsl_struct_layout<T>(std::meta::identifier_of(^^T)));
		}
	}
}

template <typename Pack>
auto gse::shaders::emit_pack_struct_layouts(std::vector<struct_layout>& out) -> void {
	[&]<typename... Ts>(type_pack<Ts...>) {
		(collect_one_struct_layout<Ts>(out), ...);
	}(Pack{});
}

template <typename Pack>
auto gse::shaders::emit_pack_types() -> std::string {
	std::string out;
	[&]<typename... Ts>(type_pack<Ts...>) {
		(emit_one_user_type<Ts>(out), ...);
	}(Pack{});
	return out;
}

template <typename Args>
auto gse::shaders::emit_bindings() -> std::string {
	std::string out;
	std::uint32_t binding = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Args, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr auto name = std::meta::identifier_of(m);
		if (gpu::active_backend == gpu::backend_kind::webgpu) {
			out.append(emit_wgsl_binding<member_t>(name, binding++));
		}
		else {
			out.append(emit_slang_binding<member_t>(name));
		}
	}
	return out;
}

auto gse::shaders::emit_bindless_texture_table() -> std::string {
	assert(gpu::active_backend != gpu::backend_kind::webgpu, "the bindless texture table has no WGSL form; systems that sample it are unregistered on webgpu");
	return "public struct textures_table { public __subscript(uint i) -> Texture2D<float4> { get { return (Texture2D<float4>.Handle)i; } } }\npublic textures_table textures;\n";
}

template <typename Args>
auto gse::shaders::emit_binding_struct_layouts(std::vector<struct_layout>& out) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Args, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		if constexpr (member_t::form == binding_form::uniform_block) {
			out.push_back(wgsl_struct_layout<typename member_t::element>(std::format("{}_ubo_data", std::meta::identifier_of(m))));
		}
	}
}

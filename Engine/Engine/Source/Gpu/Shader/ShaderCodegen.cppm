export module gse.gpu:shader_codegen;

import std;

import gse.math;
import gse.meta;
import gse.containers;

import :aliases;

export namespace gse::shaders {
	struct shader_struct_tag {};
	struct shader_enum_tag {};
	struct shader_constant_block_tag {};
	struct sampler2d_array_tag {};
	struct sampler2d_tag {};
	struct sampler3d_tag {};
	struct ssbo_readonly_tag {};
	struct ssbo_readwrite_tag {};
	struct tlas_tag {};
	struct byte_address_buffer_tag {};
	struct rw_byte_address_buffer_tag {};
	struct storage_image_tag {};
	struct storage_image_3d_tag {};

	constexpr shader_struct_tag shader_struct{};
	constexpr shader_enum_tag shader_enum{};
	constexpr shader_constant_block_tag shader_constant_block{};
	constexpr sampler2d_array_tag sampler2d_array{};
	constexpr sampler2d_tag sampler2d{};
	constexpr sampler3d_tag sampler3d{};
	constexpr ssbo_readonly_tag ssbo_readonly{};
	constexpr ssbo_readwrite_tag ssbo_readwrite{};
	constexpr tlas_tag tlas{};
	constexpr byte_address_buffer_tag byte_address_buffer{};
	constexpr rw_byte_address_buffer_tag rw_byte_address_buffer{};
	constexpr storage_image_tag storage_image{};
	constexpr storage_image_3d_tag storage_image_3d{};

	constexpr std::uint32_t bindless_texture_capacity = 2048;

	template <std::uint32_t Set, std::uint32_t Slot>
	struct binding {
		static constexpr std::uint32_t set = Set;
		static constexpr std::uint32_t slot = Slot;
	};

	consteval auto find_binding_type(
		std::meta::info m
	) -> std::meta::info;

	template <typename T>
	struct slang_type;

	template <typename T>
	concept has_slang_type = requires {
		{ slang_type<T>::name } -> std::convertible_to<std::string_view>; };

	template <typename T>
	concept is_shader_struct = has_annotation<shader_struct_tag>(^^T);

	template <typename T>
	concept is_shader_enum = std::is_enum_v<T> && has_annotation<shader_enum_tag>(^^T);

	template <typename T>
	concept is_shader_constant_block = has_annotation<shader_constant_block_tag>(^^T);

	template <typename T>
	concept is_shader_binding = find_binding_type(^^T) != std::meta::info{};

	template <typename T>
	concept is_shader_user_type = is_shader_struct<T> || is_shader_enum<T> || is_shader_constant_block<T>;

	template <is_shader_struct T>
	auto emit_slang_struct() -> std::string;

	template <is_shader_enum E>
	auto emit_slang_enum() -> std::string;

	template <is_shader_constant_block T>
	auto emit_slang_constants() -> std::string;

	template <typename T>
	auto emit_slang_specialization_constants() -> std::string;

	struct spec_constant_entry {
		std::uint32_t constant_id = 0;
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	template <typename T>
	auto build_spec_constant_entries() -> std::vector<spec_constant_entry>;

	template <is_shader_binding T>
	auto emit_slang_binding() -> std::string;

	template <typename T>
	consteval auto slang_scalar_align() -> std::size_t;

	template <typename T>
	consteval auto slang_scalar_size() -> std::size_t;

	struct family_binding {
		gpu::descriptor_binding_desc desc;
	};

	struct family_set {
		std::uint32_t set_index = 0;
		std::vector<family_binding> bindings;
	};

	template <is_shader_binding T>
	consteval auto descriptor_type_of() -> gpu::descriptor_type;

	template <is_shader_binding T>
	consteval auto descriptor_count_of() -> std::uint32_t;

	template <is_shader_binding T>
	consteval auto descriptor_access_of() -> gpu::descriptor_access;

	template <typename Pack>
	auto build_family_sets(
		Pack pack
	) -> std::vector<family_set>;

	template <typename... Packs>
	auto build_combined_family_sets() -> std::vector<family_set>;

	template <typename Pack>
	auto emit_pack_types() -> std::string;

	template <typename Pack>
	auto emit_pack_bindings() -> std::string;
}

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
struct gse::shaders::slang_type<gse::mixed_mat<ColSpec, RowSpec, T>> : gse::shaders::slang_type<gse::mat<T, ColSpec::size, ColSpec::size>> {};

template <gse::shaders::has_slang_type T>
struct gse::shaders::slang_type<gse::quat_t<T>> {
	static constexpr std::string_view name{ vec_name_storage<T, 4>::value.data(), vec_name_storage<T, 4>::value.size() };
};

template <gse::shaders::is_shader_struct T>
auto gse::shaders::emit_slang_struct() -> std::string {
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
		out += std::format("    {} = {},\n", std::meta::identifier_of(e),
						   static_cast<std::underlying_type_t<E>>([:e:]));
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
	if constexpr (gse::internal::is_quantity<T>) {
		return format_slang_literal(static_cast<typename T::value_type>(v));
	}
	else if constexpr (gse::is_vec<T>) {
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
	std::string out;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr member_t v = T{}.[:m:];
		out += std::format(
			"public static const {} {} = {};\n",
			slang_type<member_t>::name,
			std::meta::identifier_of(m),
			format_slang_literal(v)
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
auto gse::shaders::build_spec_constant_entries() -> std::vector<spec_constant_entry> {
	std::vector<spec_constant_entry> entries;
	std::uint32_t idx = 0;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		entries.push_back({
			.constant_id = idx,
			.offset = std::meta::offset_of_v<m>,
			.size = static_cast<std::uint32_t>(sizeof(member_t)),
		});
		++idx;
	}
	return entries;
}

consteval auto gse::shaders::find_binding_type(const std::meta::info m) -> std::meta::info {
	for (auto ann : std::meta::annotations_of(m)) {
		const auto t = std::meta::dealias(std::meta::type_of(ann));
		if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == ^^binding) {
			return t;
		}
	}
	return std::meta::info{};
}

template <gse::shaders::is_shader_binding T>
auto gse::shaders::emit_slang_binding() -> std::string {
	using binding_t = [:find_binding_type(^^T):];
	constexpr auto name = std::meta::identifier_of(^^T);
	if constexpr (has_annotation<sampler2d_array_tag>(^^T)) {
		return std::format("public [[vk::binding({}, {})]] Sampler2D {}[];\n", binding_t::slot, binding_t::set, name);
	}
	else if constexpr (has_annotation<sampler2d_tag>(^^T)) {
		return std::format("public [[vk::binding({}, {})]] Sampler2D {};\n", binding_t::slot, binding_t::set, name);
	}
	else if constexpr (has_annotation<sampler3d_tag>(^^T)) {
		return std::format("public [[vk::binding({}, {})]] Sampler3D {};\n", binding_t::slot, binding_t::set, name);
	}
	else if constexpr (has_annotation<tlas_tag>(^^T)) {
		return std::format(
			"public [[vk::binding({}, {})]] RaytracingAccelerationStructure {};\n",
			binding_t::slot,
			binding_t::set,
			name
		);
	}
	else if constexpr (has_annotation<byte_address_buffer_tag>(^^T)) {
		return std::format(
			"public [[vk::binding({}, {})]] ByteAddressBuffer {};\n",
			binding_t::slot,
			binding_t::set,
			name
		);
	}
	else if constexpr (has_annotation<rw_byte_address_buffer_tag>(^^T)) {
		return std::format(
			"public [[vk::binding({}, {})]] RWByteAddressBuffer {};\n",
			binding_t::slot,
			binding_t::set,
			name
		);
	}
	else if constexpr (has_annotation<storage_image_tag>(^^T)) {
		using element_t = typename T::element;
		return std::format(
			"public [[vk::binding({}, {})]] RWTexture2D<{}> {};\n",
			binding_t::slot,
			binding_t::set,
			slang_type<element_t>::name,
			name
		);
	}
	else if constexpr (has_annotation<storage_image_3d_tag>(^^T)) {
		using element_t = typename T::element;
		return std::format(
			"public [[vk::binding({}, {})]] RWTexture3D<{}> {};\n",
			binding_t::slot,
			binding_t::set,
			slang_type<element_t>::name,
			name
		);
	}
	else if constexpr (has_annotation<ssbo_readonly_tag>(^^T)) {
		using element_t = typename T::element;
		return std::format(
			"public [[vk::binding({}, {})]] StructuredBuffer<{}> {};\n",
			binding_t::slot,
			binding_t::set,
			slang_type<element_t>::name,
			name
		);
	}
	else if constexpr (has_annotation<ssbo_readwrite_tag>(^^T)) {
		using element_t = typename T::element;
		return std::format(
			"public [[vk::binding({}, {})]] RWStructuredBuffer<{}> {};\n",
			binding_t::slot,
			binding_t::set,
			slang_type<element_t>::name,
			name
		);
	}
	else if constexpr (requires { typename T::element; }) {
		using element_t = typename T::element;
		std::string out =
			std::format(
				"[[vk::binding({}, {})]]\npublic cbuffer {} {{\n",
				binding_t::slot,
				binding_t::set,
				name
			);
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
		return out;
	}
	else {
		std::string out =
			std::format(
				"[[vk::binding({}, {})]]\npublic cbuffer {} {{\n",
				binding_t::slot,
				binding_t::set,
				name
			);
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
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
		return out;
	}
}

template <typename T>
consteval auto gse::shaders::slang_scalar_align() -> std::size_t {
	if constexpr (std::is_same_v<T, float> || std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::uint32_t>) {
		return 4;
	}
	else if constexpr (gse::internal::is_quantity<T>) {
		return slang_scalar_align<typename T::value_type>();
	}
	else if constexpr (gse::is_vec<T>) {
		return slang_scalar_align<typename T::value_type>();
	}
	else if constexpr (gse::is_mat<T>) {
		return slang_scalar_align<typename T::element_type>();
	}
	else if constexpr (std::is_enum_v<T>) {
		return slang_scalar_align<std::underlying_type_t<T>>();
	}
	else if constexpr (is_shader_struct<T>) {
		std::size_t a = 1;
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			using member_t = [:std::meta::type_of(m):];
			a = std::max(a, slang_scalar_align<member_t>());
		}
		return a;
	}
	else {
		static_assert(sizeof(T) == 0, "no slang scalar alignment for T");
	}
}

template <typename T>
consteval auto gse::shaders::slang_scalar_size() -> std::size_t {
	if constexpr (std::is_same_v<T, float> || std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::uint32_t>) {
		return 4;
	}
	else if constexpr (gse::internal::is_quantity<T>) {
		return slang_scalar_size<typename T::value_type>();
	}
	else if constexpr (gse::is_vec<T>) {
		return slang_scalar_size<typename T::value_type>() * T::extent;
	}
	else if constexpr (gse::is_mat<T>) {
		return slang_scalar_size<typename T::element_type>() * T::extent_cols * T::extent_rows;
	}
	else if constexpr (std::is_enum_v<T>) {
		return slang_scalar_size<std::underlying_type_t<T>>();
	}
	else if constexpr (is_shader_struct<T>) {
		std::size_t off = 0;
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			using member_t = [:std::meta::type_of(m):];
			constexpr auto a = slang_scalar_align<member_t>();
			off = (off + a - 1) & ~(a - 1);
			off += slang_scalar_size<member_t>();
		}
		return off;
	}
	else {
		static_assert(sizeof(T) == 0, "no slang scalar size for T");
	}
}

template <gse::shaders::is_shader_binding T>
consteval auto gse::shaders::descriptor_type_of() -> gpu::descriptor_type {
	if constexpr (has_annotation<sampler2d_array_tag>(^^T)) {
		return gpu::descriptor_type::combined_image_sampler;
	}
	else if constexpr (has_annotation<sampler2d_tag>(^^T)) {
		return gpu::descriptor_type::combined_image_sampler;
	}
	else if constexpr (has_annotation<sampler3d_tag>(^^T)) {
		return gpu::descriptor_type::combined_image_sampler;
	}
	else if constexpr (has_annotation<tlas_tag>(^^T)) {
		return gpu::descriptor_type::acceleration_structure;
	}
	else if constexpr (has_annotation<ssbo_readonly_tag>(^^T) || has_annotation<ssbo_readwrite_tag>(^^T) || has_annotation<byte_address_buffer_tag>(^^T) || has_annotation<rw_byte_address_buffer_tag>(^^T)) {
		return gpu::descriptor_type::storage_buffer;
	}
	else if constexpr (has_annotation<storage_image_tag>(^^T) || has_annotation<storage_image_3d_tag>(^^T)) {
		return gpu::descriptor_type::storage_image;
	}
	else {
		return gpu::descriptor_type::uniform_buffer;
	}
}

template <gse::shaders::is_shader_binding T>
consteval auto gse::shaders::descriptor_count_of() -> std::uint32_t {
	if constexpr (has_annotation<sampler2d_array_tag>(^^T)) {
		return bindless_texture_capacity;
	}
	else {
		return 1;
	}
}

template <gse::shaders::is_shader_binding T>
consteval auto gse::shaders::descriptor_access_of() -> gpu::descriptor_access {
	if constexpr (has_annotation<ssbo_readwrite_tag>(^^T) || has_annotation<rw_byte_address_buffer_tag>(^^T) || has_annotation<storage_image_tag>(^^T) || has_annotation<storage_image_3d_tag>(^^T)) {
		return gpu::descriptor_access::read_write;
	}
	else {
		return gpu::descriptor_access::read;
	}
}

namespace gse::shaders {
	template <is_shader_binding T>
	auto append_family_binding(std::vector<family_set>& sets) -> void {
		using binding_t = [:find_binding_type(^^T):];
		constexpr auto set_idx = binding_t::set;
		constexpr auto slot_idx = binding_t::slot;
		constexpr auto desc_type = descriptor_type_of<T>();
		constexpr auto count = descriptor_count_of<T>();
		constexpr auto access = descriptor_access_of<T>();
		constexpr gpu::stage_flags all_stages = gpu::stage_flag::vertex | gpu::stage_flag::fragment |
			gpu::stage_flag::compute | gpu::stage_flag::task | gpu::stage_flag::mesh;

		auto it = std::ranges::find_if(
			sets,
			[&](const family_set& s) {
				return s.set_index == set_idx;
			}
		);
		if (it == sets.end()) {
			sets.push_back(family_set{
				.set_index = set_idx
			});
			it = sets.end() - 1;
		}
		it->bindings.push_back(family_binding{
			.desc = {
				.binding = slot_idx,
				.type = desc_type,
				.count = count,
				.stages = all_stages,
				.access = access,
			},
		});
	}
}

template <typename Pack>
auto gse::shaders::build_family_sets(Pack) -> std::vector<family_set> {
	std::vector<family_set> sets;
	[&]<typename... Ts>(type_pack<Ts...>) {
		(append_family_binding<Ts>(sets), ...);
	}(Pack{});
	std::ranges::sort(
		sets,
		{},
		[](const family_set& s) {
			return s.set_index;
		}
	);
	return sets;
}

template <typename... Packs>
auto gse::shaders::build_combined_family_sets() -> std::vector<family_set> {
	std::vector<family_set> result;
	auto merge_into = [&](std::vector<family_set> src) {
		for (auto& s : src) {
			auto it = std::ranges::find_if(
				result,
				[&](const family_set& d) {
					return d.set_index == s.set_index;
				}
			);
			if (it == result.end()) {
				result.push_back(std::move(s));
			}
			else {
				for (auto& b : s.bindings) {
					it->bindings.push_back(std::move(b));
				}
			}
		}
	};
	(merge_into(build_family_sets(Packs{})), ...);
	std::ranges::sort(
		result,
		{},
		[](const family_set& s) {
			return s.set_index;
		}
	);
	return result;
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
	auto emit_one_binding(std::string& out) -> void {
		if constexpr (is_shader_binding<T>) {
			out.append(emit_slang_binding<T>());
		}
	}
}

template <typename Pack>
auto gse::shaders::emit_pack_types() -> std::string {
	std::string out;
	[&]<typename... Ts>(type_pack<Ts...>) {
		(emit_one_user_type<Ts>(out), ...);
	}(Pack{});
	return out;
}

template <typename Pack>
auto gse::shaders::emit_pack_bindings() -> std::string {
	std::string out;
	[&]<typename... Ts>(type_pack<Ts...>) {
		(emit_one_binding<Ts>(out), ...);
	}(Pack{});
	return out;
}

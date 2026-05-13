export module gse.gpu:shader_codegen;

import std;
import gse.std_meta;
import gse.math;
import gse.meta;

export namespace gse::shaders {
	struct shader_struct_tag {};
	struct shader_enum_tag {};
	struct sampler2d_tag {};
	struct ssbo_readonly_tag {};
	struct ssbo_readwrite_tag {};
	struct tlas_tag {};
	struct byte_address_buffer_tag {};

	constexpr shader_struct_tag shader_struct{};
	constexpr shader_enum_tag shader_enum{};
	constexpr sampler2d_tag sampler2d{};
	constexpr ssbo_readonly_tag ssbo_readonly{};
	constexpr ssbo_readwrite_tag ssbo_readwrite{};
	constexpr tlas_tag tlas{};
	constexpr byte_address_buffer_tag byte_address_buffer{};

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
		{ slang_type<T>::name } -> std::convertible_to<std::string_view>;
	};

	template <typename T>
	concept is_shader_struct = has_annotation<shader_struct_tag>(^^T);

	template <typename T>
	concept is_shader_enum = std::is_enum_v<T> && has_annotation<shader_enum_tag>(^^T);

	template <typename T>
	concept is_shader_binding = find_binding_type(^^T) != std::meta::info{};

	template <typename T>
	concept is_shader_user_type = is_shader_struct<T> || is_shader_enum<T>;

	template <is_shader_struct T>
	auto emit_slang_struct(
	) -> std::string;

	template <is_shader_enum E>
	auto emit_slang_enum(
	) -> std::string;

	template <is_shader_binding T>
	auto emit_slang_binding(
	) -> std::string;

	template <typename T>
	consteval auto slang_scalar_align(
	) -> std::size_t;

	template <typename T>
	consteval auto slang_scalar_size(
	) -> std::size_t;
}

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

template <gse::shaders::has_slang_type T, std::size_t N> requires (N >= 2 && N <= 4)
struct gse::shaders::slang_type<gse::vec<T, N>> {
	static constexpr std::string_view name{
		vec_name_storage<T, N>::value.data(),
		vec_name_storage<T, N>::value.size()
	};
};

template <gse::shaders::has_slang_type T, std::size_t Cols, std::size_t Rows>
	requires (Cols >= 2 && Cols <= 4 && Rows >= 2 && Rows <= 4)
struct gse::shaders::slang_type<gse::mat<T, Cols, Rows>> {
	static constexpr std::string_view name{
		mat_name_storage<T, Cols, Rows>::value.data(),
		mat_name_storage<T, Cols, Rows>::value.size()
	};
};

template <gse::shaders::is_shader_struct T>
auto gse::shaders::emit_slang_struct() -> std::string {
	std::string out = std::format("public struct {} {{\n", std::meta::identifier_of(^^T));
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_t = [: std::meta::type_of(m) :];
		out += std::format("    public {} {};\n", slang_type<member_t>::name, std::meta::identifier_of(m));
	}
	out += "};\n";
	return out;
}

template <gse::shaders::is_shader_enum E>
auto gse::shaders::emit_slang_enum() -> std::string {
	std::string out = std::format("public enum class {} {{\n", std::meta::identifier_of(^^E));
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
		out += std::format(
			"    {} = {},\n",
			std::meta::identifier_of(e),
			static_cast<std::underlying_type_t<E>>([: e :])
		);
	}
	out += "};\n";
	return out;
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
	using binding_t = [: find_binding_type(^^T) :];
	constexpr auto name = std::meta::identifier_of(^^T);
	if constexpr (has_annotation<sampler2d_tag>(^^T)) {
		return std::format("public [[vk::binding({}, {})]] Sampler2D {};\n", binding_t::slot, binding_t::set, name);
	}
	else if constexpr (has_annotation<tlas_tag>(^^T)) {
		return std::format("public [[vk::binding({}, {})]] RaytracingAccelerationStructure {};\n", binding_t::slot, binding_t::set, name);
	}
	else if constexpr (has_annotation<byte_address_buffer_tag>(^^T)) {
		return std::format("public [[vk::binding({}, {})]] ByteAddressBuffer {};\n", binding_t::slot, binding_t::set, name);
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
		std::string out = std::format("[[vk::binding({}, {})]]\npublic cbuffer {} {{\n", binding_t::slot, binding_t::set, name);
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^element_t, std::meta::access_context::unchecked()))) {
			using member_t = [: std::meta::type_of(m) :];
			out += std::format("    public {} {};\n", slang_type<member_t>::name, std::meta::identifier_of(m));
		}
		out += "};\n";
		return out;
	}
	else {
		std::string out = std::format("[[vk::binding({}, {})]]\npublic cbuffer {} {{\n", binding_t::slot, binding_t::set, name);
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			using member_t = [: std::meta::type_of(m) :];
			out += std::format("    public {} {};\n", slang_type<member_t>::name, std::meta::identifier_of(m));
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
			using member_t = [: std::meta::type_of(m) :];
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
			using member_t = [: std::meta::type_of(m) :];
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

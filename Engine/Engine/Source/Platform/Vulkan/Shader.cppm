export module gse.platform:shader;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_push_constants;

import gse.assert;
import gse.utility;

namespace gse {
	export class shader final : public identifiable, non_copyable {
	public:
		struct info {
			std::string name;
			std::filesystem::path path;
		};

		struct vertex_input {
			static_vector<gpu::vertex_binding_desc, 4>   bindings;
			static_vector<gpu::vertex_attribute_desc, 16> attributes;
		};

		struct uniform_member {
			std::string name;
			std::string type_name;
			std::uint32_t offset = 0;
			std::uint32_t size = 0;
			std::uint32_t array_size = 0;
		};

		struct uniform_block {
			std::string name;
			std::uint32_t binding = 0;
			std::uint32_t set = 0;
			std::uint32_t size = 0;
			std::unordered_map<std::string, uniform_member> members;
			gpu::stage_flags stage_flags;
		};

		struct binding {
			std::string name;
			gpu::descriptor_binding_desc desc;
			std::optional<uniform_block> member;
		};

		struct set {
			gpu::descriptor_set_type type = gpu::descriptor_set_type::persistent;
			std::vector<binding> bindings;
		};

		struct layout {
			std::unordered_map<gpu::descriptor_set_type, set> sets;
		};

		explicit shader(const std::filesystem::path& path);

		auto load(
			const auto& context
		) -> void;

		auto unload(
		) -> void;

		[[nodiscard]] auto is_compute(
		) const -> bool;

		[[nodiscard]] auto is_mesh_shader(
		) const -> bool;

		[[nodiscard]] auto required_bindings(
		) const -> std::vector<std::string>;

		[[nodiscard]] auto push_block(
			const std::string& name
		) const -> uniform_block;

		[[nodiscard]] auto uniform_block(
			const std::string& name
		) const -> uniform_block;

		[[nodiscard]] auto uniform_blocks(
		) const -> std::vector<class uniform_block>;

		template <typename T>
		auto set_uniform(
			std::string_view full_name,
			const T& value,
			const gpu::buffer& buf
		) const -> void;

		auto set_uniform_block(
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& data,
			const gpu::buffer& buf
		) const -> void;

		auto set_ssbo_element(
			std::string_view block_name,
			std::uint32_t index,
			std::string_view member_name,
			std::span<const std::byte> bytes,
			const gpu::buffer& buf
		) const -> void;

		auto set_ssbo_struct(
			std::string_view block_name,
			std::uint32_t index,
			std::span<const std::byte> element_bytes,
			const gpu::buffer& buf
		) const -> void;

		auto cache_push_block(
			std::string_view block_name
		) const -> gpu::cached_push_constants;

		[[nodiscard]] auto spirv(
			gpu::shader_stage stage
		) const -> std::span<const std::uint32_t>;

		[[nodiscard]] auto vertex_input_data(
		) const -> const vertex_input&;

		[[nodiscard]] auto layout_data(
		) const -> const layout&;

		[[nodiscard]] auto push_constants(
		) const -> std::span<const struct uniform_block>;

		[[nodiscard]] auto layout_name(
		) const -> const std::string&;

		[[nodiscard]] auto shader_info(
		) const -> const info&;
	private:
		std::vector<std::uint32_t> m_vert_spirv;
		std::vector<std::uint32_t> m_frag_spirv;
		std::vector<std::uint32_t> m_compute_spirv;
		std::vector<std::uint32_t> m_task_spirv;
		std::vector<std::uint32_t> m_mesh_spirv;
		bool m_is_compute = false;
		bool m_is_mesh_shader_pipeline = false;

		layout m_layout;
		vertex_input m_vertex_input;
		static_vector<struct uniform_block, 8> m_push_constants;
		info m_info;
		std::string m_layout_name;

		[[nodiscard]] auto find_binding(
			std::string_view name
		) const -> const binding*;

		[[nodiscard]] auto find_block(
			std::string_view name
		) const -> const struct uniform_block*;
	};
}

export namespace gse {
    template<archive Ar>
    auto serialize(Ar& ar, shader::uniform_member& m) -> void {
        ar & m.name & m.offset & m.size & m.array_size;
    }

    template<archive Ar>
    auto serialize(Ar& ar, struct shader::uniform_block& b) -> void {
        ar & b.name & b.binding & b.set & b.size & b.members & b.stage_flags;
    }

    template<archive Ar>
    auto serialize(Ar& ar, shader::binding& b) -> void {
        ar & b.name & b.desc.binding & b.desc.type & b.desc.count & b.desc.stages & b.member;
    }

    template<archive Ar>
    auto serialize(Ar& ar, shader::set& s) -> void {
        ar & s.type & s.bindings;
    }

    template<archive Ar>
    auto serialize(Ar& ar, shader::layout& l) -> void {
        ar & l.sets;
    }

    template<archive Ar>
    auto serialize(Ar& ar, gpu::vertex_attribute_desc& attr) -> void {
        ar & attr.location & attr.binding & attr.format & attr.offset;
    }
}

gse::shader::shader(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path) {
	m_info = {
		.name = path.stem().string(),
		.path = path
	};
}

auto gse::shader::load(const auto&) -> void {
	std::ifstream in(m_info.path, std::ios::binary);
	assert(in.is_open(), std::source_location::current(), "Failed to open gshader asset: {}", m_info.path.string());
	if (!in.is_open()) return;

	binary_reader ar(in, 0x47534852, 1, m_info.path.string());
	if (!ar.valid()) return;

	std::uint8_t shader_type = 0;
	ar & shader_type;
	m_is_compute = (shader_type == 1);
	m_is_mesh_shader_pipeline = (shader_type == 2);

	ar & m_layout_name;
	ar & m_vertex_input.attributes;
	ar & m_layout;
	ar & m_push_constants;

	if (m_is_compute) {
		ar & raw_blob(m_compute_spirv);
	} else if (m_is_mesh_shader_pipeline) {
		ar & raw_blob(m_task_spirv);
		ar & raw_blob(m_mesh_spirv);
		ar & raw_blob(m_frag_spirv);
	} else {
		ar & raw_blob(m_vert_spirv);
		ar & raw_blob(m_frag_spirv);
	}

	if (!m_vertex_input.attributes.empty()) {
		std::ranges::sort(m_vertex_input.attributes, {}, &gpu::vertex_attribute_desc::location);

		auto format_size = [](const gpu::vertex_format fmt) -> std::uint32_t {
			switch (fmt) {
				case gpu::vertex_format::r32_sfloat:
				case gpu::vertex_format::r32_sint:
				case gpu::vertex_format::r32_uint:            return 4;
				case gpu::vertex_format::r32g32_sfloat:
				case gpu::vertex_format::r32g32_sint:
				case gpu::vertex_format::r32g32_uint:         return 8;
				case gpu::vertex_format::r32g32b32_sfloat:
				case gpu::vertex_format::r32g32b32_sint:
				case gpu::vertex_format::r32g32b32_uint:      return 12;
				case gpu::vertex_format::r32g32b32a32_sfloat:
				case gpu::vertex_format::r32g32b32a32_sint:
				case gpu::vertex_format::r32g32b32a32_uint:   return 16;
			}
			return 0;
		};

		std::uint32_t stride = 0;
		for (auto& attr : m_vertex_input.attributes) {
			attr.offset = stride;
			stride += format_size(attr.format);
		}

		m_vertex_input.bindings.push_back({
			.binding = 0,
			.stride = stride,
			.per_instance = false
		});
	}
}

auto gse::shader::unload() -> void {
	m_vert_spirv.clear();
	m_frag_spirv.clear();
	m_compute_spirv.clear();
	m_task_spirv.clear();
	m_mesh_spirv.clear();
	m_layout = {};
	m_vertex_input = {};
	m_push_constants.clear();
	m_info = {};
	m_layout_name.clear();
}

auto gse::shader::is_compute() const -> bool {
	return m_is_compute;
}

auto gse::shader::is_mesh_shader() const -> bool {
	return m_is_mesh_shader_pipeline;
}

auto gse::shader::required_bindings() const -> std::vector<std::string> {
	std::vector<std::string> result;
	for (const auto& [type, bindings] : m_layout.sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.desc.type == gpu::descriptor_type::storage_buffer) {
				result.push_back(b.name);
			}
		}
	}
	return result;
}

auto gse::shader::push_block(const std::string& name) const -> struct uniform_block {
	const auto it = std::ranges::find_if(m_push_constants, [&](auto& b) { return b.name == name; });
	assert(it != m_push_constants.end(), std::source_location::current(), "Push constant block '{}' not found", name);
	return *it;
}

auto gse::shader::cache_push_block(const std::string_view block_name) const -> gpu::cached_push_constants {
	const auto it = std::ranges::find_if(m_push_constants, [&](const struct uniform_block& b) {
		return b.name == block_name;
	});

	assert(it != m_push_constants.end(), std::source_location::current(), "Push constant block '{}' not found", block_name);
	std::unordered_map<std::string, gpu::push_constant_member> members;
	for (const auto& [name, member] : it->members) {
		members[name] = {
			.offset = member.offset, 
			.size = member.size
		};
	}
	return {
		.members = std::move(members),
		.data = std::vector(it->size, std::byte{ 0 }),
		.stage_flags = it->stage_flags,
	};
}

auto gse::shader::uniform_block(const std::string& name) const -> class uniform_block {
	const auto* block = find_block(name);
	assert(block, std::source_location::current(), "Uniform block '{}' not found in shader", name);
	return *block;
}

auto gse::shader::uniform_blocks() const -> std::vector<struct uniform_block> {
	std::vector<struct uniform_block> blocks;
	for (const auto& [type, bindings] : m_layout.sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.member.has_value()) {
				blocks.push_back(b.member.value());
			}
		}
	}
	return blocks;
}

auto gse::shader::find_binding(const std::string_view name) const -> const binding* {
	for (const auto& [type, bindings] : m_layout.sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.name == name) return &b;
		}
	}
	return nullptr;
}

auto gse::shader::find_block(const std::string_view name) const -> const struct uniform_block* {
	for (const auto& [type, bindings] : m_layout.sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.member.has_value() && b.member->name == name) {
				return &b.member.value();
			}
		}
	}
	return nullptr;
}

template <typename T>
auto gse::shader::set_uniform(const std::string_view full_name, const T& value, const gpu::buffer& buf) const -> void {
	const auto dot_pos = full_name.find('.');
	assert(dot_pos != std::string_view::npos, std::source_location::current(), "Uniform name '{}' must be in the format 'Block.member'", full_name);

	const auto block_name = full_name.substr(0, dot_pos);
	const auto member_name = full_name.substr(dot_pos + 1);

	const auto* block = find_block(block_name);
	assert(block, std::source_location::current(), "Uniform block '{}' not found", block_name);

	const auto mem_it = block->members.find(std::string(member_name));
	assert(mem_it != block->members.end(), std::source_location::current(), "Member '{}' not found in block '{}'", member_name, block_name);

	const auto& mem_info = mem_it->second;
	assert(sizeof(T) <= mem_info.size, std::source_location::current(), "Value size {} exceeds member '{}' size {}", sizeof(T), member_name, mem_info.size);

	auto* mapped = buf.mapped();
	assert(mapped, std::source_location::current(), "Attempted to set uniform '{}.{}' but memory is not mapped", block_name, member_name);

	gse::memcpy(mapped + mem_info.offset, value);
}

auto gse::shader::set_uniform_block(const std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>& data, const gpu::buffer& buf) const -> void {
	const auto* block = find_block(block_name);
	assert(block, std::source_location::current(), "Uniform block '{}' not found", block_name);

	auto* mapped = buf.mapped();
	assert(mapped, std::source_location::current(), "Attempted to set uniform block but memory is not mapped");

	for (const auto& [name, bytes] : data) {
		auto member_it = block->members.find(name);
		assert(member_it != block->members.end(), std::source_location::current(), "Uniform member '{}' not found in block '{}'", name, block_name);

		const auto& member_info = member_it->second;
		assert(bytes.size() <= member_info.size, std::source_location::current(), "Data size {} > member size {} for '{}.{}'", bytes.size(), member_info.size, block_name, name);

		gse::memcpy(mapped + member_info.offset, bytes);
	}
}

auto gse::shader::set_ssbo_element(const std::string_view block_name, const std::uint32_t index, const std::string_view member_name, const std::span<const std::byte> bytes, const gpu::buffer& buf) const -> void {
	const auto* block = find_block(block_name);
	assert(block, std::source_location::current(), "SSBO '{}' not found", block_name);
	assert(block->size > 0, std::source_location::current(), "SSBO '{}' has zero element stride", block_name);

	auto* mapped = buf.mapped();
	assert(mapped, std::source_location::current(), "Attempted to set SSBO but memory is not mapped");

	const auto mit = block->members.find(std::string(member_name));
	assert(mit != block->members.end(), std::source_location::current(), "Member '{}' not found in SSBO '{}'", member_name, block_name);

	const auto& m_info = mit->second;
	assert(bytes.size() <= m_info.size, std::source_location::current(), "Bytes size {} > member '{}' size {} in SSBO '{}'", bytes.size(), member_name, m_info.size, block_name);

	gse::memcpy(mapped + index * block->size + m_info.offset, bytes);
}

auto gse::shader::set_ssbo_struct(const std::string_view block_name, const std::uint32_t index, const std::span<const std::byte> element_bytes, const gpu::buffer& buf) const -> void {
	const auto* block = find_block(block_name);
	assert(block, std::source_location::current(), "SSBO '{}' not found", block_name);
	assert(block->size > 0, std::source_location::current(), "SSBO '{}' has zero element stride", block_name);

	auto* mapped = buf.mapped();
	assert(mapped, std::source_location::current(), "Attempted to set SSBO but memory is not mapped");
	assert(element_bytes.size() == block->size, std::source_location::current(), "Element bytes {} != stride {} for SSBO '{}'", element_bytes.size(), block->size, block_name);

	gse::memcpy(mapped + index * block->size, element_bytes);
}

auto gse::shader::spirv(const gpu::shader_stage stage) const -> std::span<const std::uint32_t> {
	switch (stage) {
		case gpu::shader_stage::vertex:   return m_vert_spirv;
		case gpu::shader_stage::fragment: return m_frag_spirv;
		case gpu::shader_stage::compute:  return m_compute_spirv;
		case gpu::shader_stage::task:     return m_task_spirv;
		case gpu::shader_stage::mesh:     return m_mesh_spirv;
	}
	return {};
}

auto gse::shader::vertex_input_data() const -> const vertex_input& {
	return m_vertex_input;
}

auto gse::shader::layout_data() const -> const layout& {
	return m_layout;
}

auto gse::shader::push_constants() const -> std::span<const struct uniform_block> {
	return m_push_constants.span();
}

auto gse::shader::layout_name() const -> const std::string& {
	return m_layout_name;
}

auto gse::shader::shader_info() const -> const info& {
	return m_info;
}

module;

#include <cstdio>
#include <slang-com-ptr.h>
#include <slang.h>

#undef assert

export module gse.platform:shader;

import std;

import :gpu_context;
import :shader_layout;
import :render_graph;
import :descriptor_heap;

import gse.assert;
import :vulkan_allocator;
import gse.utility;

namespace gse {
	export class shader final : public identifiable, non_copyable {
	public:
		struct info {
			std::string name;
			std::filesystem::path path;
		};

		struct vertex_input {
			std::vector<vk::VertexInputBindingDescription> bindings;
			std::vector<vk::VertexInputAttributeDescription> attributes;
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
			vk::ShaderStageFlags stage_flags;
		};

		struct binding {
			std::string name;
			vk::DescriptorSetLayoutBinding layout_binding;
			std::optional<uniform_block> member;
		};

		struct set {
			enum class binding_type : std::uint8_t {
				persistent = 0,
				push = 1,
				bind_less = 2,
			};

			binding_type type;
			std::variant<std::monostate, std::shared_ptr<vk::raii::DescriptorSetLayout>, vk::DescriptorSetLayout> layout;
			std::vector<binding> bindings;
		};

		struct layout {
			std::unordered_map<set::binding_type, set> sets;
		};


		explicit shader(const std::filesystem::path& path);

		auto load(
			const gpu::context& context
		) -> void;

		auto unload(
		) -> void;

		auto is_compute(
		) const -> bool;

		auto shader_stages(
		) const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;

		auto compute_stage(
		) const -> vk::PipelineShaderStageCreateInfo;

		auto required_bindings(
		) const -> std::vector<std::string>;

		auto push_block(
			const std::string& name
		) const -> uniform_block;

		auto uniform_block(
			const std::string& name
		) const -> uniform_block;

		auto uniform_blocks(
		) const -> std::vector<class uniform_block>;

		auto binding(
			const std::string& name
		) const -> std::optional<vk::DescriptorSetLayoutBinding>;

		auto layout(
			set::binding_type type
		) const -> vk::DescriptorSetLayout;

		auto layouts(
		) const -> std::vector<vk::DescriptorSetLayout>;

		auto push_constant_range(
			const std::string_view& name
		) const -> vk::PushConstantRange;

		auto vertex_input_state(
		) const -> vk::PipelineVertexInputStateCreateInfo;

		auto allocate_descriptors(
			vulkan::descriptor_heap& heap
		) const -> vulkan::descriptor_region;

		auto create_writer(
			vulkan::descriptor_heap& heap,
			set::binding_type type
		) const -> vulkan::descriptor_writer;

		auto write_descriptors(
			const vulkan::descriptor_region& region,
			const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
			const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos,
			const std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>>& image_array_infos = {}
		) const -> void;

		template <typename T>
		auto set_uniform(
			std::string_view full_name,
			const T& value,
			const vulkan::allocation& alloc
		) const -> void;

		auto set_uniform_block(
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& data,
			const vulkan::allocation& alloc
		) const -> void;

		auto set_ssbo_element(
			std::string_view block_name,   
			std::uint32_t index,        
			std::string_view member_name, 
			std::span<const std::byte> bytes,
			const vulkan::allocation& alloc
		) const -> void;

		auto set_ssbo_struct(
			std::string_view block_name,
			std::uint32_t index,
			std::span<const std::byte> element_bytes,
			const vulkan::allocation& alloc
		) const -> void;

		auto push_descriptor(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view name,
			const vk::DescriptorImageInfo& image_info
		) const -> void;

		auto push_descriptor(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view name,
			const vk::DescriptorBufferInfo& buffer_info
		) const -> void;

		auto cache_push_block(
			std::string_view block_name
		) const -> vulkan::cached_push_constants;

		auto push_bytes(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view block_name,
			const void* data,
			std::size_t offset = 0
		) const -> void;

		template <typename... NameValue>
		auto push(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view block_name,
			NameValue&&... name_value_pairs
		) const -> void;

		auto is_mesh_shader() const -> bool;
		auto mesh_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 3>;
	private:
		vk::raii::ShaderModule m_vert_module = nullptr;
		vk::raii::ShaderModule m_frag_module = nullptr;
		vk::raii::ShaderModule m_compute_module = nullptr;
		vk::raii::ShaderModule m_task_module = nullptr;
		vk::raii::ShaderModule m_mesh_module = nullptr;
		bool m_is_compute = false;
		bool m_is_mesh_shader_pipeline = false;

		struct layout m_layout;
		vertex_input m_vertex_input;
		std::vector<struct uniform_block> m_push_constants;
		info m_info;
		std::string m_layout_name;

		static auto layout(
			const set& set
		) -> vk::DescriptorSetLayout {
			if (std::holds_alternative<std::shared_ptr<vk::raii::DescriptorSetLayout>>(set.layout)) {
				return **std::get<std::shared_ptr<vk::raii::DescriptorSetLayout>>(set.layout);
			}
			if (std::holds_alternative<vk::DescriptorSetLayout>(set.layout)) {
				return std::get<vk::DescriptorSetLayout>(set.layout);
			}
			assert(false, std::source_location::current(), "Shader set layout is not initialized");
			return {};
		}
	};
}

gse::shader::shader(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path) {
	m_info = {
		.name = path.stem().string(),
		.path = path
	};
}

auto gse::shader::load(const gpu::context& context) -> void {
	std::ifstream in(m_info.path, std::ios::binary);
	assert(in.is_open(), std::source_location::current(), "Failed to open gshader asset: {}", m_info.path.string());

	auto read_string = [](
		std::ifstream& stream,
		std::string& str
		) {
			std::uint32_t size;
			stream.read(reinterpret_cast<char*>(&size), sizeof(size));
			str.resize(size);
			stream.read(str.data(), size);
		};

	auto read_data = [](
		std::ifstream& stream,
		auto& value
		) {
			stream.read(reinterpret_cast<char*>(&value), sizeof(value));
		};

	std::uint8_t shader_type = 0;
	read_data(in, shader_type);
	m_is_compute = (shader_type == 1);
	m_is_mesh_shader_pipeline = (shader_type == 2);

	read_string(in, m_layout_name);

	std::uint32_t attr_count = 0;
	read_data(in, attr_count);
	m_vertex_input.attributes.resize(attr_count);
	for (auto& attr : m_vertex_input.attributes) read_data(in, attr);

	// Always read reflected sets data (uniform blocks are always serialized)
	std::unordered_map<set::binding_type, set> reflected_sets;
	{
		std::uint32_t num_sets = 0;
		read_data(in, num_sets);
		for (std::uint32_t i = 0; i < num_sets; ++i) {
			set::binding_type type;
			read_data(in, type);
			std::uint32_t num_bindings = 0;
			read_data(in, num_bindings);
			std::vector<struct binding> bindings;
			for (std::uint32_t j = 0; j < num_bindings; ++j) {
				struct binding b;
				read_string(in, b.name);
				read_data(in, b.layout_binding);
				bool has_member = false;
				read_data(in, has_member);
				if (has_member) {
					struct uniform_block member;
					read_string(in, member.name);
					read_data(in, member.binding);
					read_data(in, member.set);
					read_data(in, member.size);
					std::uint32_t num_ubo_members = 0;
					read_data(in, num_ubo_members);
					for (std::uint32_t k = 0; k < num_ubo_members; ++k) {
						std::string key_name;
						uniform_member ubo_member;
						read_string(in, key_name);
						read_string(in, ubo_member.name);
						read_data(in, ubo_member.offset);
						read_data(in, ubo_member.size);
						read_data(in, ubo_member.array_size);
						member.members[key_name] = ubo_member;
					}
					b.member = member;
				}
				bindings.push_back(std::move(b));
			}
			reflected_sets[type] = { .type = type, .bindings = std::move(bindings) };
		}
	}

	if (m_layout_name.empty()) {
		m_layout.sets = std::move(reflected_sets);
	}
	else {
		const auto* layout_ptr = context.shader_layout(m_layout_name);
		assert(layout_ptr, std::source_location::current(), "Shader layout '{}' not found", m_layout_name);

		for (const auto& [set_index, layout_bindings] : layout_ptr->sets()) {
			auto type = static_cast<set::binding_type>(set_index);
			std::vector<struct binding> resolved_bindings;
			resolved_bindings.reserve(layout_bindings.size());

			for (const auto& [name, layout_binding] : layout_bindings) {
				std::optional<struct uniform_block> member_opt;
				if (auto set_it = reflected_sets.find(type); set_it != reflected_sets.end()) {
					for (const auto& reflected_binding : set_it->second.bindings) {
						if (reflected_binding.name == name && reflected_binding.member.has_value()) {
							member_opt = reflected_binding.member;
							break;
						}
					}
				}

				resolved_bindings.push_back({
					.name = name,
					.layout_binding = layout_binding,
					.member = member_opt
				});
			}

			m_layout.sets[type] = {
				.type = type,
				.layout = layout_ptr->vk_layout(set_index),
				.bindings = std::move(resolved_bindings)
			};
		}

		for (auto& [type, reflected_set] : reflected_sets) {
			if (!m_layout.sets.contains(type)) {
				m_layout.sets[type] = std::move(reflected_set);
			}
		}
	}

	std::uint32_t pc_count = 0;
	read_data(in, pc_count);
	m_push_constants.resize(pc_count);
	for (auto& pc : m_push_constants) {
		read_string(in, pc.name);
		read_data(in, pc.size);
		read_data(in, pc.stage_flags);
		std::uint32_t member_count = 0;
		read_data(in, member_count);
		for (std::uint32_t i = 0; i < member_count; ++i) {
			std::string key, name;
			std::uint32_t offset = 0, size = 0, arr = 0;
			read_string(in, key);
			read_string(in, name);
			read_data(in, offset);
			read_data(in, size);
			read_data(in, arr);
			pc.members[key] = {
				.name = name,
				.type_name = {},
				.offset = offset,
				.size = size,
				.array_size = arr
			};
		}
	}

	auto read_shader_module = [&]() -> vk::raii::ShaderModule {
		std::uint64_t size = 0;
		read_data(in, size);
		std::vector<char> code(size);
		in.read(code.data(), size);
		return context.device().createShaderModule({
			.codeSize = code.size(),
			.pCode = reinterpret_cast<const std::uint32_t*>(code.data())
		});
	};

	if (m_is_compute) {
		m_compute_module = read_shader_module();
	} else if (m_is_mesh_shader_pipeline) {
		m_task_module = read_shader_module();
		m_mesh_module = read_shader_module();
		m_frag_module = read_shader_module();
	} else {
		m_vert_module = read_shader_module();
		m_frag_module = read_shader_module();
	}

	if (!m_vertex_input.attributes.empty()) {
		std::ranges::sort(m_vertex_input.attributes, {}, &vk::VertexInputAttributeDescription::location);
		std::uint32_t stride = 0;

		auto format_size = [](const vk::Format format) -> std::uint32_t {
			switch (format) {
				case vk::Format::eR32Sfloat:
				case vk::Format::eR32Sint:
				case vk::Format::eR32Uint:                 return 4;
				case vk::Format::eR32G32Sfloat:
				case vk::Format::eR32G32Sint:
				case vk::Format::eR32G32Uint:              return 8;
				case vk::Format::eR32G32B32Sfloat:
				case vk::Format::eR32G32B32Sint:
				case vk::Format::eR32G32B32Uint:           return 12;
				case vk::Format::eR32G32B32A32Sfloat:
				case vk::Format::eR32G32B32A32Sint:
				case vk::Format::eR32G32B32A32Uint:        return 16;
				default:
					assert(false, std::source_location::current(), "Unsupported vertex format in size calc");
					return 0;
			}
		};

		for (auto& attr : m_vertex_input.attributes) {
			attr.offset = stride;
			stride += format_size(attr.format);
		}

		m_vertex_input.bindings.push_back({
			.binding = 0,
			.stride = stride,
			.inputRate = vk::VertexInputRate::eVertex
		});
	}

	for (auto& [type, set_data] : m_layout.sets) {
		if (!std::holds_alternative<std::monostate>(set_data.layout)) continue;

		std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
		raw_bindings.reserve(set_data.bindings.size());

		for (const auto& b : set_data.bindings) {
			auto lb = b.layout_binding;
			lb.pImmutableSamplers = nullptr;
			raw_bindings.push_back(lb);
		}

		vk::DescriptorSetLayoutCreateInfo ci{
			.flags = type == set::binding_type::push
				? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR
				: vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
			.bindingCount = static_cast<uint32_t>(raw_bindings.size()),
			.pBindings = raw_bindings.data()
		};

		set_data.layout = std::make_shared<vk::raii::DescriptorSetLayout>(context.device(), ci);
	}

	std::uint32_t max_set_index = 0;
	for (const auto& t : m_layout.sets | std::views::keys) {
		max_set_index = std::max(max_set_index, static_cast<uint32_t>(t));
	}

	for (uint32_t i = 0; i <= max_set_index; ++i) {
		if (auto t = static_cast<set::binding_type>(i); !m_layout.sets.contains(t)) {
			vk::DescriptorSetLayoutCreateInfo ci{
				.flags = t == set::binding_type::push
					? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR
					: vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
				.bindingCount = 0,
				.pBindings = nullptr
			};
			set empty_set{
				.type = t,
				.layout = std::make_shared<vk::raii::DescriptorSetLayout>(context.device(), ci)
			};
			m_layout.sets.emplace(t, std::move(empty_set));
		}
	}
}

auto gse::shader::unload() -> void {
	m_vert_module = nullptr;
	m_frag_module = nullptr;
	m_compute_module = nullptr;
	m_layout = {};
	m_vertex_input = {};
	m_push_constants.clear();
	m_info = {};
	m_layout_name.clear();
}

auto gse::shader::is_compute() const -> bool {
	return m_is_compute;
}

auto gse::shader::shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2> {
	assert(!m_is_compute, std::source_location::current(), "Cannot get graphics shader stages from a compute shader");
	return {
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = *m_vert_module,
			.pName = "main"
		},
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = *m_frag_module,
			.pName = "main"
		}
	};
}

auto gse::shader::compute_stage() const -> vk::PipelineShaderStageCreateInfo {
	assert(m_is_compute, std::source_location::current(), "Cannot get compute shader stage from a graphics shader");
	return {
		.stage = vk::ShaderStageFlagBits::eCompute,
		.module = *m_compute_module,
		.pName = "main"
	};
}

auto gse::shader::is_mesh_shader() const -> bool {
	return m_is_mesh_shader_pipeline;
}

auto gse::shader::mesh_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 3> {
	assert(m_is_mesh_shader_pipeline, std::source_location::current(), "Cannot get mesh shader stages from a non-mesh shader");
	return {
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eTaskEXT,
			.module = *m_task_module,
			.pName = "main"
		},
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eMeshEXT,
			.module = *m_mesh_module,
			.pName = "main"
		},
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = *m_frag_module,
			.pName = "main"
		}
	};
}

auto gse::shader::required_bindings() const -> std::vector<std::string> {
	std::vector<std::string> required_bindings;
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [member_name, layout_binding, member] : s.bindings) {
			if (layout_binding.descriptorType == vk::DescriptorType::eStorageBuffer) {
				required_bindings.push_back(member_name);
			}
		}
	}
	return required_bindings;
}

auto gse::shader::push_block(const std::string& name) const -> struct uniform_block {
	const auto it = std::ranges::find_if(m_push_constants, [&](auto& b){ return b.name == name; });
    assert(it != m_push_constants.end(), std::source_location::current(), "Push constant block '{}' not found", name);
    return *it;
}

auto gse::shader::cache_push_block(const std::string_view block_name) const -> vulkan::cached_push_constants {
	const auto it = std::ranges::find_if(m_push_constants, [&](auto& b) { return b.name == block_name; });
	assert(it != m_push_constants.end(), std::source_location::current(), "Push constant block '{}' not found", block_name);
	std::unordered_map<std::string, vulkan::push_constant_member> members;
	for (const auto& [name, member] : it->members) {
		members[name] = { .offset = member.offset, .size = member.size };
	}
	return {
		.data = std::vector<std::byte>(it->size, std::byte{ 0 }),
		.stage_flags = it->stage_flags,
		.members = std::move(members)
	};
}

auto gse::shader::uniform_block(const std::string& name) const -> class uniform_block {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& b : s.bindings) {
			if (b.member.has_value() && b.member.value().name == name) {
				return b.member.value();
			}
		}
	}
	assert(
		false,
		std::source_location::current(),
		"Uniform block '{}' not found in shader",
		name
	);
	return {};
}

auto gse::shader::uniform_blocks() const -> std::vector<struct uniform_block> {
	std::vector<struct uniform_block> blocks;
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& b : s.bindings) {
			if (b.member.has_value()) {
				blocks.push_back(b.member.value());
			}
		}
	}
	return blocks;
}

auto gse::shader::binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding> {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [binding_name, layout_binding, member] : s.bindings) {
			if (binding_name == name) {
				return layout_binding;
			}
		}
	}
	return std::nullopt;
}

auto gse::shader::layout(const set::binding_type type) const -> vk::DescriptorSetLayout {
	assert(
		m_layout.sets.contains(type),
		std::source_location::current(),
		"Shader does not have set of type {}",
		static_cast<int>(type)
	);
	return layout(m_layout.sets.at(type));
}

auto gse::shader::layouts() const -> std::vector<vk::DescriptorSetLayout> {
	std::map<set::binding_type, vk::DescriptorSetLayout> sorted_layouts;
	for (const auto& [type, set] : m_layout.sets) {
		sorted_layouts[type] = layout(set);
	}

	std::vector<vk::DescriptorSetLayout> result;
	for (const auto& layout : sorted_layouts | std::views::values) {
		result.push_back(layout);
	}
	return result;
}

auto gse::shader::push_constant_range(const std::string_view& name) const -> vk::PushConstantRange {
	const auto it = std::ranges::find_if(
		m_push_constants,
		[&](const auto& block) {
			return block.name == name;
		}
	);

	assert(it != m_push_constants.end(), std::source_location::current(), "Push constant block '{}' not found in shader", name);

	return vk::PushConstantRange{
		it->stage_flags,
		0,
		it->size
	};
}

auto gse::shader::vertex_input_state() const -> vk::PipelineVertexInputStateCreateInfo {
	return {
		.flags = {},
		.vertexBindingDescriptionCount = static_cast<std::uint32_t>(m_vertex_input.bindings.size()),
		.pVertexBindingDescriptions = m_vertex_input.bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(m_vertex_input.attributes.size()),
		.pVertexAttributeDescriptions = m_vertex_input.attributes.data()
	};
}

auto gse::shader::create_writer(vulkan::descriptor_heap& heap, set::binding_type type) const -> vulkan::descriptor_writer {
	assert(
		m_layout.sets.contains(type),
		std::source_location::current(),
		"Shader '{}' has no set of type {}",
		m_info.name, static_cast<int>(type)
	);

	const auto& set_data = m_layout.sets.at(type);
	const auto set_layout = layout(set_data);
	const auto& props = heap.props();
	const bool is_push = (type == set::binding_type::push);

	auto descriptor_size_for = [&](vk::DescriptorType dt) -> vk::DeviceSize {
		switch (dt) {
			case vk::DescriptorType::eUniformBuffer:
			case vk::DescriptorType::eUniformBufferDynamic:
				return props.uniform_buffer_descriptor_size;
			case vk::DescriptorType::eStorageBuffer:
			case vk::DescriptorType::eStorageBufferDynamic:
				return props.storage_buffer_descriptor_size;
			case vk::DescriptorType::eCombinedImageSampler:
				return props.combined_image_sampler_descriptor_size;
			case vk::DescriptorType::eSampledImage:
				return props.sampled_image_descriptor_size;
			case vk::DescriptorType::eStorageImage:
				return props.storage_image_descriptor_size;
			case vk::DescriptorType::eSampler:
				return props.sampler_descriptor_size;
			default:
				return props.storage_buffer_descriptor_size;
		}
	};

	std::uint32_t max_binding = 0;
	for (const auto& b : set_data.bindings) {
		max_binding = std::max(max_binding, b.layout_binding.binding);
	}

	std::vector<vulkan::descriptor_binding_info> bindings(max_binding + 1);

	if (is_push) {
		vk::DeviceSize running_offset = 0;
		for (const auto& b : set_data.bindings) {
			const auto idx = b.layout_binding.binding;
			const auto desc_size = descriptor_size_for(b.layout_binding.descriptorType);
			bindings[idx] = {
				.offset = running_offset,
				.descriptor_size = desc_size,
				.type = b.layout_binding.descriptorType
			};
			running_offset += desc_size;
		}

		return vulkan::descriptor_writer(heap, set_layout, running_offset, std::move(bindings), true);
	}

	const auto size = heap.layout_size(set_layout);

	for (const auto& b : set_data.bindings) {
		const auto idx = b.layout_binding.binding;
		const auto boff = heap.binding_offset(set_layout, idx);
		const auto desc_size = descriptor_size_for(b.layout_binding.descriptorType);

		bindings[idx] = {
			.offset = boff,
			.descriptor_size = desc_size,
			.type = b.layout_binding.descriptorType
		};
	}

	vk::DeviceSize actual_size = size;
	for (const auto& b : bindings) {
		actual_size = std::max(actual_size, b.offset + b.descriptor_size);
	}

	return vulkan::descriptor_writer(heap, set_layout, actual_size, std::move(bindings));
}

auto gse::shader::allocate_descriptors(vulkan::descriptor_heap& heap) const -> vulkan::descriptor_region {
	assert(
		m_layout.sets.contains(set::binding_type::persistent),
		std::source_location::current(),
		"Shader has no persistent descriptor set to allocate"
	);

	const auto& persistent_set = m_layout.sets.at(set::binding_type::persistent);
	const auto set_layout = layout(persistent_set);
	const auto size = heap.layout_size(set_layout);

	log::println(log::category::vulkan, "[Descriptor] Allocating region for '{}': layout_size={}, bindings={}",
		m_info.name, size, persistent_set.bindings.size());

	return heap.allocate(size);
}

auto gse::shader::write_descriptors(
	const vulkan::descriptor_region& region,
	const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
	const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos,
	const std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>>& image_array_infos
) const -> void {
	assert(region, std::source_location::current(), "Cannot write to null descriptor region");
	auto& heap = *region.heap;
	const auto& props = heap.props();

	for (const auto& s : m_layout.sets | std::views::values) {
		if (s.type != set::binding_type::persistent) continue;

		const auto set_layout = layout(s);

		for (const auto& [binding_name, layout_binding, member] : s.bindings) {
			const bool in_bufs = buffer_infos.contains(binding_name);
			const bool in_imgs = image_infos.contains(binding_name);
			const bool in_arrs = image_array_infos.contains(binding_name);
			if (!in_bufs && !in_imgs && !in_arrs) {
				log::println(log::level::warning, log::category::vulkan,
					"[Descriptor] Binding '{}' (binding={}) not provided", binding_name, layout_binding.binding);
			}

			const auto boff = heap.binding_offset(set_layout, layout_binding.binding);

			if (auto it = buffer_infos.find(binding_name); it != buffer_infos.end()) {
				const auto& buf_info = it->second;
				assert(
					buf_info.range != vk::WholeSize,
					std::source_location::current(),
					"Descriptor buffer writes require an explicit range for '{}'",
					binding_name
				);
				const auto buf_addr = heap.buffer_address(buf_info.buffer);

				assert(
					buf_addr != 0,
					std::source_location::current(),
					"Buffer '{}' has zero device address — was it created with eShaderDeviceAddress?",
					binding_name
				);

				log::println(log::category::vulkan, "[Descriptor] Write buffer '{}': binding={}, boff={}, addr=0x{:x}, range={}, region=[{}, {}]",
					binding_name, layout_binding.binding, boff, buf_addr + buf_info.offset, buf_info.range,
					region.offset, region.size);

				const bool is_uniform = layout_binding.descriptorType == vk::DescriptorType::eUniformBuffer
					|| layout_binding.descriptorType == vk::DescriptorType::eUniformBufferDynamic;

				const vk::DescriptorAddressInfoEXT addr_info{
					.address = buf_addr + buf_info.offset,
					.range = buf_info.range,
					.format = vk::Format::eUndefined
				};

				if (is_uniform) {
					const vk::DescriptorGetInfoEXT get_info{
						.type = vk::DescriptorType::eUniformBuffer,
						.data = { .pUniformBuffer = &addr_info }
					};
					heap.write_descriptor(region, boff, get_info, props.uniform_buffer_descriptor_size);
				}
				else {
					const vk::DescriptorGetInfoEXT get_info{
						.type = vk::DescriptorType::eStorageBuffer,
						.data = { .pStorageBuffer = &addr_info }
					};
					heap.write_descriptor(region, boff, get_info, props.storage_buffer_descriptor_size);
				}
				continue;
			}

			if (auto it_arr = image_array_infos.find(binding_name); it_arr != image_array_infos.end()) {
				const auto& vec = it_arr->second;
				const auto desc_size = props.combined_image_sampler_descriptor_size;

				log::println(log::category::vulkan, "[Descriptor] Write image array '{}': binding={}, boff={}, count={}, desc_size={}",
					binding_name, layout_binding.binding, boff, vec.size(), desc_size);

				for (std::size_t i = 0; i < vec.size() && i < layout_binding.descriptorCount; ++i) {
					const vk::DescriptorImageInfo& img = vec[i];
					const vk::DescriptorGetInfoEXT get_info{
						.type = vk::DescriptorType::eCombinedImageSampler,
						.data = { .pCombinedImageSampler = &img }
					};
					heap.write_descriptor(region, boff + i * desc_size, get_info, desc_size);
				}
				continue;
			}

			if (auto it2 = image_infos.find(binding_name); it2 != image_infos.end()) {
				const vk::DescriptorImageInfo& img = it2->second;

				log::println(log::category::vulkan, "[Descriptor] Write image '{}': binding={}, boff={}, view={}, sampler={}",
					binding_name, layout_binding.binding, boff, !!img.imageView, !!img.sampler);
				const vk::DescriptorGetInfoEXT get_info{
					.type = vk::DescriptorType::eCombinedImageSampler,
					.data = { .pCombinedImageSampler = &img }
				};
				heap.write_descriptor(region, boff, get_info, props.combined_image_sampler_descriptor_size);
			}
		}
	}
}

template <typename T>
auto gse::shader::set_uniform(std::string_view full_name, const T& value, const vulkan::allocation& alloc) const -> void {
	const auto dot_pos = full_name.find('.');

	assert(
		dot_pos != std::string_view::npos,
		std::source_location::current(),
		"Uniform name '{}' must be in the format 'Block.member'",
		full_name
	);

	const auto block_name = std::string(full_name.substr(0, dot_pos));
	const auto member_name = std::string(full_name.substr(dot_pos + 1));

	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name != block_name) {
				continue;
			}

			assert(
				b.member.has_value(),
				std::source_location::current(),
				"No uniform block data for '{}'",
				block_name
			);

			const auto& ub = b.member.value();

			const auto mem_it = ub.members.find(member_name);

			assert(
				mem_it != ub.members.end(),
				std::source_location::current(),
				"Member '{}' not found in block '{}'",
				member_name,
				block_name
			);

			const auto& mem_info = mem_it->second;

			assert(
				sizeof(T) <= mem_info.size,
				std::source_location::current(),
				"Value size {} exceeds member '{}' size {}",
				sizeof(T),
				member_name,
				mem_info.size
			);

			assert(
				alloc.mapped(),
				std::source_location::current(),
				"Attempted to set uniform '{}.{}' but memory is not mapped",
				block_name,
				member_name
			);

			gse::memcpy(
				alloc.mapped() + mem_info.offset,
				value
			);
			return;
		}
	}

	assert(
		false,
		std::source_location::current(),
		"Uniform block '{}' not found",
		block_name
	);
}

auto gse::shader::set_uniform_block(const std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>& data, const vulkan::allocation& alloc) const -> void {
	const struct binding* block_binding = nullptr;
	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name == block_name && b.member.has_value()) {
				block_binding = &b;
				break;
			}
		}
		if (block_binding) {
			break;
		}
	}
	assert(block_binding, std::source_location::current(), "Uniform block '{}' not found", block_name);

	const auto& block = *block_binding->member;
	assert(alloc.mapped(), std::source_location::current(), "Attempted to set uniform block but memory is not mapped");

	for (const auto& [name, bytes] : data) {
		auto member_it = block.members.find(name);

		assert(
			member_it != block.members.end(),
			std::source_location::current(),
			"Uniform member '{}' not found in block '{}'",
			name,
			block_name
		);

		const auto& member_info = member_it->second;

		assert(
			bytes.size() <= member_info.size,
			std::source_location::current(),
			"Data size {} > member size {} for '{}.{}'",
			bytes.size(),
			member_info.size,
			block_name,
			name
		);

		gse::memcpy(
			alloc.mapped() + member_info.offset,
			bytes
		);
	}
}

auto gse::shader::set_ssbo_element(std::string_view block_name, const std::uint32_t index, std::string_view member_name, const std::span<const std::byte> bytes, const vulkan::allocation& alloc) const -> void {
	const struct binding* block_binding = nullptr;
	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name == block_name && b.member.has_value()) {
				block_binding = &b;
				break;
			}
		}
		if (block_binding) break;
	}
	assert(block_binding, std::source_location::current(), "SSBO '{}' not found", block_name);

	const auto& block = *block_binding->member;
	const auto elem_stride = block.size;
	assert(elem_stride > 0, std::source_location::current(), "SSBO '{}' has zero element stride", block_name);

	assert(alloc.mapped(), std::source_location::current(), "Attempted to set SSBO but memory is not mapped");

	const auto mit = block.members.find(std::string(member_name));
	assert(
		mit != block.members.end(),
		std::source_location::current(),
		"Member '{}' not found in SSBO '{}'",
		member_name,
		block_name
	);

	const auto& m_info = mit->second;
	assert(
		bytes.size() <= m_info.size,
		std::source_location::current(),
		"Bytes size {} > member '{}' size {} in SSBO '{}'",
		bytes.size(),
		member_name,
		m_info.size,
		block_name
	);

	const auto base = alloc.mapped();
	gse::memcpy(
		base + index * elem_stride + m_info.offset,
		bytes
	);
}

auto gse::shader::set_ssbo_struct(std::string_view block_name, const std::uint32_t index, const std::span<const std::byte> element_bytes, const vulkan::allocation& alloc) const -> void {
	const struct binding* block_binding = nullptr;
	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name == block_name && b.member.has_value()) {
				block_binding = &b;
				break;
			}
		}
		if (block_binding) break;
	}
	assert(block_binding, std::source_location::current(), "SSBO '{}' not found", block_name);

	const auto& block = *block_binding->member;
	const auto elem_stride = block.size;

	assert(elem_stride > 0, std::source_location::current(), "SSBO '{}' has zero element stride", block_name);
	assert(alloc.mapped(), std::source_location::current(), "Attempted to set SSBO but memory is not mapped");

	assert(
		element_bytes.size() == elem_stride,
		std::source_location::current(),
		"Element bytes {} != stride {} for SSBO '{}'",
		element_bytes.size(),
		elem_stride,
		block_name
	);

	const auto base = alloc.mapped();
	gse::memcpy(
		base + index * elem_stride,
		element_bytes
	);
}

auto gse::shader::push_descriptor(const vk::CommandBuffer command, const vk::PipelineLayout layout, const std::string_view name, const vk::DescriptorImageInfo& image_info) const -> void {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [member_name, layout_binding, member] : s.bindings) {
			if (member_name == name) {
				assert(
					layout_binding.descriptorType == vk::DescriptorType::eCombinedImageSampler || layout_binding.descriptorType == vk::DescriptorType::eSampledImage,
					std::source_location::current(),
					"Binding '{}' is not a combined image sampler",
					name
				);

				auto info = image_info;

				if (layout_binding.descriptorType == vk::DescriptorType::eSampledImage) {
					info.sampler = nullptr;
				}

				command.pushDescriptorSetKHR(
					vk::PipelineBindPoint::eGraphics,
					layout,
					static_cast<std::uint32_t>(s.type),
					{
						vk::WriteDescriptorSet{
							.pNext = {},
							.dstBinding = layout_binding.binding,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = layout_binding.descriptorType,
							.pImageInfo = &info,
						}
					}
				);
				return;
			}
		}
	}
	assert(false, std::source_location::current(), "Binding '{}' not found in shader", name);
}

auto gse::shader::push_descriptor(const vk::CommandBuffer command, const vk::PipelineLayout layout, const std::string_view name, const vk::DescriptorBufferInfo& buffer_info) const -> void {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [member_name, layout_binding, member] : s.bindings) {
			if (member_name == name) {
				assert(
					layout_binding.descriptorType == vk::DescriptorType::eStorageBuffer || layout_binding.descriptorType == vk::DescriptorType::eUniformBuffer,
					std::source_location::current(),
					"Binding '{}' is not a storage or uniform buffer",
					name
				);

				command.pushDescriptorSetKHR(
					vk::PipelineBindPoint::eGraphics,
					layout,
					static_cast<std::uint32_t>(s.type),
					{
						vk::WriteDescriptorSet{
							.pNext = {},
							.dstBinding = layout_binding.binding,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = layout_binding.descriptorType,
							.pBufferInfo = &buffer_info,
						}
					}
				);
				return;
			}
		}
	}
	assert(false, std::source_location::current(), "Binding '{}' not found in shader", name);
}

auto gse::shader::push_bytes(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, const void* data, const std::size_t offset) const -> void {
	const auto it = std::ranges::find_if(
		m_push_constants, 
		[&](const auto& block) {
			return block.name == block_name;
		}
	);

	assert(it != m_push_constants.end(), std::source_location::current(), "Push constant block '{}' not found in shader", block_name);

	const auto& block = *it;
	assert(offset + block.size <= 128, std::source_location::current(), "Push constant default limit is 128 bytes");

	command.pushConstants(
		layout,
		it->stage_flags,
		static_cast<std::uint32_t>(offset),
		block.size,
		data
	);
}

template <typename ... NameValue>
auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, NameValue&&... name_value_pairs) const -> void {
	static_assert(
		sizeof...(name_value_pairs) % 2 == 0,
		"push(...) expects (name, value) pairs"
	);

    const auto it = std::ranges::find_if(
        m_push_constants,
        [&](const auto& b) {
	        return b.name == block_name;
        }
    );

    assert(
		it != m_push_constants.end(),
		std::source_location::current(),
		"Push constant block '{}' not found in shader",
		block_name
	);

    const auto& block = *it;
    std::vector<std::byte> buffer(block.size, std::byte{0});

    auto write_one = [&](auto&& name_like, auto&& value_like) {
        const std::string_view name_sv = name_like;

        const auto mit = block.members.find(std::string(name_sv));
        assert(
			mit != block.members.end(),
			std::source_location::current(),
            "Member '{}' not found in push constant block '{}'", name_sv, block_name
		);

        const auto& mi = mit->second;

        auto ptr_and_size = [&]<typename T0>(const T0& v) -> std::pair<const void*, std::size_t> {
            using val = std::remove_cvref_t<T0>;
            if constexpr (std::is_same_v<val, std::span<const std::byte>>)
            {
                return { v.data(), v.size() };
            }
            else
            {
                static_assert(
					std::is_trivially_copyable_v<val>,
                    "push(): value must be trivially copyable or std::span<const std::byte>"
				);

                return { &v, sizeof(val) };
            }
        };

        const auto [src_ptr, n] = ptr_and_size(value_like);

		assert(
			n <= mi.size,
			std::source_location::current(),
			"Provided bytes for '{}' (size {}) exceed member size ({})", mi.name, n, mi.size
		);

        gse::memcpy(buffer.data() + mi.offset, src_ptr, n);
    };

    auto args = std::forward_as_tuple(std::forward<NameValue>(name_value_pairs)...);
    [&]<std::size_t... I>(std::index_sequence<I...>)
    {
        (write_one(std::get<2*I>(args), std::get<2*I + 1>(args)), ...);
    }(std::make_index_sequence<sizeof...(name_value_pairs) / 2>{});

    command.pushConstants(
        layout,
        block.stage_flags,
        0,
        static_cast<std::uint32_t>(buffer.size()),
        buffer.data()
    );
}

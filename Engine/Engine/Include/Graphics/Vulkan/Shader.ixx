module;

#include <SPIRV-Reflect/spirv_reflect.h>

export module gse.graphics.shader;

import std;
import vulkan_hpp;

import gse.platform;

namespace gse {
	export class shader {
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
			std::vector<uniform_member> members;
		};
	public:
		shader() = default;
		shader(
			const vk::raii::Device& device,
			const std::filesystem::path& vert_path,
			const std::filesystem::path& frag_path, 
			vk::DescriptorSetLayout layout = nullptr, 
			std::span<vk::DescriptorSetLayoutBinding> bindings = {}
		);

		shader(const shader&) = delete;
		auto operator=(const shader&) -> shader & = delete;

		auto create(
			const vk::raii::Device& device, 
			const std::filesystem::path& vert_path, 
			const std::filesystem::path& frag_path,
			vk::DescriptorSetLayout layout = nullptr, 
			std::span<vk::DescriptorSetLayoutBinding> expected_bindings = {}
		) -> void;

		auto get_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
		auto get_uniform_blocks() const -> const std::unordered_map<std::string, uniform_block>& { return m_uniform_blocks; }
		auto get_descriptor_set_layout() const -> vk::DescriptorSetLayout {
			if (m_custom_layout) {
				return *m_descriptor_set_layout;
			}
			return m_shared_layout;
		}
		auto get_required_bindings() const -> std::vector<std::string>;
		auto get_binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding>;

		auto get_descriptor_writes(
			vk::DescriptorSet set,
			const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
			const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos
		) const -> std::vector<vk::WriteDescriptorSet>;

		template <typename T>
		auto set_uniform(
			std::string_view full_name, 
			const T& value, 
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;

		template <typename T>
		auto set_uniform_block(
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& data,
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;
	private:
		vk::raii::ShaderModule m_vert_module = nullptr;
		vk::raii::ShaderModule m_frag_module = nullptr;

		bool m_custom_layout = false;
		vk::raii::DescriptorSetLayout m_descriptor_set_layout = nullptr;
		vk::DescriptorSetLayout m_shared_layout = nullptr;

		std::unordered_map<std::string, vk::DescriptorSetLayoutBinding> m_bindings;
		std::unordered_map<std::string, uniform_block> m_uniform_blocks;
	};

	auto read_file(const std::filesystem::path& path) -> std::vector<char>;
}

gse::shader::shader(const vk::raii::Device& device, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, vk::DescriptorSetLayout layout, const std::span<vk::DescriptorSetLayoutBinding> bindings) {
	create(device, vert_path, frag_path, layout, bindings);
}

auto gse::shader::create(
	const vk::raii::Device& device,
	const std::filesystem::path& vert_path,
	const std::filesystem::path& frag_path,
	const vk::DescriptorSetLayout layout,
	const std::span<vk::DescriptorSetLayoutBinding> expected_bindings
) -> void {
	const auto vert_code = read_file(vert_path);
	const auto frag_code = read_file(frag_path);

	auto create_shader_module = [&](const std::vector<char>& code) -> vk::raii::ShaderModule {
		const vk::ShaderModuleCreateInfo create_info{
			{},
			code.size() * sizeof(char),
			reinterpret_cast<const uint32_t*>(code.data())
		};
		return device.createShaderModule(create_info);
		};

	m_vert_module = create_shader_module(vert_code);
	m_frag_module = create_shader_module(frag_code);

	std::vector<vk::DescriptorSetLayoutBinding> reflected_bindings;
	auto reflect_from = [&](const std::vector<char>& spirv_code) {
		SpvReflectShaderModule module;
		const auto result = spvReflectCreateShaderModule(spirv_code.size(), spirv_code.data(), &module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection failed");

		uint32_t count = 0;
		spvReflectEnumerateDescriptorBindings(&module, &count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> reflect_bindings(count);
		spvReflectEnumerateDescriptorBindings(&module, &count, reflect_bindings.data());

		for (auto* b : reflect_bindings) {
			reflected_bindings.emplace_back(
				b->binding,
				static_cast<vk::DescriptorType>(b->descriptor_type),
				1,
				static_cast<vk::ShaderStageFlagBits>(module.shader_stage)
			);

			m_bindings[b->name] = reflected_bindings.back();
		}

		spvReflectDestroyShaderModule(&module);
		};

	reflect_from(vert_code);
	reflect_from(frag_code);

	auto reflect_uniforms = [&](const std::span<const char> spirv_code) -> std::vector<uniform_block> {
		std::vector<uniform_block> blocks;

		SpvReflectShaderModule module;
		const SpvReflectResult result = spvReflectCreateShaderModule(spirv_code.size(), spirv_code.data(), &module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection failed");

		uint32_t binding_count = 0;
		spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
		spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

		for (const auto* b : bindings) {
			if (b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				uniform_block block{
					.name = b->name,
					.binding = b->binding,
					.set = b->set,
					.size = b->block.size
				};

				for (uint32_t i = 0; i < b->block.member_count; ++i) {
					const SpvReflectBlockVariable& member = b->block.members[i];
					block.members.emplace_back(
						member.name,
						member.type_description->type_name ? member.type_description->type_name : "",
						member.offset,
						member.size,
						member.array.dims_count ? member.array.dims[0] : 0
					);
				}
				blocks.push_back(block);
			}
		}

		spvReflectDestroyShaderModule(&module);
		return blocks;
		};

	const auto vert_blocks = reflect_uniforms(vert_code);
	const auto frag_blocks = reflect_uniforms(frag_code);

	for (const auto& block : vert_blocks) {
		m_uniform_blocks[block.name] = block;
	}

	for (const auto& block : frag_blocks) {
		if (m_uniform_blocks.contains(block.name)) {
			const auto& existing = m_uniform_blocks[block.name];
			assert(
				existing.binding == block.binding && existing.set == block.set,
				std::format(
					"Uniform block '{}' has conflicting bindings: existing (set={}, binding={}) vs new (set={}, binding={})",
					block.name,
					existing.set,
					existing.binding,
					block.set,
					block.binding
				)
			);
		}
		else {
			m_uniform_blocks[block.name] = block;
		}
	}

	if (layout && !expected_bindings.empty()) {
		std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding> layout_map;
		for (auto& e : expected_bindings) {
			layout_map[e.binding] = e;
		}
		for (auto& b : reflected_bindings) {
			auto it = layout_map.find(b.binding);
			assert(it != layout_map.end(),
				std::format("Shader uses binding {} but layout did not include it", b.binding));

			const auto& a = it->second;
			assert(
				a.descriptorType == b.descriptorType
				&& (a.stageFlags & b.stageFlags) == b.stageFlags,
				std::format(
					"Descriptor mismatch at binding {}: shader({}/{}) vs layout({}/{})",
					b.binding,
					to_string(b.descriptorType), to_string(b.stageFlags),
					to_string(a.descriptorType), to_string(a.stageFlags)
				)
			);
		}

		m_shared_layout = layout;
		return; 
	}

	const vk::DescriptorSetLayoutCreateInfo layout_info{
		{},
		static_cast<uint32_t>(reflected_bindings.size()),
		reflected_bindings.data()
	};
	m_descriptor_set_layout = device.createDescriptorSetLayout(layout_info);
	m_custom_layout = true;
}

auto gse::shader::get_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2> {
	const std::array shader_stages{
		vk::PipelineShaderStageCreateInfo{
			{},
			vk::ShaderStageFlagBits::eVertex,
			m_vert_module,
			"main"
		},
		vk::PipelineShaderStageCreateInfo{
			{},
			vk::ShaderStageFlagBits::eFragment,
			m_frag_module,
			"main"
		}
	};
	return shader_stages;
}

auto gse::shader::get_required_bindings() const -> std::vector<std::string> {
	std::vector<std::string> required_bindings;
	for (const auto& [name, binding] : m_bindings) {
		if (binding.descriptorType == vk::DescriptorType::eStorageBuffer) {
			required_bindings.push_back(name);
		}
	}
	return required_bindings;
}

auto gse::shader::get_binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding> {
	if (const auto it = m_bindings.find(name); it != m_bindings.end()) {
		return it->second;
	}
	return std::nullopt;
}

auto gse::shader::get_descriptor_writes(
	vk::DescriptorSet set,
	const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
	const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos
) const -> std::vector<vk::WriteDescriptorSet> {
	std::vector<vk::WriteDescriptorSet> writes;
	std::unordered_set<std::string> used_keys;

	for (const auto& [name, binding] : m_bindings) {
		if (auto it = buffer_infos.find(name); it != buffer_infos.end()) {
			writes.emplace_back(
				set, binding.binding, 0, binding.descriptorCount, binding.descriptorType,
				nullptr, &it->second, nullptr
			);
			used_keys.insert(name);
		}
		else if (auto it2 = image_infos.find(name); it2 != image_infos.end()) {
			writes.emplace_back(
				set, binding.binding, 0, binding.descriptorCount, binding.descriptorType,
				&it2->second, nullptr, nullptr
			);
			used_keys.insert(name);
		}
		else {
			assert(false, std::format("Missing required binding: '{}'", name));
		}
	}

	const auto total_inputs = buffer_infos.size() + image_infos.size();
	assert(
		used_keys.size() == total_inputs,
		"Some descriptor inputs were not used. Possibly extra or misnamed keys?"
	);

	return writes;
}

template <typename T>
auto gse::shader::set_uniform(std::string_view full_name, const T& value, const vulkan::persistent_allocator::allocation& alloc) const -> void {
	const auto dot_pos = full_name.find('.');
	assert(dot_pos != std::string_view::npos, std::format("Uniform name '{}' must be in the format 'Block.member'", full_name));

	const auto block_name = std::string(full_name.substr(0, dot_pos));
	const auto member_name = std::string(full_name.substr(dot_pos + 1));

	const auto block_it = m_uniform_blocks.find(block_name);
	assert(block_it != m_uniform_blocks.end(), std::format("Uniform block '{}' not found", block_name));

	const auto& block = block_it->second;
	const auto member_it = std::find_if(block.members.begin(), block.members.end(), [&](const auto& member) {
		return member.name == member_name;
		});
	assert(member_it != block.members.end(), std::format("Member '{}' not found in block '{}'", member_name, block_name));

	const auto& member = *member_it;
	assert(sizeof(T) <= member.size, std::format("Value size {} exceeds member '{}' size {}", sizeof(T), member_name, member.size));
	assert(alloc.mapped(), std::format("Attempted to set uniform '{}.{}' but memory is not mapped", block_name, member_name));

	std::memcpy(static_cast<std::byte*>(alloc.mapped()) + member.offset, &value, sizeof(T));
}

template <typename T>
auto gse::shader::set_uniform_block(
	const std::string_view block_name,
	const std::unordered_map<std::string, std::span<const std::byte>>& data,
	const vulkan::persistent_allocator::allocation& alloc
) const -> void {
	const auto block_it = m_uniform_blocks.find(std::string(block_name));
	assert(block_it != m_uniform_blocks.end(), std::format("Uniform block '{}' not found", block_name));

	const auto& block = block_it->second;
	assert(alloc.mapped(), "Attempted to set uniform block but memory is not mapped");

	for (const auto& [name, bytes] : data) {
		const auto member_it = std::find_if(block.members.begin(), block.members.end(), [&](const auto& m) {
			return m.name == name;
			});

		assert(member_it != block.members.end(), std::format("Uniform member '{}' not found in block '{}'", name, block_name));
		assert(bytes.size() <= member_it->size, std::format("Data size {} > member size {} for '{}.{}'", bytes.size(), member_it->size, block_name, name));
		assert(alloc.mapped(), std::format("Memory not mapped for '{}.{}'", block_name, name));

		std::memcpy(static_cast<std::byte*>(alloc.mapped()) + member_it->offset, bytes.data(), bytes.size());
	}
}

auto gse::read_file(const std::filesystem::path& path) -> std::vector<char> {
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	assert(file.is_open(), "Failed to open file!");

	const size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	return buffer;
}
module;

#include <SPIRV-Reflect/spirv_reflect.h>

export module gse.graphics.shader;

import vulkan_hpp;
import std;

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
			vk::Device device,
			const std::filesystem::path& vert_path,
			const std::filesystem::path& frag_path, 
			const vk::DescriptorSetLayout* layout = nullptr, 
			std::span<vk::DescriptorSetLayoutBinding> bindings = {}
		);
		~shader();

		shader(const shader&) = delete;
		auto operator=(const shader&) -> shader & = delete;

		auto create(
			vk::Device device, 
			const std::filesystem::path& vert_path, 
			const std::filesystem::path& frag_path, 
			const vk::DescriptorSetLayout* layout = nullptr, 
			std::span<vk::DescriptorSetLayoutBinding> expected_bindings = {}
		) -> void;

		auto get_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
		auto get_uniform_blocks() const -> const std::unordered_map<std::string, uniform_block>& { return m_uniform_blocks; }
		auto get_descriptor_set_layout() const -> const vk::DescriptorSetLayout* { return &m_descriptor_set_layout; }
		auto get_required_bindings() const -> std::vector<std::string>;
		auto get_binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding>;

		auto get_descriptor_writes(
			vk::DescriptorSet set,
			const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
			const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos
		) const -> std::vector<vk::WriteDescriptorSet>;
	private:
		auto create_shader_module(const std::vector<char>& code) const -> vk::ShaderModule;

		vk::ShaderModule m_vert_module;
		vk::ShaderModule m_frag_module;

		vk::Device m_device;

		vk::DescriptorSetLayout m_descriptor_set_layout;
		std::unordered_map<std::string, vk::DescriptorSetLayoutBinding> m_bindings;
		std::unordered_map<std::string, uniform_block> m_uniform_blocks;
	};

	auto read_file(const std::filesystem::path& path) -> std::vector<char>;
}

gse::shader::shader(const vk::Device device, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const vk::DescriptorSetLayout* layout, const std::span<vk::DescriptorSetLayoutBinding> bindings) : m_device(device) {
	create(device, vert_path, frag_path, layout, bindings);
}

gse::shader::~shader() {
	m_device.destroyShaderModule(m_vert_module);
	m_device.destroyShaderModule(m_frag_module);
}

auto gse::shader::create(const vk::Device device, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const vk::DescriptorSetLayout* layout, std::span<vk::DescriptorSetLayoutBinding> expected_bindings) -> void {
	this->m_device = device;

	const auto vert_code = read_file(vert_path);
	const auto frag_code = read_file(frag_path);
	m_vert_module = create_shader_module(vert_code);
	m_frag_module = create_shader_module(frag_code);

	// Reflect bindings from both vertex and fragment
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

	// If a layout was passed in, validate its bindings
	if (layout && !expected_bindings.empty()) {
		std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding> layout_map;
		for (auto& e : expected_bindings) {
			layout_map[e.binding] = e;
		}

		for (auto& b : reflected_bindings) {
			auto it = layout_map.find(b.binding);
			assert(it != layout_map.end(), std::format("Shader uses binding {} but layout did not include it\n", b.binding));

			auto const& a = it->second;
			assert(
				a.descriptorType == b.descriptorType &&
				(a.stageFlags & b.stageFlags) == b.stageFlags,
				std::format(
					"Descriptor mismatch at binding {}: shader({}/{}) vs layout({}/{})\n",
					b.binding,
					to_string(b.descriptorType), to_string(b.stageFlags),
					to_string(a.descriptorType), to_string(a.stageFlags)
				)
			);
		}

		m_descriptor_set_layout = *layout;
		return;
	}

	// If no layout or expected bindings were passed, generate from reflection
	const vk::DescriptorSetLayoutCreateInfo layout_info{
		{},
		static_cast<uint32_t>(reflected_bindings.size()),
		reflected_bindings.data()
	};	

	m_descriptor_set_layout = m_device.createDescriptorSetLayout(layout_info);

	auto reflect_uniforms = [](const std::span<const char> spirv_code) -> std::vector<uniform_block> {
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
				uniform_block block {
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

	const auto vert_bindings = reflect_uniforms(vert_code);
	const auto frag_bindings = reflect_uniforms(frag_code);

	for (const auto& block : vert_bindings) {
		m_uniform_blocks[block.name] = block;
	}

	for (const auto& block : frag_bindings) {
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

auto gse::shader::create_shader_module(const std::vector<char>& code) const -> vk::ShaderModule {
	const vk::ShaderModuleCreateInfo create_info{
		{},
		code.size(),
		reinterpret_cast<const std::uint32_t*>(code.data())
	};

	return m_device.createShaderModule(create_info);
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
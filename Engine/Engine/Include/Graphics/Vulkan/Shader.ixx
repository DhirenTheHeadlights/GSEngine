module;

#include <SPIRV-Reflect/spirv_reflect.h>

export module gse.graphics.shader;

import vulkan_hpp;
import std;

import gse.platform;

namespace gse {
	export class shader {
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
		auto get_descriptor_set_layout() const -> const vk::DescriptorSetLayout* { return &m_descriptor_set_layout; }
		auto get_bindings() const -> const std::unordered_map<std::string, vk::DescriptorSetLayoutBinding>& { return m_bindings; }
		auto get_binding(const std::string& name) const -> std::optional< vk::DescriptorSetLayoutBinding>;
		auto get_required_bindings() const -> std::vector<std::string>;
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

			m_bindings.try_emplace(b->name, reflected_bindings.back());
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

auto gse::shader::create_shader_module(const std::vector<char>& code) const -> vk::ShaderModule {
	const vk::ShaderModuleCreateInfo create_info{
		{},
		code.size(),
		reinterpret_cast<const std::uint32_t*>(code.data())
	};

	return m_device.createShaderModule(create_info);
}

auto gse::shader::get_binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding> {
	assert(m_bindings.contains(name), std::format("Binding {} not found", name));
	return m_bindings.at(name);
}

auto gse::shader::get_required_bindings() const -> std::vector<std::string> {
	std::vector<std::string> required_bindings;
	required_bindings.reserve(m_bindings.size());
	for (const auto& [name, binding] : m_bindings) {
		if (binding.descriptorCount > 0) {
			required_bindings.push_back(name);
		}
	}
	return required_bindings;
}

auto gse::shader::get_descriptor_writes(vk::DescriptorSet set, const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos, const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos) const -> std::vector<vk::WriteDescriptorSet> {
	std::vector<vk::WriteDescriptorSet> writes;

	for (const auto& [name, binding] : m_bindings) {
		switch (binding.descriptorType) {
		case vk::DescriptorType::eUniformBuffer:
		case vk::DescriptorType::eUniformBufferDynamic:
		case vk::DescriptorType::eStorageBuffer:
		case vk::DescriptorType::eStorageBufferDynamic: {
			auto it = buffer_infos.find(name);
			if (it != buffer_infos.end()) {
				writes.emplace_back(
					set, binding.binding, 0, binding.descriptorCount, binding.descriptorType,
					nullptr, &it->second, nullptr
				);
			}
			break;
		}
		case vk::DescriptorType::eCombinedImageSampler:
		case vk::DescriptorType::eSampledImage:
		case vk::DescriptorType::eStorageImage:
		case vk::DescriptorType::eSampler: {
			auto it = image_infos.find(name);
			if (it != image_infos.end()) {
				writes.emplace_back(
					set, binding.binding, 0, binding.descriptorCount, binding.descriptorType,
					&it->second, nullptr, nullptr
				);
			}
			break;
		}
		default:
			break;
		}
	}

	return writes;
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
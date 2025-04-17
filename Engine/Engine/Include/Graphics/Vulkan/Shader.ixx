module;

#include <SPIRV-Reflect/spirv_reflect.h>

export module gse.graphics.shader;

import vulkan_hpp;
import std;

import gse.platform.context;
import gse.platform.assert;

namespace gse {
	export class shader {
	public:
		shader() = default;
		shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const vk::DescriptorSetLayout* layout = nullptr);
		~shader();

		shader(const shader&) = delete;
		auto operator=(const shader&) -> shader & = delete;

		auto create(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const vk::DescriptorSetLayout* layout = nullptr) -> void;
		auto get_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
		auto get_descriptor_set_layout() const -> vk::DescriptorSetLayout { return m_descriptor_set_layout; }
		auto get_descriptor_sets() const -> const std::vector<vk::DescriptorSet>& { return m_descriptor_sets; }
	private:
		static auto create_shader_module(const std::vector<char>& code) -> vk::ShaderModule;

		vk::ShaderModule m_vert_module;
		vk::ShaderModule m_frag_module;

		vk::DescriptorSetLayout m_descriptor_set_layout;
		int descriptor_layout_type = 0;
		std::vector<vk::DescriptorSet> m_descriptor_sets;
		//vk::PipelineLayout pipeline_layout;
	};

	auto read_file(const std::filesystem::path& path) -> std::vector<char>;
}

gse::shader::shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const vk::DescriptorSetLayout* layout) {
	create(vert_path, frag_path, layout);
}

gse::shader::~shader() {
	const auto device = vulkan::get_device_config().device;
	device.destroyShaderModule(m_vert_module);
	device.destroyShaderModule(m_frag_module);
}

auto gse::shader::create(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const vk::DescriptorSetLayout* layout) -> void {
	const auto vert_code = read_file(vert_path);
	const auto frag_code = read_file(frag_path);
	m_vert_module = create_shader_module(vert_code);
	m_frag_module = create_shader_module(frag_code);

	if (layout) {
		m_descriptor_set_layout = *layout;
		m_descriptor_sets = vulkan::get_device_config().device.allocateDescriptorSets({ vulkan::get_descriptor_config().descriptor_pool, 1, &m_descriptor_set_layout });
		//pipeline_layout = vulkan::get_device_config().device.createPipelineLayout({ {}, 1, gse::shader_loader::get_descriptor_layout(layout) });
		return;
	}

	std::vector<vk::DescriptorSetLayoutBinding> bindings;

	auto extract_bindings = [&](const std::vector<char>& spirv_code) {
		SpvReflectShaderModule reflect_module;
		const SpvReflectResult result = spvReflectCreateShaderModule(spirv_code.size(), spirv_code.data(), &reflect_module);
		perma_assert(result == SPV_REFLECT_RESULT_SUCCESS, "Failed to parse SPIR-V!");

		uint32_t binding_count = 0;
		spvReflectEnumerateDescriptorBindings(&reflect_module, &binding_count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> reflect_bindings(binding_count);
		spvReflectEnumerateDescriptorBindings(&reflect_module, &binding_count, reflect_bindings.data());

		for (const auto* binding : reflect_bindings) {
			vk::DescriptorSetLayoutBinding layout_binding(
				binding->binding,													// Binding index
				static_cast<vk::DescriptorType>(binding->descriptor_type),			// Descriptor type
				1,																	// Descriptor count
				static_cast<vk::ShaderStageFlagBits>(reflect_module.shader_stage)	// Shader stage
			);
			bindings.push_back(layout_binding);
		}

		spvReflectDestroyShaderModule(&reflect_module);
		};

	extract_bindings(vert_code);
	extract_bindings(frag_code);

	const vk::DescriptorSetLayoutCreateInfo layout_info(
		{},
		bindings.size(),
		bindings.data()
	);

	m_descriptor_set_layout = vulkan::get_device_config().device.createDescriptorSetLayout(layout_info);

	const auto descriptor_pool = vulkan::get_descriptor_config().descriptor_pool;
	const vk::DescriptorSetAllocateInfo alloc_info(
		descriptor_pool,
		1,
		&m_descriptor_set_layout
	);

	m_descriptor_sets = vulkan::get_device_config().device.allocateDescriptorSets(alloc_info);
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

auto gse::shader::create_shader_module(const std::vector<char>& code) -> vk::ShaderModule {
	const vk::ShaderModuleCreateInfo create_info{
		{},
		code.size(),
		reinterpret_cast<const std::uint32_t*>(code.data())
	};

	return vulkan::get_device_config().device.createShaderModule(create_info);
}

auto gse::read_file(const std::filesystem::path& path) -> std::vector<char> {
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	perma_assert(file.is_open(), "Failed to open file!");

	const size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	return buffer;
}
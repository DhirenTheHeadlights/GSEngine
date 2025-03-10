export module gse.graphics.shader;

import vulkan_hpp;
import std;

import gse.platform.context;
import gse.platform.assert;

namespace gse {
	export class shader {
	public:
		shader() = default;
		shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path);
		~shader();

		shader(const shader&) = delete;
		auto operator=(const shader&) -> shader & = delete;

		auto create(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> void;
		auto get_shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
	private:
		static auto create_shader_module(const std::vector<char>& code) -> vk::ShaderModule;

		vk::ShaderModule m_vert_module;
		vk::ShaderModule m_frag_module;
	};

	auto read_file(const std::filesystem::path& path) -> std::vector<char>;
}

gse::shader::shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) {
	create(vert_path, frag_path);
}

gse::shader::~shader() {
	const auto device = vulkan::get_device_config().device;
	device.destroyShaderModule(m_vert_module);
	device.destroyShaderModule(m_frag_module);
}

auto gse::shader::create(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> void {
	const auto vert_code = read_file(vert_path);
	const auto frag_code = read_file(frag_path);
	m_vert_module = create_shader_module(vert_code);
	m_frag_module = create_shader_module(frag_code);
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
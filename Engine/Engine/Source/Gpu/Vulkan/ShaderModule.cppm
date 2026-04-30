export module gse.gpu:vulkan_shader_module;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	class shader_module final : public non_copyable {
	public:
		shader_module() = default;

		~shader_module() override = default;

		shader_module(
			shader_module&&
		) noexcept = default;

		auto operator=(
			shader_module&&
		) noexcept -> shader_module& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			std::span<const std::uint32_t> spirv
		) -> shader_module;

		[[nodiscard]] auto handle(
			this const shader_module& self
		) -> gpu::handle<shader_module>;

		explicit operator bool(
		) const;

	private:
		explicit shader_module(
			vk::raii::ShaderModule&& module
		);

		vk::raii::ShaderModule m_module = nullptr;
	};
}

gse::vulkan::shader_module::shader_module(vk::raii::ShaderModule&& module)
	: m_module(std::move(module)) {}

auto gse::vulkan::shader_module::create(const device& dev, const std::span<const std::uint32_t> spirv) -> shader_module {
	const vk::ShaderModuleCreateInfo info{
		.codeSize = spirv.size_bytes(),
		.pCode = spirv.data(),
	};
	return shader_module(dev.raii_device().createShaderModule(info));
}

auto gse::vulkan::shader_module::handle(this const shader_module& self) -> gpu::handle<shader_module> {
	return std::bit_cast<gpu::handle<shader_module>>(*self.m_module);
}

gse::vulkan::shader_module::operator bool() const {
	return *m_module != nullptr;
}

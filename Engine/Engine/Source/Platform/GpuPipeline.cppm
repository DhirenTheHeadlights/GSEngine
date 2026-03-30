export module gse.platform:gpu_pipeline;

import std;
import vulkan;

import :gpu_types;

import gse.utility;

export namespace gse::gpu {
	class pipeline final : public non_copyable {
	public:
		pipeline() = default;
		pipeline(
			vk::raii::Pipeline&& handle,
			vk::raii::PipelineLayout&& layout,
			bind_point point = bind_point::graphics
		);
		pipeline(pipeline&&) noexcept = default;
		auto operator=(pipeline&&) noexcept -> pipeline& = default;

		[[nodiscard]] auto native_pipeline(this const pipeline& self) -> vk::Pipeline { return *self.m_handle; }
		[[nodiscard]] auto native_layout(this const pipeline& self) -> vk::PipelineLayout { return *self.m_layout; }
		[[nodiscard]] auto point(this const pipeline& self) -> bind_point { return self.m_point; }

		explicit operator bool() const;
	private:
		vk::raii::Pipeline m_handle = nullptr;
		vk::raii::PipelineLayout m_layout = nullptr;
		bind_point m_point = bind_point::graphics;
	};

	class descriptor_set final : public non_copyable {
	public:
		descriptor_set() = default;
		descriptor_set(vk::raii::DescriptorSet&& set);
		descriptor_set(descriptor_set&&) noexcept = default;
		auto operator=(descriptor_set&&) noexcept -> descriptor_set& = default;

		[[nodiscard]] auto native(this const descriptor_set& self) -> vk::DescriptorSet { return *self.m_set; }

		explicit operator bool() const;
	private:
		vk::raii::DescriptorSet m_set = nullptr;
	};

	class sampler final : public non_copyable {
	public:
		sampler() = default;
		sampler(vk::raii::Sampler&& s);
		sampler(sampler&&) noexcept = default;
		auto operator=(sampler&&) noexcept -> sampler& = default;

		[[nodiscard]] auto native(this const sampler& self) -> vk::Sampler { return *self.m_sampler; }

		explicit operator bool() const;
	private:
		vk::raii::Sampler m_sampler = nullptr;
	};
}

gse::gpu::pipeline::pipeline(
	vk::raii::Pipeline&& handle,
	vk::raii::PipelineLayout&& layout,
	bind_point point
) : m_handle(std::move(handle)),
    m_layout(std::move(layout)),
    m_point(point) {}

gse::gpu::pipeline::operator bool() const {
	return *m_handle != nullptr;
}

gse::gpu::descriptor_set::descriptor_set(vk::raii::DescriptorSet&& set)
	: m_set(std::move(set)) {}

gse::gpu::descriptor_set::operator bool() const {
	return *m_set != nullptr;
}

gse::gpu::sampler::sampler(vk::raii::Sampler&& s)
	: m_sampler(std::move(s)) {}

gse::gpu::sampler::operator bool() const {
	return *m_sampler != nullptr;
}

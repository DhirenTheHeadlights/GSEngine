export module gse.gpu:vulkan_descriptor_set_layout;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	class descriptor_set_layout final : public non_copyable {
	public:
		descriptor_set_layout() = default;

		~descriptor_set_layout() override = default;

		descriptor_set_layout(
			descriptor_set_layout&&
		) noexcept = default;

		auto operator=(
			descriptor_set_layout&&
		) noexcept -> descriptor_set_layout& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			std::span<const gpu::descriptor_binding_desc> bindings
		) -> descriptor_set_layout;

		[[nodiscard]] auto handle(
			this const descriptor_set_layout& self
		) -> gpu::handle<descriptor_set_layout>;

		explicit operator bool(
		) const;

	private:
		explicit descriptor_set_layout(
			vk::raii::DescriptorSetLayout&& layout
		);

		vk::raii::DescriptorSetLayout m_layout = nullptr;
	};
}

gse::vulkan::descriptor_set_layout::descriptor_set_layout(vk::raii::DescriptorSetLayout&& layout)
	: m_layout(std::move(layout)) {}

auto gse::vulkan::descriptor_set_layout::create(const device& dev, const std::span<const gpu::descriptor_binding_desc> bindings) -> descriptor_set_layout {
	std::vector<vk::DescriptorSetLayoutBinding> raw;
	raw.reserve(bindings.size());
	for (const auto& b : bindings) {
		raw.push_back({
			.binding = b.binding,
			.descriptorType = to_vk(b.type),
			.descriptorCount = b.count,
			.stageFlags = to_vk(b.stages),
			.pImmutableSamplers = nullptr,
		});
	}

	const vk::DescriptorSetLayoutCreateInfo info{
		.flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
		.bindingCount = static_cast<std::uint32_t>(raw.size()),
		.pBindings = raw.data(),
	};

	return descriptor_set_layout(dev.raii_device().createDescriptorSetLayout(info));
}

auto gse::vulkan::descriptor_set_layout::handle(this const descriptor_set_layout& self) -> gpu::handle<descriptor_set_layout> {
	return std::bit_cast<gpu::handle<descriptor_set_layout>>(*self.m_layout);
}

gse::vulkan::descriptor_set_layout::operator bool() const {
	return *m_layout != nullptr;
}

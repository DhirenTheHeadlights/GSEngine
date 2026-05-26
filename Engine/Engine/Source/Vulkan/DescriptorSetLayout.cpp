module gse.vulkan;

import std;
import vulkan;

gse::vulkan::descriptor_set_layout::descriptor_set_layout(vk::raii::DescriptorSetLayout&& layout)
	: m_layout(std::move(layout)) {
}

auto gse::vulkan::descriptor_set_layout::create(const device& dev, const std::span<const gpu::descriptor_binding_desc> bindings) -> descriptor_set_layout {
	std::vector<vk::DescriptorSetLayoutBinding> raw;
	raw.reserve(bindings.size());
	std::vector<vk::DescriptorBindingFlags> binding_flags;
	binding_flags.reserve(bindings.size());
	for (const auto& b : bindings) {
		raw.push_back({
			.binding = b.binding,
			.descriptorType = to_vk(b.type),
			.descriptorCount = b.count,
			.stageFlags = to_vk(b.stages),
			.pImmutableSamplers = nullptr,
		});
		binding_flags.push_back(b.count > 1 ? vk::DescriptorBindingFlagBits::ePartiallyBound : vk::DescriptorBindingFlags{});
	}

	const vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info{
		.bindingCount = static_cast<std::uint32_t>(binding_flags.size()),
		.pBindingFlags = binding_flags.data(),
	};

	const vk::DescriptorSetLayoutCreateInfo info{
		.pNext = &flags_info,
		.bindingCount = static_cast<std::uint32_t>(raw.size()),
		.pBindings = raw.data(),
	};

	return descriptor_set_layout(dev.raii_device().createDescriptorSetLayout(info));
}

auto gse::vulkan::descriptor_set_layout::handle(this const descriptor_set_layout& self) -> gpu::handle<descriptor_set_layout> {
	return std::bit_cast<gpu::handle<descriptor_set_layout>>(*self.m_layout);
}

auto gse::vulkan::descriptor_set_layout::valid() const -> bool {
	return *m_layout != nullptr;
}

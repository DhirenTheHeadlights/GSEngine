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

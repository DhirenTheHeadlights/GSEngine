export module gse.gpu:vulkan_sampler;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	class sampler final : public non_copyable {
	public:
		sampler() = default;

		~sampler() override = default;

		sampler(
			sampler&&
		) noexcept = default;

		auto operator=(
			sampler&&
		) noexcept -> sampler& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			const gpu::sampler_desc& desc
		) -> sampler;

		[[nodiscard]] auto native(
			this const sampler& self
		) -> gpu::handle<sampler>;

		explicit operator bool(
		) const;

	private:
		explicit sampler(
			vk::raii::Sampler&& sampler
		);

		vk::raii::Sampler m_sampler = nullptr;
	};
}

gse::vulkan::sampler::sampler(vk::raii::Sampler&& sampler) : m_sampler(std::move(sampler)) {}

auto gse::vulkan::sampler::create(const device& dev, const gpu::sampler_desc& desc) -> sampler {
	const vk::SamplerCreateInfo info{
		.magFilter = to_vk(desc.mag),
		.minFilter = to_vk(desc.min),
		.mipmapMode = desc.min == gpu::sampler_filter::nearest ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear,
		.addressModeU = to_vk(desc.address_u),
		.addressModeV = to_vk(desc.address_v),
		.addressModeW = to_vk(desc.address_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = desc.max_anisotropy > 0.0f ? vk::True : vk::False,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.compare_enable ? vk::True : vk::False,
		.compareOp = to_vk(desc.compare),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = to_vk(desc.border),
		.unnormalizedCoordinates = vk::False
	};
	return sampler(dev.raii_device().createSampler(info));
}

auto gse::vulkan::sampler::native(this const sampler& self) -> gpu::handle<sampler> {
	return std::bit_cast<gpu::handle<sampler>>(*self.m_sampler);
}

gse::vulkan::sampler::operator bool() const {
	return *m_sampler != nullptr;
}

export module gse.graphics:texture;

import std;

import :rendering_context;

import gse.utility;
import gse.physics.math;
import gse.platform;

export namespace gse {
	class texture : public identifiable {
	public:
		enum struct profile : std::uint8_t {
			generic_repeat,
			generic_clamp_to_edge,
			msdf,
			pixel_art
		};

		texture(
			const std::filesystem::path& filepath
		);

		texture(
			std::string_view name, 
			const unitless::vec4& color, 
			unitless::vec2u size = { 1, 1 }
		);

		texture(
			std::string_view name,
			const std::vector<std::byte>& data,
			unitless::vec2u size,
			std::uint32_t channels,
			profile texture_profile = profile::generic_repeat
		);

		auto load(
			const renderer::context& context
		) -> void;

		auto unload(
		) -> void;

		auto descriptor_info(
		) const -> vk::DescriptorImageInfo;

		auto image_data(
		) const -> const image::data&;
	private:
		auto create_vulkan_resources(
			renderer::context& context,
			profile texture_profile
		) -> void;

		vulkan::image_resource m_texture_image;
		vk::raii::Sampler m_texture_sampler = nullptr;
		image::data m_image_data;
		profile m_profile = profile::generic_repeat;
	};
}

gse::texture::texture(const std::filesystem::path& filepath) : identifiable(filepath), m_image_data{ .path = filepath } {}

gse::texture::texture(const std::string_view name, const unitless::vec4& color, const unitless::vec2u size) : identifiable(name), m_image_data(image::load(color, size)) {}

gse::texture::texture(const std::string_view name, const std::vector<std::byte>& data, const unitless::vec2u size, const std::uint32_t channels, const profile texture_profile) : identifiable(name), m_image_data(image::data{ .path = {}, .size = size, .channels = channels, .pixels = data }), m_profile(texture_profile) {}

auto gse::texture::load(const renderer::context& context) -> void {
	if (!m_image_data.path.empty()) {
		std::ifstream in_file(m_image_data.path, std::ios::binary);
		assert(
			in_file.is_open(), 
			std::source_location::current(),
			"Failed to open baked texture file: {}",
			m_image_data.path.string()
		);

		std::uint32_t magic, version;
		in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		in_file.read(reinterpret_cast<char*>(&version), sizeof(version));
		assert(magic == 0x47544558 && version == 1, std::source_location::current(), "Invalid baked texture file format or version.");

		std::uint32_t width, height, channels;
		profile texture_profile;
		in_file.read(reinterpret_cast<char*>(&width), sizeof(width));
		in_file.read(reinterpret_cast<char*>(&height), sizeof(height));
		in_file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
		in_file.read(reinterpret_cast<char*>(&texture_profile), sizeof(texture_profile));

		std::uint64_t data_size;
		in_file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
		m_image_data.pixels.resize(data_size);
		in_file.read(reinterpret_cast<char*>(m_image_data.pixels.data()), data_size);

		m_image_data.size = { width, height };
		m_image_data.channels = channels;
	}

	context.queue_gpu_command<texture>(this, [](renderer::context& ctx, texture& self) {
		self.create_vulkan_resources(ctx, self.m_profile);
	});
}

auto gse::texture::unload() -> void {
	m_image_data = {};
	m_texture_image = {};
	m_texture_sampler = nullptr;
}

auto gse::texture::descriptor_info() const -> vk::DescriptorImageInfo {
	return {
		.sampler = m_texture_sampler,
		.imageView = m_texture_image.view,
		.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
	};
}

auto gse::texture::image_data() const -> const image::data& {
	return m_image_data;
}

auto gse::texture::create_vulkan_resources(renderer::context& context, const profile texture_profile) -> void {
	auto& config = context.config();

	const auto width = m_image_data.size.x();
	const auto height = m_image_data.size.y();
	const auto channels = m_image_data.channels;
	const auto data_size = m_image_data.size_bytes();

	assert(
		data_size > 0 && !m_image_data.pixels.empty(),
		std::source_location::current(),
		"Texture '{}' has no pixel data. Ensure the texture is loaded correctly.",
		id()
	);


	// MSDF textures need linear format (Unorm) for correct distance calculations
	// Regular textures use sRGB for proper gamma handling
	const bool use_linear = (texture_profile == profile::msdf);
	const auto format = channels == 4
		? (use_linear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb)
		: channels == 1
			? vk::Format::eR8Unorm
			: (use_linear ? vk::Format::eR8G8B8Unorm : vk::Format::eR8G8B8Srgb);

	m_texture_image = config.allocator().create_image(
		vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {width, height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		vk::ImageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		}
	);

	vulkan::uploader::upload_image_2d(
		config,
		m_texture_image,
		width, height,
		m_image_data.pixels.data(),
		data_size,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	vk::SamplerCreateInfo sampler_info;
	sampler_info.maxLod = 1.0f;

	switch (texture_profile) {
	case profile::generic_repeat:
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
		sampler_info.anisotropyEnable = vk::True;
		sampler_info.maxAnisotropy = 16.0f;
		break;
	case profile::generic_clamp_to_edge:
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		break;
	case profile::msdf:
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		break;
	case profile::pixel_art:
		sampler_info.magFilter = vk::Filter::eNearest;
		sampler_info.minFilter = vk::Filter::eNearest;
		sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		break;
	}
	m_texture_sampler = config.device_config().device.createSampler(sampler_info);

	m_image_data.pixels.clear();
	m_image_data.pixels.shrink_to_fit();
}

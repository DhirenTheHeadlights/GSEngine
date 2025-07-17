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

        struct handle {
            vk::DescriptorImageInfo descriptor_info;
			const image::data* image_data = nullptr;

            handle(const texture& texture)
        	: descriptor_info{
                .sampler = texture.m_texture_sampler,
                .imageView = texture.m_texture_image.view,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			},
        	image_data{
        		&texture.m_image_data
        	} {}
        };

        texture(const std::filesystem::path& filepath) : identifiable(filepath.stem().string()), m_image_data{ .path = filepath } {}
        texture(const unitless::vec4& color, unitless::vec2u size = { 1, 1 });
        texture(
            const vulkan::config& config,
            const std::vector<std::byte>& data,
            unitless::vec2u size,
            std::uint32_t channels,
            profile texture_profile = profile::generic_repeat,
            const std::string& name = std::string("Texture from Data")
        );

        auto load(const renderer::context& context) -> void;
		auto unload() -> void;

        auto descriptor_info() const -> vk::DescriptorImageInfo {
            return {
                .sampler = m_texture_sampler,
                .imageView = m_texture_image.view,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            };
		}
    private:
		vulkan::persistent_allocator::image_resource m_texture_image;
		vk::raii::Sampler m_texture_sampler = nullptr;
        image::data m_image_data;
    };
}

gse::texture::texture(const unitless::vec4& color, const unitless::vec2u size)
    : identifiable(std::format("Solid Color ({}, {}, {}, {})", color.x, color.y, color.z, color.w)) {
    std::array<std::byte, 4> pixel_data;
    pixel_data[0] = static_cast<std::byte>(color.x * 255.0f);
    pixel_data[1] = static_cast<std::byte>(color.y * 255.0f);
    pixel_data[2] = static_cast<std::byte>(color.z * 255.0f);
    pixel_data[3] = static_cast<std::byte>(color.w * 255.0f);

    const std::size_t total_pixels = static_cast<std::size_t>(size.x) * size.y;
    std::vector<std::byte> pixels(total_pixels * 4);

    for (std::size_t i = 0; i < total_pixels; ++i) {
        std::memcpy(pixels.data() + i * 4, pixel_data.data(), 4);
    }

    m_image_data = image::data{
        .size = size,
        .channels = 4,
        .pixels = std::move(pixels)
    };
}

gse::texture::texture(const vulkan::config& config, const std::vector<std::byte>& data, unitless::vec2u size, std::uint32_t channels, profile texture_profile, const std::string& name) 
    : identifiable(name)
    , m_image_data(image::data{ 
        .path = name, 
        .size = size, 
        .channels = channels, 
        .pixels = data 
      }) {
    const auto format = channels == 4 ? vk::Format::eR8G8B8A8Unorm : channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Unorm;
    const std::size_t image_size = static_cast<std::size_t>(size.x) * size.y * channels;

    m_texture_image = vulkan::persistent_allocator::create_image(
        config.device_data,
        vk::ImageCreateInfo{
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {
                .width = size.x,
                .height = size.y,
                .depth = 1
            },
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
            .flags = {},
            .image = nullptr,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .components = {},
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
        size.x,
        size.y,
        data.data(),
        image_size,
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    vk::SamplerCreateInfo sampler_info{};
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

    m_texture_sampler = config.device_data.device.createSampler(sampler_info);
}

auto gse::texture::load(const renderer::context& context) -> void {
    const auto& config = context.config();

    if (!m_image_data.path.empty()) {
        m_image_data = image::load(m_image_data.path);
    }

    const vk::BufferCreateInfo buffer_info{
        .size = m_image_data.size_bytes(),
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive
    };

    auto [buffer, allocation] = vulkan::persistent_allocator::create_buffer(
        config.device_data,
        buffer_info,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        m_image_data.pixels.data()
    );

    const auto format = m_image_data.channels == 4 ? vk::Format::eR8G8B8A8Srgb : m_image_data.channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb;

    const vk::ImageCreateInfo image_info{
        .flags = {},
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {
            .width = m_image_data.size.x,
            .height = m_image_data.size.y,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = vk::ImageLayout::eUndefined
    };

    const vk::ImageViewCreateInfo view_info{
        .flags = {},
        .image = nullptr,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = {},
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    m_texture_image = vulkan::persistent_allocator::create_image(config.device_data, image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info);

    vulkan::uploader::upload_image_2d(
        config,
        m_texture_image,
        m_image_data.size.x,
        m_image_data.size.y,
        m_image_data.pixels.data(),
        m_image_data.size_bytes(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    constexpr vk::SamplerCreateInfo sampler_info{
        .flags = {},
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = 16.0f,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False
    };

    m_texture_sampler = config.device_data.device.createSampler(sampler_info);
}

auto gse::texture::unload() -> void {
    m_image_data = {};
	m_texture_image = {};
	m_texture_sampler = nullptr;
}
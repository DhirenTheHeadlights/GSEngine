export module gse.graphics:cube_map;

import std;

import gse.physics.math;
import gse.platform;

export namespace gse {
    class cube_map {
    public:
        auto create(vulkan::config& config, const std::array<std::filesystem::path, 6>& face_paths) -> void;
        auto create(const vulkan::config& config, int resolution, bool depth_only = false) -> void;

        auto image_resource() const -> const vulkan::persistent_allocator::image_resource&;
    private:
        vulkan::persistent_allocator::image_resource m_image_resource;
        vk::raii::Sampler m_sampler = nullptr;

        int m_resolution = 0;
        bool m_depth_only = false;
        bool m_initialized = false;
    };
}

auto gse::cube_map::create(vulkan::config& config, const std::array<std::filesystem::path, 6>& face_paths) -> void {
    const auto faces = image::load_cube_faces(face_paths);

    const vk::ImageCreateInfo image_info{
        .flags = vk::ImageCreateFlagBits::eCubeCompatible,
        .imageType = vk::ImageType::e2D,
        .format = vk::Format::eR8G8B8A8Srgb,
        .extent = {
            .width = faces[0].size.x,
            .height = faces[0].size.y,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 6,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined
    };

    constexpr vk::ImageViewCreateInfo view_info{
        .flags = {},
        .image = nullptr,
        .viewType = vk::ImageViewType::eCube,
        .format = vk::Format::eR8G8B8A8Srgb,
        .components = {},
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6
        }
    };

    m_image_resource = vulkan::persistent_allocator::create_image(
        config.device_config(), image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info
    );

    vulkan::uploader::upload_image_layers(
        config,
        m_image_resource,
        faces[0].size.x,
        faces[0].size.y,
        reinterpret_cast<const std::vector<const void*>&>(faces),
        faces[0].size_bytes(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    constexpr vk::SamplerCreateInfo sampler_info{
        .flags = {},
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::False,
        .maxAnisotropy = 1.0f,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False
    };
     
    m_sampler = config.device_config().device.createSampler(sampler_info);
    m_initialized = true;
}

auto gse::cube_map::create(const vulkan::config& config, const int resolution, const bool depth_only) -> void {
    m_resolution = resolution;
    m_depth_only = depth_only;

    const vk::Format format = depth_only ? vk::Format::eD32Sfloat : vk::Format::eR16G16B16A16Sfloat;
    const vk::ImageUsageFlags usage = depth_only
        ? (vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        : (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

    const vk::ImageCreateInfo image_info{
        .flags = vk::ImageCreateFlagBits::eCubeCompatible,
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {
            .width = static_cast<std::uint32_t>(resolution),
            .height = static_cast<std::uint32_t>(resolution),
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 6,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined
    };

    const vk::ImageAspectFlags aspect = depth_only ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

    const vk::ImageViewCreateInfo view_info{
        .flags = {},
        .image = nullptr,
        .viewType = vk::ImageViewType::eCube,
        .format = format,
        .components = {},
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6
        }
    };

    m_image_resource = vulkan::persistent_allocator::create_image(
        config.device_config(), image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info
    );

    constexpr vk::SamplerCreateInfo sampler_info{
        .flags = {},
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::False,
        .maxAnisotropy = 1.0f,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False
    };

    m_sampler = config.device_config().device.createSampler(sampler_info);
    m_initialized = true;
}

auto gse::cube_map::image_resource() const -> const vulkan::persistent_allocator::image_resource& {
    return m_image_resource;
}
module gse.graphics.cube_map;

import std;
import vulkan_hpp;

auto gse::cube_map::get_image_resource() const -> const vulkan::persistent_allocator::image_resource& {
    return m_image_resource;
}

gse::cube_map::~cube_map() {
    assert(!m_initialized, "cube_map::~cube_map(): GPU resources were not destroyed. Call destroy(config).");
}

auto gse::cube_map::destroy(const vulkan::config& config) -> void {
    if (!m_initialized) return;

    config.device_data.device.destroySampler(m_sampler);
    free(config.device_data, m_image_resource);

    m_sampler = nullptr;
    m_image_resource = {};
    m_initialized = false;
}

auto gse::cube_map::create(vulkan::config& config, const std::array<std::filesystem::path, 6>& face_paths) -> void {
    auto faces = stb::image::load_cube_faces(face_paths);

    const vk::ImageCreateInfo image_info(
        vk::ImageCreateFlagBits::eCubeCompatible,
        vk::ImageType::e2D,
        vk::Format::eR8G8B8A8Srgb,
        { faces[0].size.x, faces[0].size.y, 1 },
        1, 6,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive,
        {}, vk::ImageLayout::eUndefined
    );

    constexpr vk::ImageViewCreateInfo view_info(
        {}, nullptr,
        vk::ImageViewType::eCube,
        vk::Format::eR8G8B8A8Srgb,
        {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 }
    );

    m_image_resource = vulkan::persistent_allocator::create_image(
        config.device_data, image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info
    );

    vulkan::uploader::upload_image_layers(
        config,
        m_image_resource.image,
        vk::Format::eR8G8B8A8Srgb,
        faces[0].size.x,
        faces[0].size.y,
        reinterpret_cast<const std::vector<const void*>&>(faces),
        faces[0].size_bytes(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    constexpr vk::SamplerCreateInfo sampler_info(
        {}, vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.0f, vk::False, 1.0f, vk::False,
        vk::CompareOp::eAlways,
        0.0f, 0.0f,
        vk::BorderColor::eIntOpaqueBlack,
        vk::False
    );

    m_sampler = config.device_data.device.createSampler(sampler_info);
    m_initialized = true;
}

auto gse::cube_map::create(const vulkan::config& config, const int resolution, const bool depth_only) -> void {
    m_resolution = resolution;
    m_depth_only = depth_only;

    const vk::Format format = depth_only ? vk::Format::eD32Sfloat : vk::Format::eR16G16B16A16Sfloat;
    const vk::ImageUsageFlags usage = depth_only
        ? (vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        : (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

    const vk::ImageCreateInfo image_info(
        vk::ImageCreateFlagBits::eCubeCompatible,
        vk::ImageType::e2D,
        format,
        { static_cast<std::uint32_t>(resolution), static_cast<std::uint32_t>(resolution), 1 },
        1, 6,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        usage, vk::SharingMode::eExclusive,
        {}, vk::ImageLayout::eUndefined
    );

    const vk::ImageViewCreateInfo view_info(
        {}, nullptr,
        vk::ImageViewType::eCube,
        format,
        {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 }
    );

    m_image_resource = vulkan::persistent_allocator::create_image(
        config.device_data, image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info
    );

    constexpr vk::SamplerCreateInfo sampler_info(
        {}, vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.0f, vk::False, 1.0f, vk::False,
        vk::CompareOp::eAlways,
        0.0f, 0.0f,
        vk::BorderColor::eIntOpaqueBlack,
        vk::False
    );

    m_sampler = config.device_data.device.createSampler(sampler_info);
    m_initialized = true;
}

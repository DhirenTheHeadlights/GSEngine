module;

#include <vulkan/vulkan.hpp>
#include <stb_image.h>

export module gse.graphics.cube_map;

import std;
import gse.physics.math;
import gse.platform.vulkan.context;

export namespace gse {
    class cube_map {
    public:
        cube_map() = default;
        ~cube_map();

        auto create(const std::vector<std::string>& faces) -> void;
        auto create(int resolution, bool depth_only = false) -> void;

        auto get_image() const -> vk::Image { return m_image; }
        auto get_image_view() const -> vk::ImageView { return m_image_view; }
        auto get_frame_buffer() const -> vk::Framebuffer { return m_frame_buffer; }
    private:
        vk::Image m_image;
        vk::DeviceMemory m_image_memory;
        vk::ImageView m_image_view;
        vk::Sampler m_sampler;
        vk::Framebuffer m_frame_buffer;
        int m_resolution;
        bool m_depth_only;
    };
}

gse::cube_map::~cube_map() {
    auto device = gse::vulkan::get_device();
    device.destroyFramebuffer(m_frame_buffer);
    device.destroyImageView(m_image_view);
    device.destroyImage(m_image);
    device.freeMemory(m_image_memory);
    device.destroySampler(m_sampler);
}

auto gse::cube_map::create(const std::vector<std::string>& faces) -> void {
    auto device = gse::vulkan::get_device();

    // Load textures
    int width, height, nr_channels;
    std::vector<unsigned char*> face_data;
    for (const auto& face : faces) {
        unsigned char* data = stbi_load(face.c_str(), &width, &height, &nr_channels, STBI_rgb_alpha);
        if (!data) {
            throw std::runtime_error("Failed to load cube map texture: " + face);
        }
        face_data.push_back(data);
    }

    // Create Vulkan image
    vk::ImageCreateInfo image_info({}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb,
        { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 }, 1, 6, vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive, {}, vk::ImageLayout::eUndefined);
    image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;

    m_image = device.createImage(image_info);

    // Allocate memory
    vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(m_image);
    vk::MemoryAllocateInfo alloc_info(mem_requirements.size, gse::vulkan::find_memory_type(mem_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    m_image_memory = device.allocateMemory(alloc_info);
    device.bindImageMemory(m_image, m_image_memory, 0);

    // Create image view
    vk::ImageViewCreateInfo view_info({}, m_image, vk::ImageViewType::eCube, vk::Format::eR8G8B8A8Srgb,
        {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 });
    m_image_view = device.createImageView(view_info);

    // Cleanup
    for (auto* data : face_data) {
        stbi_image_free(data);
    }
}

auto gse::cube_map::create(const int resolution, const bool depth_only) -> void {
    this->m_resolution = resolution;
    this->m_depth_only = depth_only;

    auto device = gse::vulkan::get_device();

    vk::Format format = depth_only ? vk::Format::eD32Sfloat : vk::Format::eR16G16B16A16Sfloat;
    vk::ImageUsageFlags usage = depth_only ? vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
        : vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

    vk::ImageCreateInfo image_info({}, vk::ImageType::e2D, format,
        { static_cast<uint32_t>(resolution), static_cast<uint32_t>(resolution), 1 }, 1, 6, vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal, usage, vk::SharingMode::eExclusive, {}, vk::ImageLayout::eUndefined);
    image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;

    m_image = device.createImage(image_info);

    vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(m_image);
    vk::MemoryAllocateInfo alloc_info(mem_requirements.size, gse::vulkan::find_memory_type(mem_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    m_image_memory = device.allocateMemory(alloc_info);
    device.bindImageMemory(m_image, m_image_memory, 0);

    vk::ImageViewCreateInfo view_info({}, m_image, vk::ImageViewType::eCube, format, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 });
    m_image_view = device.createImageView(view_info);
}

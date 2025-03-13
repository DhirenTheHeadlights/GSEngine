module;

#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

module gse.graphics.texture;

import std;
import vulkan_hpp;

gse::texture::texture(const std::filesystem::path& filepath) : identifiable(filepath.string()), m_filepath(filepath) {
    load_from_file(filepath);
}

gse::texture::~texture() {
    const auto device = vulkan::get_device_config().device;
    device.destroySampler(m_texture_sampler);
    device.destroyImageView(m_texture_image_view);
    device.destroyImage(m_texture_image);
    device.freeMemory(m_texture_image_memory);
}

auto gse::texture::load_from_file(const std::filesystem::path& filepath) -> void {
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filepath.string().c_str(), &m_size.x, &m_size.y, &m_channels, 0);
    perma_assert(data, "Failed to load texture image.");

    create_texture_image(data, m_size.x, m_size.y, m_channels);
    stbi_image_free(data);

    create_texture_image_view();
    create_texture_sampler();
}

auto gse::texture::load_from_memory(const std::span<std::uint8_t>& data, const int width, const int height, const int channels) -> void {
    m_size = { width, height };
    m_channels = channels;

    create_texture_image(data.data(), width, height, channels);
    create_texture_image_view();
    create_texture_sampler();
}

auto gse::texture::create_texture_image(const unsigned char* data, const int width, const int height, const int channels) -> void {
    const auto device = vulkan::get_device_config().device;
    const vk::DeviceSize image_size = width * height * channels;

    const vk::BufferCreateInfo buffer_info(
        {},
        image_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive
    );
    const vk::Buffer staging_buffer = device.createBuffer(buffer_info);

    vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(staging_buffer);
    vk::MemoryAllocateInfo alloc_info(
        mem_requirements.size,
        vulkan::find_memory_type(
            mem_requirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        )
    );

    const vk::DeviceMemory staging_buffer_memory = device.allocateMemory(alloc_info);
    device.bindBufferMemory(staging_buffer, staging_buffer_memory, 0);

    void* mapped_data = device.mapMemory(staging_buffer_memory, 0, image_size);
    std::memcpy(mapped_data, data, image_size);
    device.unmapMemory(staging_buffer_memory);

    const vk::Format format = channels == 4 ? vk::Format::eR8G8B8A8Srgb : channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb; // Note: 3-channel formats might require special handling

    const vk::ImageCreateInfo image_info(
        {},
        vk::ImageType::e2D,
        format,
        vk::Extent3D{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1 },
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        0, nullptr,
        vk::ImageLayout::eUndefined
    );
    m_texture_image = device.createImage(image_info);

    mem_requirements = device.getImageMemoryRequirements(m_texture_image);
    alloc_info = vk::MemoryAllocateInfo(
        mem_requirements.size,
        vulkan::find_memory_type(
            mem_requirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        )
    );
    m_texture_image_memory = device.allocateMemory(alloc_info);
    device.bindImageMemory(m_texture_image, m_texture_image_memory, 0);

    const vk::CommandBuffer command_buffer = vulkan::begin_single_line_commands();
    vk::ImageMemoryBarrier barrier(
        {},
        vk::AccessFlagBits::eTransferWrite,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
        m_texture_image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );
    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    const vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        vk::Offset3D{ 0, 0, 0 },
        vk::Extent3D{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1 }
    );
    command_buffer.copyBufferToImage(
        staging_buffer,
        m_texture_image,
        vk::ImageLayout::eTransferDstOptimal,
        1, &region
    );

    barrier = vk::ImageMemoryBarrier(
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
        m_texture_image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );
    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vulkan::end_single_line_commands(command_buffer);

    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);
}

auto gse::texture::create_texture_image_view() -> void {
    const vk::Format format = (m_channels == 4) ? vk::Format::eR8G8B8A8Srgb : m_channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb;
    const vk::ImageViewCreateInfo view_info(
        {},
        m_texture_image,
        vk::ImageViewType::e2D,
        format,
        {},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );
    m_texture_image_view = vulkan::get_device_config().device.createImageView(view_info);
}

auto gse::texture::create_texture_sampler() -> void {
    constexpr vk::SamplerCreateInfo sampler_info(
        {},
        vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.0f, vk::True, 16, vk::False, vk::CompareOp::eAlways,
        0.0f, 0.0f,
        vk::BorderColor::eIntOpaqueBlack
    );
    m_texture_sampler = vulkan::get_device_config().device.createSampler(sampler_info);
}

auto gse::texture::get_descriptor_info() const -> vk::DescriptorImageInfo {
    return vk::DescriptorImageInfo(m_texture_sampler, m_texture_image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
}



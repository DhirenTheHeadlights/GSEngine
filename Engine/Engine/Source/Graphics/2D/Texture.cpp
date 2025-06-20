module gse.graphics.texture;

import std;
import vulkan_hpp;

auto gse::texture::load(const vulkan::config& config) -> void {
    const vk::BufferCreateInfo buffer_info{
        .flags = {},
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

    const auto command_buffer = begin_single_line_commands(config);

    const vk::ImageMemoryBarrier to_transfer_barrier{
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = *m_texture_image.image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        nullptr,
        nullptr,
        to_transfer_barrier
    );

    const vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {.x = 0, .y = 0, .z = 0 },
        .imageExtent = {
            .width = m_image_data.size.x,
            .height = m_image_data.size.y,
            .depth = 1
        }
    };

    command_buffer.copyBufferToImage(
        *buffer,
        *m_texture_image.image,
        vk::ImageLayout::eTransferDstOptimal,
        region
    );

    const vk::ImageMemoryBarrier to_shader_read_barrier{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = *m_texture_image.image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        nullptr,
        nullptr,
        to_shader_read_barrier
    );

    end_single_line_commands(command_buffer, config);

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

auto gse::texture::load_from_memory(const vulkan::config& config, const std::vector<std::byte>& data, const unitless::vec2u size, const std::uint32_t channels) -> void {
    m_image_data = stb::image::data{
        .path = "",
        .size = size,
        .channels = channels,
        .pixels = data
    };

    const auto format = channels == 4 ? vk::Format::eR8G8B8A8Srgb : channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb;
    const std::size_t image_size = static_cast<std::size_t>(size.x) * size.y * channels;

    m_texture_image = vulkan::persistent_allocator::create_image(
        config.device_data,
        vk::ImageCreateInfo{
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {
                .width = static_cast<std::uint32_t>(size.x),
                .height = static_cast<std::uint32_t>(size.y),
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
        *m_texture_image.image,
        format,
        size.x,
        size.y,
        data.data(),
        image_size,
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
}

auto gse::texture::get_descriptor_info() const -> vk::DescriptorImageInfo {
    return vk::DescriptorImageInfo{
        .sampler = *m_texture_sampler,
        .imageView = *m_texture_image.view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };
}
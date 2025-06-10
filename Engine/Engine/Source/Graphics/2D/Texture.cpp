module gse.graphics.texture;

import std;
import vulkan_hpp;

auto gse::texture::load(const vulkan::config& config) -> void {
    const vk::BufferCreateInfo buffer_info(
        {},
        m_image_data.size_bytes(),
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive
    );

	auto staging_buffer = vulkan::persistent_allocator::create_buffer(config.device_data, buffer_info);

    const auto format = m_image_data.channels == 4 ? vk::Format::eR8G8B8A8Srgb : m_image_data.channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb; // Note: 3-channel formats might require special handling

    const vk::ImageCreateInfo image_info(
        {},
        vk::ImageType::e2D,
        format,
        vk::Extent3D{ (m_image_data.size.x), (m_image_data.size.y), 1 },
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        0, nullptr,
        vk::ImageLayout::eUndefined
    );

	const vk::ImageViewCreateInfo view_info(
        {},
        nullptr,
        vk::ImageViewType::e2D,
        format,
        {},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );

	m_texture_image = vulkan::persistent_allocator::create_image(config.device_data, image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info);

    const auto command_buffer = begin_single_line_commands(config);

    vk::ImageMemoryBarrier barrier(
        {},
        vk::AccessFlagBits::eTransferWrite,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
        m_texture_image.image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        nullptr,
        nullptr,
        barrier
    );

    const vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        vk::Offset3D{ 0, 0, 0 },
        vk::Extent3D{ (m_image_data.size.x), (m_image_data.size.y), 1 }
    );

    command_buffer.copyBufferToImage(
        staging_buffer.buffer,
        m_texture_image.image,
        vk::ImageLayout::eTransferDstOptimal,
        region
    );

    barrier = vk::ImageMemoryBarrier(
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
        m_texture_image.image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        nullptr,
        nullptr,
        barrier
    );

    end_single_line_commands(command_buffer, config);

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

    m_texture_sampler = config.device_data.device.createSampler(sampler_info);
}

auto gse::texture::load_from_memory(const vulkan::config& config, const std::vector<std::byte>& data, const unitless::vec2u size, const std::uint32_t channels) -> void {
    m_image_data = stb::image::data {
		.path = "",
		.size = size,
        .channels = channels,
        .pixels = data
    };

    const vk::Format format = channels == 4 ? vk::Format::eR8G8B8A8Srgb : channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb;
    const std::size_t image_size = static_cast<std::size_t>(size.x) * size.y * channels;

    m_texture_image = vulkan::persistent_allocator::create_image(
        config.device_data,
        vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D,
            format,
            vk::Extent3D{ static_cast<std::uint32_t>(size.x), static_cast<std::uint32_t>(size.y), 1 },
            1, 1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            vk::SharingMode::eExclusive,
            {},
            vk::ImageLayout::eUndefined
        ),
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::ImageViewCreateInfo(
            {}, nullptr,
            vk::ImageViewType::e2D,
            format,
            {},
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        )
    );

    vulkan::uploader::upload_image_2d(
		config,
        m_texture_image.image,
        format,
        size.x,
        size.y,
        data.data(),
        image_size,
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
}

auto gse::texture::get_descriptor_info() const -> vk::DescriptorImageInfo {
    return { m_texture_sampler, m_texture_image.view, vk::ImageLayout::eShaderReadOnlyOptimal };
}



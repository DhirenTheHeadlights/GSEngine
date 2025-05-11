module gse.graphics.texture;

import std;
import vulkan_hpp;

gse::texture::texture(const std::filesystem::path& filepath) : identifiable(filepath.string()), m_filepath(filepath) {
    load_from_file(filepath);
}

gse::texture::~texture() {
	vulkan::config::device::device.destroySampler(m_texture_sampler);
	free(m_texture_image);
}

auto gse::texture::load_from_file(const std::filesystem::path& filepath) -> void {
	auto data = stb::image::load(filepath);

    const vk::BufferCreateInfo buffer_info(
        {},
        data.size_bytes(),
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive
    );

	auto staging_buffer = vulkan::persistent_allocator::create_buffer(buffer_info);

    const auto format = data.channels == 4 ? vk::Format::eR8G8B8A8Srgb : data.channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb; // Note: 3-channel formats might require special handling

    const vk::ImageCreateInfo image_info(
        {},
        vk::ImageType::e2D,
        format,
        vk::Extent3D{ (data.size.x), (data.size.y), 1 },
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

	m_texture_image = vulkan::persistent_allocator::create_image(image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, view_info);

    const vk::CommandBuffer command_buffer = vulkan::begin_single_line_commands();

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
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    const vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        vk::Offset3D{ 0, 0, 0 },
        vk::Extent3D{ (data.size.x), (data.size.y), 1 }
    );

    command_buffer.copyBufferToImage(
        staging_buffer.buffer,
        m_texture_image.image,
        vk::ImageLayout::eTransferDstOptimal,
        1, &region
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
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vulkan::end_single_line_commands(command_buffer);

	free(staging_buffer);

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

    m_texture_sampler = vulkan::config::device::device.createSampler(sampler_info);
}

auto gse::texture::load_from_memory(const std::span<std::uint8_t>& data, const int width, const int height, const int channels) -> void {
    m_size = { width, height };
    m_channels = channels;

    const vk::Format format = channels == 4 ? vk::Format::eR8G8B8A8Srgb : channels == 1 ? vk::Format::eR8Unorm : vk::Format::eR8G8B8Srgb;
    const std::size_t image_size = static_cast<std::size_t>(width) * height * channels;

    m_texture_image = vulkan::persistent_allocator::create_image(
        vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D,
            format,
            vk::Extent3D{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1 },
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
        m_texture_image.image,
        format,
        width,
        height,
        data.data(),
        image_size,
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
}

auto gse::texture::get_descriptor_info() const -> vk::DescriptorImageInfo {
    return { m_texture_sampler, m_texture_image.view, vk::ImageLayout::eShaderReadOnlyOptimal };
}



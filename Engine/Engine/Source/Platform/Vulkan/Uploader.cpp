module gse.platform.vulkan.uploader;

import std;

import gse.platform.vulkan.config;
import gse.platform.vulkan.context;
import gse.platform.vulkan.persistent_allocator;

auto gse::vulkan::uploader::upload_image_2d(config& config, const vk::Image image, vk::Format, const std::uint32_t width, const std::uint32_t height, const void* pixel_data, const size_t data_size, const vk::ImageLayout final_layout) -> void {
    auto staging = persistent_allocator::create_buffer(
        config.device_data,
        vk::BufferCreateInfo{
	        {},
	        data_size,
	        vk::BufferUsageFlagBits::eTransferSrc,
	        vk::SharingMode::eExclusive
		}, 
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
        pixel_data
    );

    const auto cmd = begin_single_line_commands(config);
    transition_image_layout(config, cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1, 1);

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = vk::Extent3D{ width, height, 1 };

    cmd.copyBufferToImage(staging.buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    transition_image_layout(config, cmd, image, vk::ImageLayout::eTransferDstOptimal, final_layout, 1, 1);
    end_single_line_commands(cmd, config);
    free(config.device_data, staging);
}

auto gse::vulkan::uploader::upload_image_layers(config& config, const vk::Image image, vk::Format, const std::uint32_t width, const std::uint32_t height, const std::vector<const void*>& face_data, const size_t bytes_per_face, const vk::ImageLayout final_layout) -> void {
    const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());
    const size_t total_size = layer_count * bytes_per_face;

    auto staging = persistent_allocator::create_buffer(
        config.device_data,
        vk::BufferCreateInfo{
	        {},
	        total_size,
	        vk::BufferUsageFlagBits::eTransferSrc,
	        vk::SharingMode::eExclusive
		}, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

    const auto mapped = static_cast<std::byte*>(staging.allocation.mapped);

    for (size_t i = 0; i < layer_count; ++i) {
        std::memcpy(mapped + i * bytes_per_face, face_data[i], bytes_per_face);
    }

    const auto cmd = begin_single_line_commands(config);
    transition_image_layout(config, cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1, layer_count);

    std::vector<vk::BufferImageCopy> regions;
    for (std::uint32_t i = 0; i < layer_count; ++i) {
        vk::BufferImageCopy copy{};
        copy.bufferOffset = i * bytes_per_face;
        copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = i;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = vk::Extent3D{ width, height, 1 };
        regions.push_back(copy);
    }

    cmd.copyBufferToImage(staging.buffer, image, vk::ImageLayout::eTransferDstOptimal, regions);
    transition_image_layout(config, cmd, image, vk::ImageLayout::eTransferDstOptimal, final_layout, 1, layer_count);
    end_single_line_commands(cmd, config);
    free(config.device_data, staging);
}

auto gse::vulkan::uploader::upload_image_3d(config& config, const vk::Image image, vk::Format, const std::uint32_t width, const std::uint32_t height, const std::uint32_t depth, const void* pixel_data, const size_t data_size, const vk::ImageLayout final_layout) -> void {
    auto staging = persistent_allocator::create_buffer(
        config.device_data,
        vk::BufferCreateInfo{
	        {},
	        data_size,
	        vk::BufferUsageFlagBits::eTransferSrc,
	        vk::SharingMode::eExclusive
		},
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
        pixel_data
    );

    const auto cmd = begin_single_line_commands(config);
    transition_image_layout(config, cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1, 1);

	vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = vk::Extent3D{ width, height, depth };

	cmd.copyBufferToImage(staging.buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    transition_image_layout(config, cmd, image, vk::ImageLayout::eTransferDstOptimal, final_layout, 1, 1);
    end_single_line_commands(cmd, config);
    free(config.device_data, staging);
}

auto gse::vulkan::uploader::upload_mip_mapped_image(config& config, const vk::Image image, const std::span<mip_level_data>& mip_levels, const vk::ImageLayout final_layout) -> void {
    const auto cmd = begin_single_line_commands(config);
    const std::uint32_t mip_count = static_cast<std::uint32_t>(mip_levels.size());

    transition_image_layout(config, cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mip_count, 1);

    std::vector<persistent_allocator::buffer_resource> staging_buffers;
    staging_buffers.reserve(mip_levels.size());

    for (std::uint32_t level = 0; level < mip_count; ++level) {
        const auto& [pixels, size, mip_level] = mip_levels[level];
        auto staging = persistent_allocator::create_buffer(
			config.device_data,
            vk::BufferCreateInfo{
                {},
                mip_level,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::SharingMode::eExclusive
            },
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            pixels
        );
        staging_buffers.push_back(staging);

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = level;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = vk::Extent3D{ size.x, size.y, 1 };

        cmd.copyBufferToImage(staging.buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    }

    transition_image_layout(config, cmd, image, vk::ImageLayout::eTransferDstOptimal, final_layout, mip_count, 1);
    end_single_line_commands(cmd, config);

    for (auto& staging : staging_buffers) {
        free(config.device_data, staging);
    }
}

auto gse::vulkan::uploader::transition_image_layout(config& config, const vk::CommandBuffer cmd, const vk::Image image, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout, const std::uint32_t mip_levels, const std::uint32_t layer_count) -> void {
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layer_count;

    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;
    vk::AccessFlags src_access;
    vk::AccessFlags dst_access;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
        src_access = {};
        dst_access = vk::AccessFlagBits::eTransferWrite;
    }
    else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
        src_access = vk::AccessFlagBits::eTransferWrite;
        dst_access = vk::AccessFlagBits::eShaderRead;
    }
    else {
        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        src_access = {};
        dst_access = {};
    }

    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;

    cmd.pipelineBarrier(src_stage, dst_stage, {}, nullptr, nullptr, barrier);
}
module gse.platform.vulkan.uploader;

import std;

import gse.platform.vulkan.config;
import gse.platform.vulkan.context;
import gse.platform.vulkan.resources;
import gse.platform.vulkan.persistent_allocator;

auto gse::vulkan::uploader::upload_image_2d(const config& config, const vk::Image image, const vk::Format format, const std::uint32_t width, const std::uint32_t height, const void* pixel_data, const size_t data_size, const vk::ImageLayout final_layout) -> void {
    auto [buffer, allocation] = persistent_allocator::create_buffer(
        config.device_data,
        vk::BufferCreateInfo{
            .flags = {},
            .size = data_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive
        },
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        pixel_data
    );

    const auto cmd = begin_single_line_commands(config);
    transition_image_layout(*cmd, image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1, 1);

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
            .width = width,
            .height = height,
            .depth = 1
        }
    };

    cmd.copyBufferToImage(*buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    transition_image_layout(*cmd, image, format, vk::ImageLayout::eTransferDstOptimal, final_layout, 1, 1);
    end_single_line_commands(cmd, config);
}

auto gse::vulkan::uploader::upload_image_layers(const config& config, const vk::Image image, const vk::Format format, const std::uint32_t width, const std::uint32_t height, const std::vector<const void*>& face_data, const size_t bytes_per_face, const vk::ImageLayout final_layout) -> void {
    const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());
    const size_t total_size = layer_count * bytes_per_face;

    auto staging = persistent_allocator::create_buffer(
        config.device_data,
        vk::BufferCreateInfo{
            .flags = {},
            .size = total_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive
        },
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

    const auto mapped = static_cast<std::byte*>(staging.allocation.mapped());

    for (size_t i = 0; i < layer_count; ++i) {
        std::memcpy(mapped + i * bytes_per_face, face_data[i], bytes_per_face);
    }

    const auto cmd = begin_single_line_commands(config);
    transition_image_layout(*cmd, image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1, layer_count);

    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(layer_count);
    for (std::uint32_t i = 0; i < layer_count; ++i) {
        regions.emplace_back(vk::BufferImageCopy{
            .bufferOffset = i * bytes_per_face,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = i,
                .layerCount = 1
            },
            .imageExtent = {
                .width = width,
                .height = height,
                .depth = 1
            }
            });
    }

    cmd.copyBufferToImage(*staging.buffer, image, vk::ImageLayout::eTransferDstOptimal, regions);
    transition_image_layout(*cmd, image, format, vk::ImageLayout::eTransferDstOptimal, final_layout, 1, layer_count);
    end_single_line_commands(cmd, config);
}

auto gse::vulkan::uploader::upload_image_3d(const config& config, const vk::Image image, const vk::Format format, const std::uint32_t width, const std::uint32_t height, const std::uint32_t depth, const void* pixel_data, const size_t data_size, const vk::ImageLayout final_layout) -> void {
    auto [buffer, allocation] = persistent_allocator::create_buffer(
        config.device_data,
        vk::BufferCreateInfo{
            .flags = {},
            .size = data_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive
        },
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        pixel_data
    );

    const auto cmd = begin_single_line_commands(config);
    transition_image_layout(*cmd, image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1, 1);

    const vk::BufferImageCopy region{
        .bufferOffset = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageExtent = {
            .width = width,
            .height = height,
            .depth = depth
        }
    };

    cmd.copyBufferToImage(*buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    transition_image_layout(*cmd, image, format, vk::ImageLayout::eTransferDstOptimal, final_layout, 1, 1);
    end_single_line_commands(cmd, config);
}

auto gse::vulkan::uploader::upload_mip_mapped_image(const config& config, const vk::Image image, const vk::Format format, const std::span<mip_level_data>& mip_levels, const vk::ImageLayout final_layout) -> void {
    const auto cmd = begin_single_line_commands(config);
    const std::uint32_t mip_count = static_cast<std::uint32_t>(mip_levels.size());

    transition_image_layout(*cmd, image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mip_count, 1);

    std::vector<persistent_allocator::buffer_resource> staging_buffers;
    staging_buffers.reserve(mip_levels.size());

    for (std::uint32_t level = 0; level < mip_count; ++level) {
        const auto& [pixels, size, byte_size] = mip_levels[level];
        auto staging = persistent_allocator::create_buffer(
            config.device_data,
            vk::BufferCreateInfo{
                .flags = {},
                .size = byte_size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive
            },
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            pixels
        );
        staging_buffers.push_back(std::move(staging));

        const vk::BufferImageCopy region{
            .bufferOffset = 0,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = level,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageExtent = {
                .width = size.x,
                .height = size.y,
                .depth = 1
            }
        };

        cmd.copyBufferToImage(*staging.buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    }

    transition_image_layout(*cmd, image, format, vk::ImageLayout::eTransferDstOptimal, final_layout, mip_count, 1);
    end_single_line_commands(cmd, config);
}

auto gse::vulkan::uploader::transition_image_layout(const vk::CommandBuffer cmd, const vk::Image image, const vk::Format format, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout, const std::uint32_t mip_levels, const std::uint32_t layer_count) -> void {
    auto choose_aspect = [](const vk::Format fmt) -> vk::ImageAspectFlags {
        switch (fmt) {
        case vk::Format::eD16Unorm:
        case vk::Format::eD32Sfloat:
        case vk::Format::eX8D24UnormPack32:
            return vk::ImageAspectFlagBits::eDepth;
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:
            return vk::ImageAspectFlagBits::eColor;
        }
        };

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

    const vk::ImageMemoryBarrier barrier{
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = {
            .aspectMask = choose_aspect(format),
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = layer_count
        }
    };

    cmd.pipelineBarrier(src_stage, dst_stage, {}, nullptr, nullptr, barrier);
}
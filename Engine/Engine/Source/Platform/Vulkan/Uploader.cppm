export module gse.platform.vulkan:uploader;

import std;

import :config;
import :resources;
import :persistent_allocator;
import :context;

import gse.physics.math;

export namespace gse::vulkan::uploader {
    auto upload_image_2d(
        config& config,
        persistent_allocator::image_resource& resource,
        std::uint32_t width,
        std::uint32_t height,
        const void* pixel_data,
        size_t data_size,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    auto upload_image_layers(
        config& config,
        persistent_allocator::image_resource& resource,
        std::uint32_t width,
        std::uint32_t height,
        const std::vector<const void*>& face_data,
        size_t bytes_per_face,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    auto upload_image_3d(
        config& config,
        persistent_allocator::image_resource& resource,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t depth,
        const void* pixel_data,
        size_t data_size,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    struct mip_level_data {
        const void* pixels = nullptr;
        unitless::vec2_t<std::uint32_t> size;
        std::size_t mip_level;
    };

    auto upload_mip_mapped_image(
        config& config,
        persistent_allocator::image_resource& resource,
        const std::span<mip_level_data>& mip_levels,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    auto transition_image_layout(
        vk::CommandBuffer cmd,
        persistent_allocator::image_resource& image_resource,
        vk::ImageLayout new_layout,
        vk::ImageAspectFlags aspect_mask,
        vk::PipelineStageFlags2 src_stage,
        vk::AccessFlags2 src_access,
        vk::PipelineStageFlagBits2 dst_stage,
        vk::AccessFlags2 dst_access,
        std::uint32_t mip_levels = 1,
        std::uint32_t layer_count = 1
    ) -> void;

    auto upload_to_buffer(
        const persistent_allocator::buffer_resource& destination_buffer,
        const void* data,
        std::size_t size
    ) -> void;
}

auto gse::vulkan::uploader::upload_image_2d(config& config, persistent_allocator::image_resource& resource, const std::uint32_t width, const std::uint32_t height, const void* pixel_data, const size_t data_size, const vk::ImageLayout final_layout) -> void {
    config.add_transient_work(
        [&](const vk::raii::CommandBuffer& cmd) -> std::vector<persistent_allocator::buffer_resource> {
            auto staging_buffer = persistent_allocator::create_buffer(
                config.device_config(),
                vk::BufferCreateInfo{
                    .size = data_size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc
                },
                pixel_data
            );

            transition_image_layout(
                *cmd, resource,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTopOfPipe, {},
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
            );

            const vk::BufferImageCopy region{
                .bufferOffset = 0,
                .imageSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageExtent = { width, height, 1 }
            };

            cmd.copyBufferToImage(*staging_buffer.buffer, *resource.image, vk::ImageLayout::eTransferDstOptimal, region);

            transition_image_layout(
                *cmd,
                resource,
                final_layout,
                vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead
            );

            std::vector<persistent_allocator::buffer_resource> buffers;
            buffers.push_back(std::move(staging_buffer));
            return buffers;
        }
    );
}

auto gse::vulkan::uploader::upload_image_layers(config& config, persistent_allocator::image_resource& resource, const std::uint32_t width, const std::uint32_t height, const std::vector<const void*>& face_data, const size_t bytes_per_face, const vk::ImageLayout final_layout) -> void {
    const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());
    const std::size_t total_size = layer_count * bytes_per_face;

    config.add_transient_work(
        [&](const vk::raii::CommandBuffer& cmd) -> std::vector<persistent_allocator::buffer_resource> {
            auto staging_buffer = persistent_allocator::create_buffer(
                config.device_config(),
                vk::BufferCreateInfo{
                    .size = total_size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc
                }
            );

            const auto mapped = static_cast<std::byte*>(staging_buffer.allocation.mapped());
            for (std::size_t i = 0; i < layer_count; ++i) {
                std::memcpy(mapped + i * bytes_per_face, face_data[i], bytes_per_face);
            }

            transition_image_layout(
                *cmd, resource,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTopOfPipe, {},
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                1, layer_count
            );

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
                    .imageExtent = { width, height, 1 }
                    });
            }

            cmd.copyBufferToImage(*staging_buffer.buffer, *resource.image, vk::ImageLayout::eTransferDstOptimal, regions);

            transition_image_layout(
                *cmd, resource,
                final_layout, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                1, layer_count
            );

            std::vector<persistent_allocator::buffer_resource> buffers;
            buffers.push_back(std::move(staging_buffer));
            return buffers;
        }
    );
}

auto gse::vulkan::uploader::upload_image_3d(config& config, persistent_allocator::image_resource& resource, const std::uint32_t width, const std::uint32_t height, const std::uint32_t depth, const void* pixel_data, const size_t data_size, const vk::ImageLayout final_layout) -> void {
    config.add_transient_work(
        [&](const vk::raii::CommandBuffer& cmd) -> std::vector<persistent_allocator::buffer_resource> {
            auto staging_buffer = persistent_allocator::create_buffer(
                config.device_config(),
                vk::BufferCreateInfo{
                    .size = data_size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc
                },
                pixel_data
            );

            transition_image_layout(
                *cmd, resource,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTopOfPipe, {},
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
            );

            const vk::BufferImageCopy region{
                .bufferOffset = 0,
                .imageSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageExtent = { width, height, depth }
            };

            cmd.copyBufferToImage(*staging_buffer.buffer, *resource.image, vk::ImageLayout::eTransferDstOptimal, region);

            transition_image_layout(
                *cmd, resource,
                final_layout, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
            );

            std::vector<persistent_allocator::buffer_resource> buffers;
            buffers.push_back(std::move(staging_buffer));
            return buffers;
        }
    );
}

auto gse::vulkan::uploader::upload_mip_mapped_image(config& config, persistent_allocator::image_resource& resource, const std::span<mip_level_data>& mip_levels, const vk::ImageLayout final_layout) -> void {
    config.add_transient_work(
        [&](const vk::raii::CommandBuffer& cmd) -> std::vector<persistent_allocator::buffer_resource> {
            const std::uint32_t mip_count = static_cast<std::uint32_t>(mip_levels.size());

            std::vector<persistent_allocator::buffer_resource> staging_buffers;
            staging_buffers.reserve(mip_levels.size());

            transition_image_layout(
                *cmd, resource,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTopOfPipe, {},
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                mip_count, 1
            );

            for (const auto& [pixels, size, mip_level] : mip_levels) {
                auto staging = persistent_allocator::create_buffer(
                    config.device_config(),
                    vk::BufferCreateInfo{
                        .size = mip_level,
                        .usage = vk::BufferUsageFlagBits::eTransferSrc,
                    },
                    pixels
                    );

                const vk::BufferImageCopy region{
                    .bufferOffset = 0,
                    .imageSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = static_cast<uint32_t>(mip_level),
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    },
                    .imageExtent = { size.x(), size.y(), 1}
                };

                cmd.copyBufferToImage(*staging.buffer, *resource.image, vk::ImageLayout::eTransferDstOptimal, region);
                staging_buffers.push_back(std::move(staging));
            }

            transition_image_layout(
                *cmd, resource,
                final_layout, vk::ImageAspectFlagBits::eColor,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                mip_count, 1
            );

            return staging_buffers;
        }
    );
}

auto gse::vulkan::uploader::transition_image_layout(const vk::CommandBuffer cmd, persistent_allocator::image_resource& image_resource, const vk::ImageLayout new_layout, const vk::ImageAspectFlags aspect_mask, const vk::PipelineStageFlags2 src_stage, const vk::AccessFlags2 src_access, const vk::PipelineStageFlagBits2 dst_stage, const vk::AccessFlags2 dst_access, const std::uint32_t mip_levels, const std::uint32_t layer_count) -> void {
    if (image_resource.current_layout == new_layout) {
        return;
    }

    const vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .oldLayout = image_resource.current_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = *image_resource.image,
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = layer_count
        }
    };

    cmd.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

    image_resource.current_layout = new_layout;
}

auto gse::vulkan::uploader::upload_to_buffer(const persistent_allocator::buffer_resource& destination_buffer, const void* data, const std::size_t size) -> void {
    assert(destination_buffer.allocation.mapped(), std::source_location::current(), "Buffer for uploading must be persistently mapped");
    assert(size <= destination_buffer.allocation.size(), std::source_location::current(), "Upload size exceeds buffer allocation size");

    std::memcpy(destination_buffer.allocation.mapped(), data, size);
}

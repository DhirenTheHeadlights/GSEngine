export module gse.gpu.vulkan:vulkan_uploader;

import std;

import :vulkan_allocator;

import gse.assert;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::vulkan::uploader {
    auto transition_image_layout(
	    vk::CommandBuffer cmd,
	    image_resource& image_resource,
	    vk::ImageLayout new_layout,
	    vk::ImageAspectFlags aspect_mask,
	    vk::PipelineStageFlags2 src_stage,
	    vk::AccessFlags2 src_access,
        vk::PipelineStageFlags2 dst_stage,
	    vk::AccessFlags2 dst_access,
	    std::uint32_t mip_levels = 1,
	    std::uint32_t layer_count = 1
    ) -> void;

    auto upload_to_buffer(
        const buffer_resource& destination_buffer,
        const void* data,
        std::size_t size
    ) -> void;
}

auto gse::vulkan::uploader::transition_image_layout(const vk::CommandBuffer cmd, image_resource& image_resource, const vk::ImageLayout new_layout, const vk::ImageAspectFlags aspect_mask, const vk::PipelineStageFlags2 src_stage, const vk::AccessFlags2 src_access, const vk::PipelineStageFlags2 dst_stage, const vk::AccessFlags2 dst_access, const std::uint32_t mip_levels, const std::uint32_t layer_count) -> void {
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
        .image = image_resource.image,
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

auto gse::vulkan::uploader::upload_to_buffer(const buffer_resource& destination_buffer, const void* data, const std::size_t size) -> void {
    assert(destination_buffer.allocation.mapped(), std::source_location::current(), "Buffer for uploading must be persistently mapped");
	assert(size <= destination_buffer.size, std::source_location::current(), "Upload size exceeds buffer size");

    gse::memcpy(destination_buffer.allocation.mapped(), data, size);
}

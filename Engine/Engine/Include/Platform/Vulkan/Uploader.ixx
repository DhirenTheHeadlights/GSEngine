export module gse.platform.vulkan.uploader;

import std;
import vulkan_hpp;

import gse.physics.math;
import gse.platform.vulkan.config;

export namespace gse::vulkan::uploader {
    auto upload_image_2d(
        config& config,
        vk::Image image,
        vk::Format format,
        std::uint32_t width,
        std::uint32_t height,
        const void* pixel_data,
        size_t data_size,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    auto upload_image_layers(
        config& config,
        vk::Image image,
        vk::Format format,
        std::uint32_t width,
        std::uint32_t height,
        const std::vector<const void*>& face_data,
        size_t bytes_per_face,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    auto upload_image_3d(
        config& config,
        vk::Image image,
        vk::Format format,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t depth,
        const void* voxel_data,
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
        vk::Image image,
        const std::span<mip_level_data>& mip_levels,
        vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
    ) -> void;

    auto transition_image_layout(
        config& config,
        vk::CommandBuffer cmd,
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        std::uint32_t mip_levels, 
        std::uint32_t layer_count
    ) -> void;
}
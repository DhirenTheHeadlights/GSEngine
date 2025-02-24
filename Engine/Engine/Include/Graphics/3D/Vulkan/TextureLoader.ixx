module;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

export module gse.graphics.texture_loader;

import std;
import vulkan_hpp;

import gse.core.id;
import gse.platform.perma_assert;
import gse.platform.context;

export namespace gse::texture_loader {
    auto load_texture(const std::filesystem::path& path) -> vk::Image;
    auto get_texture_by_path(const std::filesystem::path& texture_path) -> vk::Image;
    auto get_texture_by_id(id* texture_id) -> vk::Image;
}

namespace gse::texture_loader {
	auto create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& memory) -> void;
    auto create_image(std::uint32_t width, std::uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& memory) -> void;
	auto create_image_view(vk::Image image, vk::Format format) -> vk::ImageView;
	auto transition_image_layout(vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout) -> void;
    auto copy_buffer_to_image(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) -> void;
}

struct texture;
std::unordered_map<gse::id*, texture> g_textures;

struct texture : gse::identifiable {
    texture(const std::string& tag, const vk::Image image, const vk::DeviceMemory memory, const vk::ImageView view)
        : identifiable(tag), image(image), memory(memory), view(view) {
    }
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageView view;
};

auto gse::texture_loader::load_texture(const std::filesystem::path& path) -> vk::Image {
    int width, height, number_components;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &number_components, STBI_rgb_alpha);
    perma_assert(pixels, "Failed to load texture image");

    const vk::Device device = get_device();
    vk::PhysicalDevice physical_device = get_physical_device();

    const vk::DeviceSize image_size = width * height * 4;

    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_memory;
    create_buffer(
        image_size, 
        vk::BufferUsageFlagBits::eTransferSrc, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        staging_buffer, staging_memory
    );

    void* data;
	perma_assert(device.mapMemory(staging_memory, 0, image_size, {}, &data) == vk::Result::eSuccess, "Failed to map memory");

    memcpy(data, pixels, image_size);
    device.unmapMemory(staging_memory);

    stbi_image_free(pixels);

    vk::Image texture_image;
    vk::DeviceMemory texture_memory;
    create_image(width, height, vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, texture_image, texture_memory);

    transition_image_layout(texture_image, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal);
    copy_buffer_to_image(staging_buffer, texture_image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    transition_image_layout(texture_image, vk::ImageLayout::eTransferDstOptimal,
                            vk::ImageLayout::eShaderReadOnlyOptimal);

    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_memory);

    const vk::ImageView texture_view = create_image_view(texture_image, vk::Format::eR8G8B8A8Srgb);

    texture new_texture(path.string(), texture_image, texture_memory, texture_view);
    g_textures.insert({ new_texture.get_id(), std::move(new_texture) });
    return texture_image;
}

auto gse::texture_loader::get_texture_by_path(const std::filesystem::path& texture_path) -> vk::Image {
    const auto it = std::ranges::find_if(g_textures, [&texture_path](const auto& texture) { return texture.first->tag() == texture_path; });
    if (it != g_textures.end()) {
        return it->second.image;
    }
    return load_texture(texture_path);
}

auto gse::texture_loader::get_texture_by_id(id* texture_id) -> vk::Image {
    const auto it = g_textures.find(texture_id);
    perma_assert(it != g_textures.end(), "Grabbing non-existent texture by id, could not find.");
    return it->second.image;
}

auto gse::texture_loader::create_buffer(const vk::DeviceSize size, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory) -> void {
    const vk::Device device = get_device();

    const vk::BufferCreateInfo buffer_info({}, size, usage, vk::SharingMode::eExclusive);
    buffer = device.createBuffer(buffer_info);

    const vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(buffer);
    const vk::MemoryAllocateInfo alloc_info(mem_requirements.size, gse::find_memory_type(mem_requirements.memoryTypeBits, properties));

    buffer_memory = device.allocateMemory(alloc_info);
    device.bindBufferMemory(buffer, buffer_memory, 0);
}


auto gse::texture_loader::create_image(const std::uint32_t width, const std::uint32_t height, const vk::Format format, const vk::ImageTiling tiling, const vk::ImageUsageFlags usage, const vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& memory) -> void {
	const vk::Device device = get_device();
	const vk::ImageCreateInfo image_info(
		{},
		vk::ImageType::e2D,
		format,
		vk::Extent3D(width, height, 1),
		1,
		1,
		vk::SampleCountFlagBits::e1,
		tiling,
		usage,
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined
	);

	image = device.createImage(image_info);

	const vk::MemoryRequirements memory_requirements = device.getImageMemoryRequirements(image);
	const vk::MemoryAllocateInfo alloc_info(
		memory_requirements.size,
		find_memory_type(memory_requirements.memoryTypeBits, properties)
	);

	memory = device.allocateMemory(alloc_info);
	device.bindImageMemory(image, memory, 0);
}

auto gse::texture_loader::create_image_view(const vk::Image image, const vk::Format format) -> vk::ImageView {
	const vk::Device device = get_device();
	const vk::ImageViewCreateInfo view_info(
		{},
		image,
		vk::ImageViewType::e2D,
		format,
		{},
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
	);
	return device.createImageView(view_info);
}

auto gse::texture_loader::transition_image_layout(const vk::Image image, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout) -> void {
    const vk::CommandBuffer command_buffer = begin_single_line_commands();

	const vk::ImageMemoryBarrier barrier({}, {}, old_layout, new_layout, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, image, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
        {}, nullptr, nullptr, barrier);

    vulkan::end_single_line_commands(command_buffer);
}

auto gse::texture_loader::copy_buffer_to_image(const vk::Buffer buffer, const vk::Image image, uint32_t width, uint32_t height) -> void {
    const vk::CommandBuffer command_buffer = begin_single_line_commands();

    const vk::BufferImageCopy region(0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, { width, height, 1 });
    command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    end_single_line_commands(command_buffer);
}

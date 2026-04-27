export module gse.gpu:image;

import std;
import vulkan;

import :types;
import :vulkan_allocator;
import :vulkan_reflect;
import :vulkan_uploader;
import :device;
import :frame;
import :context;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::gpu {
	class image_view {
	public:
		image_view() = default;
		explicit image_view(vk::ImageView handle) : m_handle(handle) {}

		[[nodiscard]] auto native() const -> vk::ImageView { return m_handle; }
		explicit operator bool() const { return static_cast<bool>(m_handle); }
	private:
		vk::ImageView m_handle{};
	};

	class image final : public non_copyable {
	public:
		image() = default;
		image(vulkan::image_resource&& resource, image_format fmt, vec2u extent, std::vector<vk::raii::ImageView>&& layer_views = {});
		image(image&&) noexcept = default;
		auto operator=(image&&) noexcept -> image& = default;

		[[nodiscard]] auto extent(this const image& self) -> vec2u { return self.m_extent; }
		[[nodiscard]] auto format(this const image& self) -> image_format { return self.m_format; }
		[[nodiscard]] auto layout(this const image& self) -> image_layout;
		auto set_layout(image_layout l) -> void;

		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_resource); }
		[[nodiscard]] auto view(this const image& self) -> image_view { return image_view(self.m_resource.view); }
		[[nodiscard]] auto layer_view(std::uint32_t layer) const -> image_view;

		operator const vulkan::image_resource&(
		) const;

		explicit operator bool() const;
	private:
		vulkan::image_resource m_resource;
		image_format m_format = image_format::d32_sfloat;
		vec2u m_extent = { 1, 1 };
		std::vector<vk::raii::ImageView> m_layer_views;
	};

	auto create_image(
		context& ctx,
		const image_desc& desc
	) -> image;

	auto upload_image_2d(
		context& ctx,
		image& img,
		const void* pixel_data,
		std::size_t data_size
	) -> void;

	auto upload_image_layers(
		context& ctx,
		image& img,
		const std::vector<const void*>& face_data,
		std::size_t bytes_per_face
	) -> void;
}

namespace {
	auto from_vk_layout(vk::ImageLayout l) -> gse::gpu::image_layout {
		switch (l) {
			case vk::ImageLayout::eGeneral:              return gse::gpu::image_layout::general;
			case vk::ImageLayout::eShaderReadOnlyOptimal: return gse::gpu::image_layout::shader_read_only;
			default:                                     return gse::gpu::image_layout::undefined;
		}
	}
}

gse::gpu::image::image(vulkan::image_resource&& resource, image_format fmt, vec2u extent, std::vector<vk::raii::ImageView>&& layer_views)
	: m_resource(std::move(resource)), m_format(fmt), m_extent(extent), m_layer_views(std::move(layer_views)) {}

auto gse::gpu::image::layer_view(const std::uint32_t layer) const -> image_view {
	return image_view(*m_layer_views[layer]);
}

auto gse::gpu::image::layout(this const image& self) -> image_layout {
	return from_vk_layout(self.m_resource.current_layout);
}

auto gse::gpu::image::set_layout(image_layout l) -> void {
	m_resource.current_layout = vulkan::to_vk(l);
}

gse::gpu::image::operator const vulkan::image_resource&() const {
	return m_resource;
}

gse::gpu::image::operator bool() const {
	return m_resource.image != nullptr;
}

auto gse::gpu::create_image(context& ctx, const image_desc& desc) -> image {
	auto& dev = ctx.device();
	const auto vk_format = vulkan::to_vk(desc.format);
	const bool is_depth = desc.format == image_format::d32_sfloat;
	const bool is_cube = desc.view == image_view_type::cube;
	const std::uint32_t layers = is_cube ? 6u : 1u;

	vk::ImageUsageFlags usage{};
	using enum image_flag;
	if (desc.usage.test(sampled))          usage |= vk::ImageUsageFlagBits::eSampled;
	if (desc.usage.test(depth_attachment)) usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	if (desc.usage.test(color_attachment)) usage |= vk::ImageUsageFlagBits::eColorAttachment;
	if (desc.usage.test(transfer_dst))     usage |= vk::ImageUsageFlagBits::eTransferDst;
	if (desc.usage.test(storage))          usage |= vk::ImageUsageFlagBits::eStorage;
	if (desc.usage.test(transfer_src))     usage |= vk::ImageUsageFlagBits::eTransferSrc;

	auto resource = dev.allocator().create_image(
		{
			.flags = is_cube ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{},
			.imageType = vk::ImageType::e2D,
			.format = vk_format,
			.extent = { desc.size.x(), desc.size.y(), 1 },
			.mipLevels = 1,
			.arrayLayers = layers,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = usage
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{
			.viewType = is_cube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
			.format = vk_format,
			.subresourceRange = {
				.aspectMask = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = layers
			}
		}
	);

	if (desc.ready_layout != image_layout::undefined) {
		const auto target = vulkan::to_vk(desc.ready_layout);
		const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
		const auto img = resource.image;

		ctx.frame().add_transient_work([img, target, aspect, layers, is_depth](const auto& cmd) {
			const auto dst_stage = is_depth
				? (vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
				: vk::PipelineStageFlagBits2::eAllCommands;
			const auto dst_access = is_depth
				? (vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead)
				: vk::AccessFlagBits2::eShaderRead;

			const vk::ImageMemoryBarrier2 barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = dst_stage,
				.dstAccessMask = dst_access,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = target,
				.image = img,
				.subresourceRange = {
					.aspectMask = aspect,
					.baseMipLevel = 0, .levelCount = 1,
					.baseArrayLayer = 0, .layerCount = layers
				}
			};

			const vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
			cmd.pipelineBarrier2(dep);

			return std::vector<vulkan::buffer_resource>{};
		});

		resource.current_layout = target;
	}

	std::vector<vk::raii::ImageView> layer_views;
	if (is_cube) {
		const auto& vk_device = dev.logical_device();
		const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
		layer_views.reserve(layers);
		for (std::uint32_t i = 0; i < layers; ++i) {
			layer_views.push_back(vk_device.createImageView({
				.image = resource.image,
				.viewType = vk::ImageViewType::e2D,
				.format = vk_format,
				.subresourceRange = {
					.aspectMask = aspect,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = i,
					.layerCount = 1
				}
			}));
		}
	}

	return image(std::move(resource), desc.format, desc.size, std::move(layer_views));
}

auto gse::gpu::upload_image_2d(context& ctx, image& img, const void* pixel_data, const std::size_t data_size) -> void {
	const auto extent = img.extent();
	auto& resource = img.native();
	auto& dev = ctx.device();

	ctx.frame().add_transient_work([&dev, &resource, extent, pixel_data, data_size](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
		auto staging = dev.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = data_size,
				.usage = vk::BufferUsageFlagBits::eTransferSrc
			},
			pixel_data
		);

		vulkan::uploader::transition_image_layout(
			*cmd,
			resource,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTopOfPipe,
			{},
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite
		);

		const vk::BufferImageCopy region{
			.bufferOffset = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.imageExtent = { extent.x(), extent.y(), 1 }
		};

		cmd.copyBufferToImage(staging.buffer, resource.image, vk::ImageLayout::eTransferDstOptimal, region);

		vulkan::uploader::transition_image_layout(
			*cmd,
			resource,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderRead
		);

		std::vector<vulkan::buffer_resource> buffers;
		buffers.push_back(std::move(staging));
		return buffers;
	});

	img.set_layout(image_layout::shader_read_only);
}

auto gse::gpu::upload_image_layers(context& ctx, image& img, const std::vector<const void*>& face_data, const std::size_t bytes_per_face) -> void {
	const auto extent = img.extent();
	auto& resource = img.native();
	auto& dev = ctx.device();
	const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());
	const std::size_t total_size = layer_count * bytes_per_face;

	ctx.frame().add_transient_work([&dev, &resource, &face_data, extent, layer_count, total_size, bytes_per_face](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
		auto staging = dev.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = total_size,
				.usage = vk::BufferUsageFlagBits::eTransferSrc
			}
		);

		const auto mapped = staging.allocation.mapped();
		for (std::size_t i = 0; i < layer_count; ++i) {
			gse::memcpy(mapped + i * bytes_per_face, face_data[i], bytes_per_face);
		}

		vulkan::uploader::transition_image_layout(
			*cmd,
			resource,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTopOfPipe,
			{},
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			1,
			layer_count
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
				.imageExtent = { extent.x(), extent.y(), 1 }
			});
		}

		cmd.copyBufferToImage(staging.buffer, resource.image, vk::ImageLayout::eTransferDstOptimal, regions);

		vulkan::uploader::transition_image_layout(
			*cmd,
			resource,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderRead,
			1,
			layer_count
		);

		std::vector<vulkan::buffer_resource> buffers;
		buffers.push_back(std::move(staging));
		return buffers;
	});

	img.set_layout(image_layout::shader_read_only);
}

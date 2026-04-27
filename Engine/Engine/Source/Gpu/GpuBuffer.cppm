export module gse.gpu.resources:gpu_buffer;

import std;

import gse.gpu.types;
import gse.gpu.vulkan;
import gse.gpu.device;
import gse.gpu.context;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::gpu {
	class buffer final : public non_copyable {
	public:
		buffer() = default;
		buffer(vulkan::buffer_resource&& resource);
		buffer(buffer&&) noexcept = default;
		auto operator=(buffer&&) noexcept -> buffer& = default;

		[[nodiscard]] auto mapped(this const buffer& self) -> std::byte* { return self.m_resource.allocation.mapped(); }
		[[nodiscard]] auto size(this const buffer& self) -> std::size_t { return self.m_resource.size; }
		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_resource); }

		operator const vulkan::buffer_resource&(
		) const;

		explicit operator bool() const;
	private:
		vulkan::buffer_resource m_resource;
	};

	struct descriptor_buffer_binding {
		std::uint32_t binding = 0;
		const buffer* buf = nullptr;
		std::size_t offset = 0;
		std::size_t range = 0;
	};

	struct buffer_upload {
		const buffer* dst = nullptr;
		const void* data = nullptr;
		std::size_t size = 0;
		std::size_t dst_offset = 0;
	};

	auto create_buffer(
		context& ctx,
		const buffer_desc& desc
	) -> buffer;

	auto upload_to_buffers(
		context& ctx,
		std::span<const buffer_upload> uploads
	) -> void;
}

gse::gpu::buffer::buffer(vulkan::buffer_resource&& resource)
	: m_resource(std::move(resource)) {}

gse::gpu::buffer::operator const vulkan::buffer_resource&() const {
	return m_resource;
}

gse::gpu::buffer::operator bool() const {
	return m_resource.buffer != nullptr;
}

auto gse::gpu::create_buffer(context& ctx, const buffer_desc& desc) -> buffer {
	auto vk_usage = vulkan::to_vk(desc.usage);
	auto resource = ctx.allocator().create_buffer({
		.size = static_cast<vk::DeviceSize>(desc.size),
		.usage = vk_usage
	}, desc.data);
	return buffer(std::move(resource));
}

auto gse::gpu::upload_to_buffers(context& ctx, const std::span<const buffer_upload> uploads) -> void {
	if (uploads.empty()) return;

	struct upload_entry {
		vk::Buffer dst;
		const void* data;
		vk::DeviceSize size;
		vk::DeviceSize offset;
	};

	std::vector<upload_entry> entries;
	entries.reserve(uploads.size());
	for (std::size_t i = 0; i < uploads.size(); ++i) {
		const auto& [dst, data, size, dst_offset] = uploads[i];
		entries.push_back({
			.dst = dst->native().buffer,
			.data = data,
			.size = static_cast<vk::DeviceSize>(size),
			.offset = static_cast<vk::DeviceSize>(dst_offset)
		});
	}

	auto& dev = ctx.device();
	ctx.frame().add_transient_work([&dev, entries = std::move(entries)](const auto& cmd) {
		std::vector<vulkan::buffer_resource> transient;
		transient.reserve(entries.size());

		for (const auto& [dst, data, size, offset] : entries) {
			auto staging = dev.allocator().create_buffer({
				.size = size,
				.usage = vk::BufferUsageFlagBits::eTransferSrc
			}, data);

			cmd.copyBuffer(staging.buffer, dst, vk::BufferCopy(0, offset, size));
			transient.push_back(std::move(staging));
		}

		return transient;
	});
}

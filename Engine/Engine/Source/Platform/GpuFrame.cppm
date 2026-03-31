export module gse.platform:gpu_frame;

import std;

import :vulkan_runtime;

import gse.utility;

export namespace gse::gpu {
	enum class frame_status : std::uint8_t {
		minimized,
		swapchain_out_of_date,
		device_lost
	};

	struct frame_token {
		std::uint32_t frame_index = 0;
		std::uint32_t image_index = 0;
	};

	class frame final : public non_copyable {
	public:
		explicit frame(vulkan::runtime& runtime);

		[[nodiscard]] auto current_frame(
		) const -> std::uint32_t;

		[[nodiscard]] auto image_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto command_buffer(
		) const -> vk::CommandBuffer;

		[[nodiscard]] auto frame_in_progress(
		) const -> bool;

		[[nodiscard]] auto runtime_ref(
		) -> vulkan::runtime&;

	private:
		vulkan::runtime* m_runtime;
	};
}

gse::gpu::frame::frame(vulkan::runtime& runtime)
	: m_runtime(&runtime) {}

auto gse::gpu::frame::current_frame() const -> std::uint32_t {
	return m_runtime->current_frame();
}

auto gse::gpu::frame::image_index() const -> std::uint32_t {
	return m_runtime->frame_context().image_index;
}

auto gse::gpu::frame::command_buffer() const -> vk::CommandBuffer {
	return m_runtime->frame_context().command_buffer;
}

auto gse::gpu::frame::frame_in_progress() const -> bool {
	return m_runtime->frame_in_progress();
}

auto gse::gpu::frame::runtime_ref() -> vulkan::runtime& {
	return *m_runtime;
}

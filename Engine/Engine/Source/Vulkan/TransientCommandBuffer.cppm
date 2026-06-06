export module gse.vulkan:transient_command_buffer;

import std;
import vulkan;

import :handles;
import :commands;

import gse.core;
import gse.assert;

export namespace gse::vulkan {
	class transient_command_buffer final {
	public:
		transient_command_buffer() = default;

		transient_command_buffer(
			gpu::command_buffer_handle cmd,
			std::size_t worker_index
		);

		[[nodiscard]] auto handle() const -> gpu::command_buffer_handle;

		[[nodiscard]] auto worker_index() const -> std::size_t;

		auto begin_one_time() -> void;

		auto end() -> void;

		auto set_marker_seq(
			std::uint64_t seq
		) -> void;

		[[nodiscard]] auto marker_seq() const -> std::uint64_t;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::command_buffer_handle m_cmd;
		std::size_t m_worker_index = 0;
		std::uint64_t m_marker_seq = std::numeric_limits<std::uint64_t>::max();
	};
}

gse::vulkan::transient_command_buffer::transient_command_buffer(const gpu::command_buffer_handle cmd, const std::size_t worker_index)
	: m_cmd(cmd), m_worker_index(worker_index) {
}

auto gse::vulkan::transient_command_buffer::handle() const -> gpu::command_buffer_handle {
	return m_cmd;
}

auto gse::vulkan::transient_command_buffer::worker_index() const -> std::size_t {
	return m_worker_index;
}

auto gse::vulkan::transient_command_buffer::begin_one_time() -> void {
	constexpr vk::CommandBufferBeginInfo begin_info{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};
	const auto result = std::bit_cast<vk::CommandBuffer>(m_cmd).begin(begin_info);
	assert(result == vk::Result::eSuccess, "failed to begin command buffer: {}", vk::to_string(result));
}

auto gse::vulkan::transient_command_buffer::end() -> void {
	const auto result = std::bit_cast<vk::CommandBuffer>(m_cmd).end();
	assert(result == vk::Result::eSuccess, "failed to end command buffer: {}", vk::to_string(result));
}

auto gse::vulkan::transient_command_buffer::set_marker_seq(const std::uint64_t seq) -> void {
	m_marker_seq = seq;
}

auto gse::vulkan::transient_command_buffer::marker_seq() const -> std::uint64_t {
	return m_marker_seq;
}

auto gse::vulkan::transient_command_buffer::valid() const -> bool {
	return static_cast<bool>(m_cmd);
}

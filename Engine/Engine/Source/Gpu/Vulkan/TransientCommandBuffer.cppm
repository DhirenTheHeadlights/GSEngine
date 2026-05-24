export module gse.gpu:vulkan_transient_command_buffer;

import std;
import vulkan;

import :handles;
import :vulkan_commands;

import gse.core;

export namespace gse::vulkan {
	class transient_command_pool;

	class transient_command_buffer final {
	public:
		transient_command_buffer() = default;

		[[nodiscard]] auto handle() const -> gpu::handle<command_buffer>;

		[[nodiscard]] auto origin_pool() const -> transient_command_pool*;

		auto begin_one_time() -> void;

		auto end() -> void;

		auto set_marker_seq(
			std::uint64_t seq
		) -> void;

		[[nodiscard]] auto marker_seq() const -> std::uint64_t;

		[[nodiscard]] auto valid() const -> bool;

	private:
		friend class transient_command_pool;

		transient_command_buffer(
			vk::CommandBuffer cmd,
			transient_command_pool* pool
		);

		vk::CommandBuffer m_cmd{ nullptr };
		transient_command_pool* m_pool = nullptr;
		std::uint64_t m_marker_seq = std::numeric_limits<std::uint64_t>::max();
	};
}

gse::vulkan::transient_command_buffer::transient_command_buffer(const vk::CommandBuffer cmd, transient_command_pool* pool)
	: m_cmd(cmd), m_pool(pool) {
}

auto gse::vulkan::transient_command_buffer::handle() const -> gpu::handle<command_buffer> {
	return std::bit_cast<gpu::handle<command_buffer>>(m_cmd);
}

auto gse::vulkan::transient_command_buffer::origin_pool() const -> transient_command_pool* {
	return m_pool;
}

auto gse::vulkan::transient_command_buffer::begin_one_time() -> void {
	constexpr vk::CommandBufferBeginInfo begin_info{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};
	m_cmd.begin(begin_info);
}

auto gse::vulkan::transient_command_buffer::end() -> void {
	m_cmd.end();
}

auto gse::vulkan::transient_command_buffer::set_marker_seq(const std::uint64_t seq) -> void {
	m_marker_seq = seq;
}

auto gse::vulkan::transient_command_buffer::marker_seq() const -> std::uint64_t {
	return m_marker_seq;
}

auto gse::vulkan::transient_command_buffer::valid() const -> bool {
	return m_cmd != nullptr;
}

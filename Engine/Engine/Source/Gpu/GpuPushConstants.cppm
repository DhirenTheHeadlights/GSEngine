export module gse.gpu.shader:gpu_push_constants;

import std;
import vulkan;

import gse.gpu.types;
import gse.gpu.vulkan;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::gpu {
	struct push_constant_member {
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	class cached_push_constants {
	public:
		std::unordered_map<std::string, push_constant_member> members; 
		std::vector<std::byte> data;
		stage_flags stage_flags{};

		template <typename T>
		auto set(
			std::string_view name,
			const T& value
		) -> void;

		auto replay(
			vk::CommandBuffer cmd,
			vk::PipelineLayout layout
		) const -> void;
	};
}

template <typename T>
auto gse::gpu::cached_push_constants::set(const std::string_view name, const T& value) -> void {
	const auto it = members.find(std::string(name));
	assert(it != members.end(), std::source_location::current(), "Push constant member '{}' not found", name);
	assert(sizeof(T) <= it->second.size, std::source_location::current(), "Push constant member '{}' size mismatch", name);
	gse::memcpy(data.data() + it->second.offset, value);
}

auto gse::gpu::cached_push_constants::replay(const vk::CommandBuffer cmd, const vk::PipelineLayout layout) const -> void {
	const vk::PushConstantsInfoKHR info{
		.layout = layout,
		.stageFlags = vulkan::to_vk(stage_flags),
		.offset = 0,
		.size = static_cast<std::uint32_t>(data.size()),
		.pValues = data.data()
	};
	cmd.pushConstants2KHR(info);
}

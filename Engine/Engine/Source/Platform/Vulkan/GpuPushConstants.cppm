export module gse.platform:gpu_push_constants;

import std;
import vulkan;

import :gpu_types;

import gse.assert;
import gse.utility;

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

namespace {
	auto to_vk_stage_flags(const gse::gpu::stage_flags flags) -> vk::ShaderStageFlags {
		vk::ShaderStageFlags result;
		using enum gse::gpu::stage_flag;
		if (flags.test(vertex))   result |= vk::ShaderStageFlagBits::eVertex;
		if (flags.test(fragment)) result |= vk::ShaderStageFlagBits::eFragment;
		if (flags.test(compute))  result |= vk::ShaderStageFlagBits::eCompute;
		if (flags.test(task))     result |= vk::ShaderStageFlagBits::eTaskEXT;
		if (flags.test(mesh))     result |= vk::ShaderStageFlagBits::eMeshEXT;
		return result;
	}
}

template <typename T>
auto gse::gpu::cached_push_constants::set(const std::string_view name, const T& value) -> void {
	const auto it = members.find(std::string(name));
	assert(it != members.end(), std::source_location::current(), "Push constant member '{}' not found", name);
	assert(sizeof(T) <= it->second.size, std::source_location::current(), "Push constant member '{}' size mismatch", name);
	gse::memcpy(data.data() + it->second.offset, value);
}

auto gse::gpu::cached_push_constants::replay(const vk::CommandBuffer cmd, const vk::PipelineLayout layout) const -> void {
	cmd.pushConstants(layout, to_vk_stage_flags(stage_flags), 0, static_cast<std::uint32_t>(data.size()), data.data());
}
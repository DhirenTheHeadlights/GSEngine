export module gse.gpu:pipeline;

import std;

import :handles;
import :types;
import :vulkan_commands;
import :aliases;

export namespace gse::gpu {
	template <typename T>
	struct typed_push_constants {
		T data{};
		stage_flags stages{};

		auto replay(
			handle<command_buffer> cmd,
			handle<pipeline_layout> layout
		) const -> void;
	};
}

template <typename T>
auto gse::gpu::typed_push_constants<T>::replay(const handle<command_buffer> cmd, const handle<pipeline_layout> layout) const -> void {
	vulkan::commands{ cmd }.push_constants(
		layout,
		stages,
		0,
		static_cast<std::uint32_t>(sizeof(T)),
		&data
	);
}

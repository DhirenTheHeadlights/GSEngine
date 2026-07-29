export module gse.gpu:command_dispatch;

import std;

import :command_contract;

import gse.gpu_backend;
import gse.meta;

export namespace gse::gpu {
	template <typename B>
	[[nodiscard]] auto command_dispatch_for() -> const command_dispatch*;
}

namespace gse::gpu {
	constexpr std::array<std::string_view, 2> recorder_excludes{ "native", "valid" };
}

template <typename B>
auto gse::gpu::command_dispatch_for() -> const command_dispatch* {
	static constexpr command_dispatch dispatch = meta::build_dispatch<command_dispatch, pass_recorder, meta::value_receiver<B, command_buffer_handle>, recorder_excludes>();
	return &dispatch;
}

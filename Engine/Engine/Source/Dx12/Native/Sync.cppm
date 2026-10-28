export module gse.dx12:sync;

import std;

import gse.gpu_backend;
import gse.core;

export namespace gse::dx12 {
	struct fence {};

	class sync final : public non_copyable {
	public:
		sync() {}
		~sync() = default;

		sync(
			sync&&
		) noexcept = default;

		auto operator=(
			sync&&
		) noexcept -> sync& = default;

		[[nodiscard]] auto image_available(
			std::uint32_t frame_index
		) const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto render_finished(
			std::uint32_t image_index
		) const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto in_flight_fence(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::handle<gpu::fence>;
	};
}

auto gse::dx12::sync::image_available(const std::uint32_t) const -> gpu::handle<gpu::semaphore> {
	return {};
}

auto gse::dx12::sync::render_finished(const std::uint32_t) const -> gpu::handle<gpu::semaphore> {
	return {};
}

auto gse::dx12::sync::in_flight_fence(const gpu::queue_type, const std::uint32_t) const -> gpu::handle<gpu::fence> {
	return {};
}

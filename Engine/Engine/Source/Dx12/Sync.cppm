export module gse.dx12:sync;

import std;

import gse.gpu_types;
import gse.core;

export namespace gse::dx12 {
	struct fence {};

	class semaphore final : public non_copyable {
	public:
		semaphore() {}
		~semaphore() = default;

		semaphore(
			semaphore&&
		) noexcept = default;

		auto operator=(
			semaphore&&
		) noexcept -> semaphore& = default;

		[[nodiscard]] auto handle(
			this const semaphore& self
		) -> gpu::semaphore_handle;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::semaphore_handle m_handle;
	};

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
		) const -> gpu::semaphore_handle;

		[[nodiscard]] auto render_finished(
			std::uint32_t image_index
		) const -> gpu::semaphore_handle;

		[[nodiscard]] auto in_flight_fence(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::fence_handle;
	};

	class query_pool final : public non_copyable {
	public:
		query_pool() {}
		~query_pool() = default;

		query_pool(
			query_pool&&
		) noexcept = default;

		auto operator=(
			query_pool&&
		) noexcept -> query_pool& = default;

		[[nodiscard]] auto handle(
			this const query_pool& self
		) -> gpu::query_pool_handle;

		[[nodiscard]] auto valid() const -> bool;

		template <typename T>
		[[nodiscard]]
		auto results(
			std::uint32_t first_query,
			std::uint32_t query_count,
			std::uint64_t stride
		) const -> std::pair<gpu::query_status, std::vector<T>>;

	private:
		gpu::query_pool_handle m_handle;
	};
}

auto gse::dx12::semaphore::handle(this const semaphore& self) -> gpu::semaphore_handle {
	return self.m_handle;
}

auto gse::dx12::semaphore::valid() const -> bool {
	return false;
}

auto gse::dx12::sync::image_available(const std::uint32_t) const -> gpu::semaphore_handle {
	return {};
}

auto gse::dx12::sync::render_finished(const std::uint32_t) const -> gpu::semaphore_handle {
	return {};
}

auto gse::dx12::sync::in_flight_fence(const gpu::queue_type, const std::uint32_t) const -> gpu::fence_handle {
	return {};
}

auto gse::dx12::query_pool::handle(this const query_pool& self) -> gpu::query_pool_handle {
	return self.m_handle;
}

auto gse::dx12::query_pool::valid() const -> bool {
	return false;
}

template <typename T>
auto gse::dx12::query_pool::results(const std::uint32_t, const std::uint32_t, const std::uint64_t) const -> std::pair<gpu::query_status, std::vector<T>> {
	return { gpu::query_status::error, {} };
}

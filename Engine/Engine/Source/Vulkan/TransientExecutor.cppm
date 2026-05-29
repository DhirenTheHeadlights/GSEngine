export module gse.vulkan:transient_executor;

import std;

import :device;
import :frame_recorder;
import :frame_resource_bin;
import :transient_queue;

import gse.concurrency;
import gse.core;

export namespace gse::vulkan {
	class transient_executor final : public non_copyable {
	public:
		~transient_executor();

		transient_executor(
			transient_executor&&
		) noexcept = default;

		auto operator=(
			transient_executor&&
		) noexcept -> transient_executor& = default;

		[[nodiscard]]
		static auto create(
			device& dev,
			std::uint32_t graphics_family,
			std::uint32_t compute_family,
			std::size_t worker_count
		) -> std::unique_ptr<transient_executor>;

		[[nodiscard]] auto recorder(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto bin(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto queue(
			queue_id id
		) -> transient_queue&;

		auto detach(
			async::task<> task
		) -> void;

		auto begin_frame() -> void;

		auto wait_idle() -> void;

	private:
		transient_executor(
			transient_queue&& graphics,
			transient_queue&& compute
		);

		frame_recorder m_recorder;
		frame_resource_bin m_bin;
		transient_queue m_graphics;
		transient_queue m_compute;
		std::vector<async::task<>> m_detached;
		std::unique_ptr<std::mutex> m_detached_mutex = std::make_unique<std::mutex>();
	};
}

gse::vulkan::transient_executor::transient_executor(transient_queue&& graphics, transient_queue&& compute)
	: m_graphics(std::move(graphics)), m_compute(std::move(compute)) {
}

gse::vulkan::transient_executor::~transient_executor() = default;

auto gse::vulkan::transient_executor::create(device& dev, const std::uint32_t graphics_family, const std::uint32_t compute_family, const std::size_t worker_count) -> std::unique_ptr<transient_executor> {
	auto graphics = transient_queue::create(dev, queue_id::graphics, graphics_family, worker_count);
	auto compute = transient_queue::create(dev, queue_id::compute, compute_family, worker_count);
	return std::unique_ptr<transient_executor>(new transient_executor(std::move(graphics), std::move(compute)));
}

auto gse::vulkan::transient_executor::recorder(this auto& self) -> auto& {
	return self.m_recorder;
}

auto gse::vulkan::transient_executor::bin(this auto& self) -> auto& {
	return self.m_bin;
}

auto gse::vulkan::transient_executor::queue(const queue_id id) -> transient_queue& {
	switch (id) {
		case queue_id::graphics: {
			return m_graphics;
		}
		case queue_id::compute: {
			return m_compute;
		}
	}
	return m_graphics;
}

auto gse::vulkan::transient_executor::detach(async::task<> task) -> void {
	std::lock_guard lock(*m_detached_mutex);
	m_detached.push_back(std::move(task));
}

auto gse::vulkan::transient_executor::begin_frame() -> void {
	const auto graphics_progress = m_graphics.poll();
	const auto compute_progress = m_compute.poll();

	const std::array<queue_progress, queue_id_count> progress{
		queue_progress{
			.queue = queue_id::graphics,
			.reached_value = graphics_progress,
		},
		queue_progress{
			.queue = queue_id::compute,
			.reached_value = compute_progress,
		},
	};

	m_bin.drain(progress);

	{
		std::lock_guard lock(*m_detached_mutex);
		std::erase_if(
			m_detached,
			[](const async::task<>& t) {
				return t.done();
			}
		);
	}
}

auto gse::vulkan::transient_executor::wait_idle() -> void {
	m_graphics.wait_idle();
	m_compute.wait_idle();
	{
		std::lock_guard lock(*m_detached_mutex);
		m_detached.clear();
	}
	m_bin.wait_idle_clear();
	m_recorder.clear();
}

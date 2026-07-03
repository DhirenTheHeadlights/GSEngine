export module gse.gpu_backend:transient;

import std;

import :core;
import :enums;
import :sync;
import :queue_timeline;
import :wait_station;
import :frame_recorder;
import :frame_resource_bin;

import gse.core;

export namespace gse::gpu {
	struct transient_pool_handle {
		std::uint32_t index = 0;
	};

	class transient_command_buffer final {
	public:
		transient_command_buffer() = default;

		transient_command_buffer(
			gpu::command_buffer_handle cmd,
			std::size_t worker_index
		);

		[[nodiscard]] auto handle() const -> gpu::command_buffer_handle;

		[[nodiscard]] auto worker_index() const -> std::size_t;

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

	template <typename Device>
	class transient_queue final : public non_copyable {
	public:
		struct submit_ticket {
			std::unique_lock<std::mutex> lock;
			std::uint64_t value = 0;
		};

		~transient_queue() = default;

		transient_queue(
			transient_queue&& other
		) noexcept;

		auto operator=(
			transient_queue&& other
		) noexcept -> transient_queue&;

		[[nodiscard]]
		static auto create(
			Device& dev,
			gpu::queue_id id,
			std::uint32_t family,
			std::size_t worker_count
		) -> transient_queue;

		[[nodiscard]] auto id() const -> gpu::queue_id;

		[[nodiscard]] auto reserve_for_submit() -> submit_ticket;

		[[nodiscard]] auto progress() const -> std::uint64_t;

		[[nodiscard]] auto reached(
			std::uint64_t value
		) const -> bool;

		[[nodiscard]] auto pending_value() const -> std::uint64_t;

		[[nodiscard]] auto timeline_handle() const -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto station() -> gpu::wait_station&;

		[[nodiscard]] auto allocate_primary(
			std::size_t worker_idx
		) -> transient_command_buffer;

		auto mark_in_use(
			std::size_t worker_idx,
			std::uint64_t value
		) -> void;

		auto park(
			std::uint64_t value,
			std::coroutine_handle<> handle
		) -> void;

		auto poll() -> std::uint64_t;

		auto wait_until(
			std::uint64_t value
		) -> void;

		auto wait_idle() -> void;

	private:
		transient_queue(
			gpu::queue_id id,
			gpu::queue_timeline<Device>&& timeline,
			std::vector<transient_pool_handle>&& pools,
			Device& dev
		);

		gpu::queue_id m_id;
		gpu::queue_timeline<Device> m_timeline;
		std::vector<transient_pool_handle> m_pool_handles;
		Device* m_device;
		std::uint64_t m_next_value = 0;
		gpu::wait_station m_station;
		std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>();
	};

	template <typename Device>
	class transient_executor final : public non_copyable {
	public:
		~transient_executor() = default;

		transient_executor(
			transient_executor&&
		) noexcept = default;

		auto operator=(
			transient_executor&&
		) noexcept -> transient_executor& = default;

		[[nodiscard]]
		static auto create(
			Device& dev,
			std::uint32_t graphics_family,
			std::uint32_t compute_family,
			std::size_t worker_count
		) -> std::unique_ptr<transient_executor>;

		[[nodiscard]] auto recorder() -> gpu::frame_recorder&;

		[[nodiscard]] auto bin() -> gpu::frame_resource_bin&;

		[[nodiscard]] auto queue(
			gpu::queue_id id
		) -> transient_queue<Device>&;

		auto begin_frame() -> void;

		auto wait_idle() -> void;

	private:
		transient_executor(
			transient_queue<Device>&& graphics,
			transient_queue<Device>&& compute
		);

		gpu::frame_recorder m_recorder;
		gpu::frame_resource_bin m_bin;
		transient_queue<Device> m_graphics;
		transient_queue<Device> m_compute;
	};
}

gse::gpu::transient_command_buffer::transient_command_buffer(const gpu::command_buffer_handle cmd, const std::size_t worker_index)
	: m_cmd(cmd), m_worker_index(worker_index) {
}

auto gse::gpu::transient_command_buffer::handle() const -> gpu::command_buffer_handle {
	return m_cmd;
}

auto gse::gpu::transient_command_buffer::worker_index() const -> std::size_t {
	return m_worker_index;
}

auto gse::gpu::transient_command_buffer::set_marker_seq(const std::uint64_t seq) -> void {
	m_marker_seq = seq;
}

auto gse::gpu::transient_command_buffer::marker_seq() const -> std::uint64_t {
	return m_marker_seq;
}

auto gse::gpu::transient_command_buffer::valid() const -> bool {
	return static_cast<bool>(m_cmd);
}

template <typename Device>
gse::gpu::transient_queue<Device>::transient_queue(gpu::queue_id id, gpu::queue_timeline<Device>&& timeline, std::vector<transient_pool_handle>&& pools, Device& dev)
	: m_id(id), m_timeline(std::move(timeline)), m_pool_handles(std::move(pools)), m_device(&dev) {
}

template <typename Device>
gse::gpu::transient_queue<Device>::transient_queue(transient_queue&& other) noexcept
	: m_id(other.m_id), m_timeline(std::move(other.m_timeline)), m_pool_handles(std::move(other.m_pool_handles)), m_device(other.m_device), m_next_value(other.m_next_value), m_station(std::move(other.m_station)), m_mutex(std::move(other.m_mutex)) {
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::operator=(transient_queue&& other) noexcept -> transient_queue& {
	if (this != &other) {
		m_id = other.m_id;
		m_timeline = std::move(other.m_timeline);
		m_pool_handles = std::move(other.m_pool_handles);
		m_device = other.m_device;
		m_next_value = other.m_next_value;
		m_station = std::move(other.m_station);
		m_mutex = std::move(other.m_mutex);
	}
	return *this;
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::create(Device& dev, const gpu::queue_id id, const std::uint32_t family, const std::size_t worker_count) -> transient_queue {
	std::vector<transient_pool_handle> pools;
	pools.reserve(worker_count);
	for (std::size_t i = 0; i < worker_count; ++i) {
		pools.push_back(dev.create_transient_command_pool(family));
	}
	return transient_queue(id, gpu::queue_timeline<Device>::create(dev), std::move(pools), dev);
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::id() const -> gpu::queue_id {
	return m_id;
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::reserve_for_submit() -> submit_ticket {
	std::unique_lock lock(*m_mutex);
	const auto value = ++m_next_value;
	return submit_ticket{
		.lock = std::move(lock),
		.value = value
	};
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::progress() const -> std::uint64_t {
	return m_station.progress();
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::reached(const std::uint64_t value) const -> bool {
	return m_station.reached(value);
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::pending_value() const -> std::uint64_t {
	std::lock_guard lock(*m_mutex);
	return m_next_value;
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::timeline_handle() const -> gpu::handle<gpu::semaphore> {
	return m_timeline.handle();
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::station() -> gpu::wait_station& {
	return m_station;
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::allocate_primary(const std::size_t worker_idx) -> transient_command_buffer {
	m_device->transient_pool_try_reset(m_pool_handles[worker_idx], m_station.progress());
	return transient_command_buffer(m_device->allocate_transient_primary(m_pool_handles[worker_idx]), worker_idx);
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::mark_in_use(const std::size_t worker_idx, const std::uint64_t value) -> void {
	m_device->transient_pool_mark_in_use(m_pool_handles[worker_idx], value);
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::park(const std::uint64_t value, std::coroutine_handle<> handle) -> void {
	m_station.park(value, handle);
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::poll() -> std::uint64_t {
	const auto reached_value = m_timeline.read();
	m_station.advance(reached_value);
	return reached_value;
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::wait_until(const std::uint64_t value) -> void {
	if (m_station.reached(value)) {
		return;
	}
	m_timeline.wait_until(value);
	m_station.advance(value);
}

template <typename Device>
auto gse::gpu::transient_queue<Device>::wait_idle() -> void {
	std::uint64_t target;
	{
		std::lock_guard lock(*m_mutex);
		if (m_next_value == 0) {
			return;
		}
		target = m_next_value;
	}
	wait_until(target);

	for (const auto handle : m_pool_handles) {
		m_device->transient_pool_reset_all(handle);
	}
}

template <typename Device>
gse::gpu::transient_executor<Device>::transient_executor(transient_queue<Device>&& graphics, transient_queue<Device>&& compute)
	: m_graphics(std::move(graphics)), m_compute(std::move(compute)) {
}

template <typename Device>
auto gse::gpu::transient_executor<Device>::create(Device& dev, const std::uint32_t graphics_family, const std::uint32_t compute_family, const std::size_t worker_count) -> std::unique_ptr<transient_executor> {
	auto graphics = transient_queue<Device>::create(dev, gpu::queue_id::graphics, graphics_family, worker_count);
	auto compute = transient_queue<Device>::create(dev, gpu::queue_id::compute, compute_family, worker_count);
	return std::unique_ptr<transient_executor>(new transient_executor(std::move(graphics), std::move(compute)));
}

template <typename Device>
auto gse::gpu::transient_executor<Device>::recorder() -> gpu::frame_recorder& {
	return m_recorder;
}

template <typename Device>
auto gse::gpu::transient_executor<Device>::bin() -> gpu::frame_resource_bin& {
	return m_bin;
}

template <typename Device>
auto gse::gpu::transient_executor<Device>::queue(const gpu::queue_id id) -> transient_queue<Device>& {
	switch (id) {
		case gpu::queue_id::graphics: {
			return m_graphics;
		}
		case gpu::queue_id::compute: {
			return m_compute;
		}
	}
	return m_graphics;
}

template <typename Device>
auto gse::gpu::transient_executor<Device>::begin_frame() -> void {
	const auto graphics_progress = m_graphics.poll();
	const auto compute_progress = m_compute.poll();

	const std::array<gpu::queue_progress, gpu::queue_id_count> progress{
		gpu::queue_progress{
			.queue = gpu::queue_id::graphics,
			.reached_value = graphics_progress,
		},
		gpu::queue_progress{
			.queue = gpu::queue_id::compute,
			.reached_value = compute_progress,
		},
	};

	m_bin.drain(progress);
}

template <typename Device>
auto gse::gpu::transient_executor<Device>::wait_idle() -> void {
	m_graphics.wait_idle();
	m_compute.wait_idle();
	m_bin.wait_idle_clear();
	m_recorder.clear();
}

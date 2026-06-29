export module gse.dx12:command_pools;

import std;
import gse.gpu_backend;
import gse.directx;

import :device;

export namespace gse::dx12 {
	class command_pools {
	public:
		auto bind(
			device* owner
		) -> void;

		auto reset_worker_command_pools(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_worker_command_buffer(
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> gpu::command_buffer_handle;

		auto begin_one_time_commands(
			gpu::command_buffer_handle cmd
		) -> void;

		auto end_commands(
			gpu::command_buffer_handle cmd
		) -> void;

		[[nodiscard]] auto create_transient_command_pool() -> gpu::transient_pool_handle;

		[[nodiscard]] auto allocate_transient_primary(
			gpu::transient_pool_handle pool
		) -> gpu::command_buffer_handle;

		auto transient_pool_try_reset(
			gpu::transient_pool_handle pool,
			std::uint64_t queue_progress
		) -> void;

		auto transient_pool_mark_in_use(
			gpu::transient_pool_handle pool,
			std::uint64_t value
		) -> void;

		auto transient_pool_reset_all(
			gpu::transient_pool_handle pool
		) -> void;

	private:
		struct entry {
			directx::com_ptr<directx::ID3D12CommandAllocator> allocator;
			directx::com_ptr<directx::ID3D12GraphicsCommandList> list;
		};

		struct list_pool {
			std::vector<entry> entries;
			std::size_t used = 0;
			std::uint64_t high_water = 0;
		};

		device* m_owner = nullptr;
		std::vector<list_pool> m_worker_lists;
		std::vector<list_pool> m_transient_pools;
		std::mutex m_mutex;
	};
}

auto gse::dx12::command_pools::bind(device* owner) -> void {
	m_owner = owner;
	m_worker_lists.resize(gpu::max_frames_in_flight);
}

auto gse::dx12::command_pools::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	const std::lock_guard lock(m_mutex);
	m_worker_lists[frame_index % m_worker_lists.size()].used = 0;
}

auto gse::dx12::command_pools::acquire_worker_command_buffer(std::size_t, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	const std::lock_guard lock(m_mutex);
	auto& p = m_worker_lists[frame_index % m_worker_lists.size()];
	if (p.used == p.entries.size()) {
		auto allocator = directx::create_command_allocator(m_owner->raw_device());
		auto list = directx::create_command_list(m_owner->raw_device(), allocator.get());
		p.entries.push_back(entry{
			.allocator = std::move(allocator),
			.list = std::move(list),
		});
	}
	auto& e = p.entries[p.used++];
	e.allocator->Reset();
	e.list->Reset(e.allocator.get(), nullptr);
	m_owner->reset_acquired_list(e.list.get());
	return std::bit_cast<gpu::command_buffer_handle>(e.list.get());
}

auto gse::dx12::command_pools::begin_one_time_commands(gpu::command_buffer_handle) -> void {}

auto gse::dx12::command_pools::end_commands(const gpu::command_buffer_handle cmd) -> void {
	if (auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)) {
		list->Close();
	}
}

auto gse::dx12::command_pools::create_transient_command_pool() -> gpu::transient_pool_handle {
	const auto index = static_cast<std::uint32_t>(m_transient_pools.size());
	m_transient_pools.emplace_back();
	return gpu::transient_pool_handle{ .index = index };
}

auto gse::dx12::command_pools::allocate_transient_primary(const gpu::transient_pool_handle pool) -> gpu::command_buffer_handle {
	auto& p = m_transient_pools[pool.index];
	if (p.used == p.entries.size()) {
		auto allocator = directx::create_command_allocator(m_owner->raw_device());
		auto list = directx::create_command_list(m_owner->raw_device(), allocator.get());
		p.entries.push_back(entry{
			.allocator = std::move(allocator),
			.list = std::move(list),
		});
	}
	auto& e = p.entries[p.used++];
	if (!e.list || !e.allocator) {
		return {};
	}
	e.list->Reset(e.allocator.get(), nullptr);
	return std::bit_cast<gpu::command_buffer_handle>(e.list.get());
}

auto gse::dx12::command_pools::transient_pool_try_reset(const gpu::transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	auto& p = m_transient_pools[pool.index];
	if (p.used == 0 || queue_progress < p.high_water) {
		return;
	}
	for (std::size_t i = 0; i < p.used; ++i) {
		p.entries[i].allocator->Reset();
	}
	p.used = 0;
}

auto gse::dx12::command_pools::transient_pool_mark_in_use(const gpu::transient_pool_handle pool, const std::uint64_t value) -> void {
	auto& p = m_transient_pools[pool.index];
	if (value > p.high_water) {
		p.high_water = value;
	}
}

auto gse::dx12::command_pools::transient_pool_reset_all(const gpu::transient_pool_handle pool) -> void {
	auto& p = m_transient_pools[pool.index];
	for (std::size_t i = 0; i < p.used; ++i) {
		p.entries[i].allocator->Reset();
	}
	p.used = 0;
}

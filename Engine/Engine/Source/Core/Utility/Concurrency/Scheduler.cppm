export module gse.utility:scheduler;

import std;

import :phase_context;
import :system_node;
import :registry;
import :task;
import :system_clock;
import :frame_sync;
import :trace;

import gse.assert;

export namespace gse {
	class scheduler final : public state_snapshot_provider, public channel_reader_provider, public system_provider {
	public:
		scheduler() = default;

		auto system_ptr(
			std::type_index idx
		) -> void* override;

		auto system_ptr(
			std::type_index idx
		) const -> const void* override;

		template <typename S, typename State, typename... Args>
		auto add_system(
			registry& reg,
			Args&&... args
		) -> State&;

		template <typename State>
		auto state(
		) -> State&;

		template <typename State>
		auto state(
		) const -> const State&;

		template <typename State>
		auto has(
		) const -> bool;

		auto snapshot_ptr(
			std::type_index type
		) const -> const void* override;

		auto channel_snapshot_ptr(
			std::type_index type
		) const -> const void* override;

		auto ensure_channel(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base& override;

		template <typename T>
		auto channel(
		) -> channel<T>&;

		auto initialize(
		) -> void;

		auto update(
		) -> void;

		auto render(
			const std::function<void()>& in_frame = {}
		) -> void;

		auto shutdown(
		) -> void;

		auto clear(
		) -> void;

		auto registry_access_mut(
		) -> registry_access&;
	private:
		std::vector<std::unique_ptr<system_node_base>> m_nodes;
		std::unordered_map<std::type_index, system_node_base*> m_state_index;
		std::unordered_map<std::type_index, std::unique_ptr<channel_base>> m_channels;
		mutable std::mutex m_channels_mutex;
		registry* m_registry = nullptr;
		registry_access m_registry_access{};

		auto snapshot_all_channels(
		) -> void;

		auto ensure_channel_internal(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base&;

		auto make_channel_writer(
		) -> channel_writer;

		static auto build_work_batches(
			std::vector<queued_work>& work
		) -> std::vector<std::vector<queued_work*>>;
	};
}

template <typename S, typename State, typename... Args>
auto gse::scheduler::add_system(registry& reg, Args&&... args) -> State& {
	if (m_registry == nullptr) {
		m_registry = &reg;
		m_registry_access.reg = m_registry;
	}

	auto ptr = std::make_unique<system_node<S, State>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();

	m_state_index.emplace(std::type_index(typeid(State)), raw);
	m_nodes.push_back(std::move(ptr));

	return raw->state();
}

template <typename State>
auto gse::scheduler::state() -> State& {
	const auto it = m_state_index.find(std::type_index(typeid(State)));
	assert(it != m_state_index.end(), std::source_location::current(), "state not found");
	return *static_cast<State*>(it->second->state_ptr());
}

template <typename State>
auto gse::scheduler::state() const -> const State& {
	const auto it = m_state_index.find(std::type_index(typeid(State)));
	assert(it != m_state_index.end(), std::source_location::current(), "state not found");
	return *static_cast<const State*>(it->second->state_ptr());
}

template <typename State>
auto gse::scheduler::has() const -> bool {
	return m_state_index.contains(std::type_index(typeid(State)));
}

auto gse::scheduler::snapshot_ptr(const std::type_index type) const -> const void* {
	const auto it = m_state_index.find(type);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::system_ptr(const std::type_index idx) -> void* {
	const auto it = m_state_index.find(idx);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::system_ptr(const std::type_index idx) const -> const void* {
	const auto it = m_state_index.find(idx);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::channel_snapshot_ptr(const std::type_index type) const -> const void* {
	std::lock_guard lock(m_channels_mutex);
	const auto it = m_channels.find(type);
	if (it == m_channels.end()) {
		return nullptr;
	}
	return it->second->snapshot_data();
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	auto& base = ensure_channel_internal(std::type_index(typeid(T)), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});

	return static_cast<typed_channel<T>&>(base).data;
}

auto gse::scheduler::initialize() -> void {
	frame_sync::on_begin([this] {
		snapshot_all_channels();
	});

	auto writer = make_channel_writer();

	initialize_phase phase{
		.registry = m_registry_access,
		.snapshots = *this,
		.channels = writer
	};

	for (const auto& n : m_nodes) {
		n->initialize(phase);
	}
}

auto gse::scheduler::update() -> void {
	auto writer = make_channel_writer();
	work_queue work;

	const registry_access const_registry_access = m_registry_access;

	update_phase phase{
		.registry = const_registry_access,
		.snapshots = *this,
		.channels = writer,
		.channel_reader = *this,
		.work = work
	};

	for (const auto& n : m_nodes) {
		n->update(phase);
	}

	if (work.work().empty()) {
		return;
	}

	for (auto& batch : build_work_batches(work.work())) {
		if (batch.size() == 1) {
			trace::scope(batch[0]->name, [&] {
				batch[0]->execute(*m_registry);
			});
		}
		else {
			task::parallel_for(0uz, batch.size(), [&](const size_t i) {
				trace::scope(batch[i]->name, [&] {
					batch[i]->execute(*m_registry);
				});
			});
		}
	}
}

auto gse::scheduler::render(const std::function<void()>& in_frame) -> void {
	begin_frame_phase bf_phase{
		.snapshots = *this
	};

	std::vector started(m_nodes.size(), false);

	if (!m_nodes.empty()) {
		started[0] = m_nodes[0]->begin_frame(bf_phase);
		if (!started[0]) {
			return;
		}

		for (auto i : std::views::iota(std::size_t{1}, m_nodes.size())) {
			started[i] = m_nodes[i]->begin_frame(bf_phase);
		}
	}

	const registry_access const_reg_access = m_registry_access;

	render_phase r_phase{
		.registry = const_reg_access,
		.snapshots = *this,
		.channel_reader = *this
	};

	for (const auto& n : m_nodes) {
		n->render(r_phase);
	}

	if (in_frame) {
		in_frame();
	}

	auto ef_writer = make_channel_writer();

	end_frame_phase ef_phase{
		.snapshots = *this,
		.channels = ef_writer
	};

	for (std::size_t i = m_nodes.size(); i-- > 0;) {
		if (started[i]) {
			m_nodes[i]->end_frame(ef_phase);
		}
	}
}

auto gse::scheduler::shutdown() -> void {
	shutdown_phase phase{
		.registry = m_registry_access
	};

	for (auto& node : m_nodes | std::views::reverse) {
		node->shutdown(phase);
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_state_index.clear();
	m_channels.clear();
}

auto gse::scheduler::registry_access_mut() -> registry_access& {
	return m_registry_access;
}

auto gse::scheduler::snapshot_all_channels() -> void {
	std::lock_guard lock(m_channels_mutex);
	for (const auto& ch_ptr : m_channels | std::views::values) {
		ch_ptr->take_snapshot();
	}
}

auto gse::scheduler::ensure_channel_internal(const std::type_index idx, const channel_factory_fn factory) -> channel_base& {
	std::lock_guard lock(m_channels_mutex);
	auto it = m_channels.find(idx);

	if (it == m_channels.end()) {
		it = m_channels.emplace(idx, factory()).first;
	}

	return *it->second;
}

auto gse::scheduler::ensure_channel(const std::type_index idx, const channel_factory_fn factory) -> channel_base& {
	return ensure_channel_internal(idx, factory);
}

auto gse::scheduler::make_channel_writer() -> channel_writer {
	return channel_writer([this](const std::type_index type, std::any item, const channel_factory_fn factory) {
		std::lock_guard lock(m_channels_mutex);
		auto it = m_channels.find(type);
		if (it == m_channels.end()) {
			it = m_channels.emplace(type, factory()).first;
		}
		it->second->push_any(std::move(item));
	});
}

auto gse::scheduler::build_work_batches(std::vector<queued_work>& work) -> std::vector<std::vector<queued_work*>> {
	std::vector<std::vector<queued_work*>> batches;

	for (auto& w : work) {
		bool placed = false;

		for (auto& batch : batches) {
			bool conflicts = false;

			for (const auto* existing : batch) {
				if (w.conflicts_with(*existing)) {
					conflicts = true;
					break;
				}
			}

			if (!conflicts) {
				batch.push_back(&w);
				placed = true;
				break;
			}
		}

		if (!placed) {
			batches.push_back({ &w });
		}
	}

	return batches;
}

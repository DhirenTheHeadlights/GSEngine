export module gse.utility:scheduler;

import std;

import :system;
import :registry;
import :task;

import gse.assert;

export namespace gse {
	template <typename T>
	concept is_system = std::derived_from<T, system>;

	class scheduler : public system_provider {
	public:
		scheduler(
		) = default;

		template <is_system S, typename... Args>
		auto emplace(
			registry& reg,
			Args&&... args
		) -> S&;

		template <is_system S>
		auto get(
		) -> S&;

		template <is_system S>
		auto get(
		) const -> const S&;

		template <is_system S>
		auto has(
		) const -> bool;

		auto system_ptr(
			std::type_index idx
		) -> void* override;

		auto system_ptr(
			std::type_index idx
		) const -> const void* override;

		auto ensure_channel(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base& override;

		template <typename T>
		auto channel(
		) -> channel<T>&;

		auto initialize(
		) const -> void;

		auto update(
		) -> void;

		auto render(
			const std::function<void()>& in_frame = {}
		) -> void;

		auto shutdown(
		) -> void;

		auto flush_deferred(
		) const -> void;

		auto clear(
		) -> void;
	private:
		struct node_base {
			virtual ~node_base(
			) = default;

			virtual auto ptr(
			) -> void* = 0;

			virtual auto ptr(
			) const -> const void* = 0;

			virtual auto initialize(
			) -> void = 0;

			virtual auto update(
			) -> void = 0;

			virtual auto render(
			) -> void = 0;

			virtual auto shutdown(
			) -> void = 0;

			virtual auto begin_frame(
			) -> bool = 0;

			virtual auto end_frame(
			) -> void = 0;

			virtual auto take_pending(
			) -> std::vector<pending_work> = 0;

			virtual auto flush_deferred(
			) -> void = 0;
		};

		template <is_system S>
		struct node final : node_base {
			S sys;

			template <typename... Args>
			explicit node(
				Args&&... args
			);

			auto ptr(
			) -> void* override;

			auto ptr(
			) const -> const void* override;

			auto initialize(
			) -> void override;

			auto update(
			) -> void override;

			auto render(
			) -> void override;

			auto shutdown(
			) -> void override;

			auto begin_frame(
			) -> bool override;

			auto end_frame(
			) -> void override;

			auto take_pending(
			) -> std::vector<pending_work> override;

			auto flush_deferred(
			) -> void override;
		};

		std::vector<std::unique_ptr<node_base>> m_nodes;
		std::unordered_map<std::type_index, node_base*> m_index;
		std::unordered_map<std::type_index, std::unique_ptr<channel_base>> m_channels;

		auto run_stage(
			void (node_base::*stage_fn)()
		) const -> void;

		static auto conflicts(
			const pending_work& a,
			const pending_work& b
		) -> bool;

		static auto build_phases(
			std::vector<pending_work>& work
		) -> std::vector<std::vector<pending_work*>>;

		static auto execute_phase(
			const std::vector<pending_work*>& phase
		) -> void;
	};
}

template <gse::is_system S, typename... Args>
auto gse::scheduler::emplace(registry& reg, Args&&... args) -> S& {
	auto ptr = std::make_unique<node<S>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();

	raw->sys.m_registry = &reg;
	raw->sys.m_scheduler = this;

	m_index.emplace(std::type_index(typeid(S)), raw);
	m_nodes.push_back(std::move(ptr));

	return raw->sys;
}

template <gse::is_system S>
auto gse::scheduler::get() -> S& {
	const auto it = m_index.find(std::type_index(typeid(S)));
	assert(it != m_index.end(), std::source_location::current(), "system not found");
	return static_cast<node<S>*>(it->second)->sys;
}

template <gse::is_system S>
auto gse::scheduler::get() const -> const S& {
	const auto it = m_index.find(std::type_index(typeid(S)));
	assert(it != m_index.end(), std::source_location::current(), "system not found");
	return static_cast<const node<S>*>(it->second)->sys;
}

template <gse::is_system S>
auto gse::scheduler::has() const -> bool {
	return m_index.contains(std::type_index(typeid(S)));
}

auto gse::scheduler::system_ptr(const std::type_index idx) -> void* {
	const auto it = m_index.find(idx);
	if (it == m_index.end()) {
		return nullptr;
	}
	return it->second->ptr();
}

auto gse::scheduler::system_ptr(const std::type_index idx) const -> const void* {
	const auto it = m_index.find(idx);
	if (it == m_index.end()) {
		return nullptr;
	}
	return it->second->ptr();
}

auto gse::scheduler::ensure_channel(const std::type_index idx, const channel_factory_fn factory) -> channel_base& {
	auto it = m_channels.find(idx);

	if (it == m_channels.end()) {
		it = m_channels.emplace(idx, factory()).first;
	}

	return *it->second;
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	auto& base = ensure_channel(std::type_index(typeid(T)), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});

	return static_cast<typed_channel<T>&>(base).data;
}

auto gse::scheduler::initialize() const -> void {
	for (auto& n : m_nodes) {
		n->initialize();
	}
}

auto gse::scheduler::update() -> void {
	run_stage(&node_base::update);
}

auto gse::scheduler::render(const std::function<void()>& in_frame) -> void {
    std::vector started(m_nodes.size(), false);

    if (!m_nodes.empty()) {
        started[0] = m_nodes[0]->begin_frame();
        if (!started[0]) return;

        for (std::size_t i = 1; i < m_nodes.size(); ++i) {
            started[i] = m_nodes[i]->begin_frame();
        }
    }

    run_stage(&node_base::render);

    if (in_frame) {
        in_frame();
    }

    for (std::size_t i = m_nodes.size(); i-- > 0;) {
        if (started[i]) {
            m_nodes[i]->end_frame();
        }
    }

	std::vector<pending_work> all_work;

	for (const auto& n : m_nodes) {
		for (auto work = n->take_pending(); auto& w : work) {
			all_work.push_back(std::move(w));
		}
	}

	if (all_work.empty()) {
		return;
	}

	for (const auto phases = build_phases(all_work); auto& phase : phases) {
		execute_phase(phase);
	}
}

auto gse::scheduler::shutdown() -> void {
	for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
		(*it)->shutdown();
	}
}

auto gse::scheduler::flush_deferred() const -> void {
	for (auto& n : m_nodes) {
		n->flush_deferred();
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_index.clear();
	m_channels.clear();
}

template <gse::is_system S>
template <typename... Args>
gse::scheduler::node<S>::node(Args&&... args) : sys(std::forward<Args>(args)...) {}

template <gse::is_system S>
auto gse::scheduler::node<S>::ptr() -> void* {
	return &sys;
}

template <gse::is_system S>
auto gse::scheduler::node<S>::ptr() const -> const void* {
	return &sys;
}

template <gse::is_system S>
auto gse::scheduler::node<S>::initialize() -> void {
	sys.initialize();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::update() -> void {
	sys.update();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::render() -> void {
	sys.render();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::shutdown() -> void {
	sys.shutdown();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::begin_frame() -> bool {
	return sys.begin_frame();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::end_frame() -> void {
	sys.end_frame();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::take_pending() -> std::vector<pending_work> {
	return sys.take_pending();
}

template <gse::is_system S>
auto gse::scheduler::node<S>::flush_deferred() -> void {
	sys.flush_deferred();
}

auto gse::scheduler::run_stage(void (node_base::*stage_fn)()) const -> void {
	for (auto& n : m_nodes) {
		(n.get()->*stage_fn)();
	}

	std::vector<pending_work> all_work;

	for (const auto& n : m_nodes) {
		for (auto work = n->take_pending(); auto& w : work) {
			all_work.push_back(std::move(w));
		}
	}

	if (all_work.empty()) {
		return;
	}

	for (const auto phases = build_phases(all_work); auto& phase : phases) {
		execute_phase(phase);
	}
}

auto gse::scheduler::conflicts(const pending_work& a, const pending_work& b) -> bool {
	for (const auto& wa : a.component_writes) {
		for (const auto& wb : b.component_writes) {
			if (wa == wb) {
				return true;
			}
		}
	}

	if (a.channel_write != typeid(void) && a.channel_write == b.channel_write) {
		return true;
	}

	return false;
}

auto gse::scheduler::build_phases(std::vector<pending_work>& work) -> std::vector<std::vector<pending_work*>> {
	std::vector<std::vector<pending_work*>> phases;

	for (auto& w : work) {
		bool placed = false;

		for (auto& phase : phases) {
			bool ok = true;

			for (const auto* existing : phase) {
				if (conflicts(*existing, w)) {
					ok = false;
					break;
				}
			}

			if (ok) {
				phase.push_back(&w);
				placed = true;
				break;
			}
		}

		if (!placed) {
			phases.push_back({ &w });
		}
	}

	return phases;
}

auto gse::scheduler::execute_phase(const std::vector<pending_work*>& phase) -> void {
	if (phase.empty()) {
		return;
	}

	if (phase.size() == 1) {
		phase[0]->execute();
		return;
	}

	task::group group;

	for (auto* w : phase) {
		group.post([w] {
			w->execute();
		});
	}
}

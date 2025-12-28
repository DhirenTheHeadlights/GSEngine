export module gse.utility:scheduler;

import std;

import :system;
import :task;
import gse.assert;

export namespace gse {
	class scheduler : public system_provider {
	public:
		scheduler();

		template <scheduled_system S, typename... Args>
		auto emplace_system(
			Args&&... args
		) -> S&;

		template <scheduled_system S>
		auto has_system(
		) const -> bool;

		template <scheduled_system S>
		auto system(
		) -> S&;

		template <scheduled_system S>
		auto system(
		) const -> const S&;

		auto system(
			std::type_index idx
		) -> void* override;

		auto system(
			std::type_index idx
		) const -> const void* override;

		auto build(
		) -> void;

		auto run_initialize(
		) const -> void;

		auto run_update(
		) const -> void;

		auto run_render(
		) const -> void;

		auto run_shutdown(
		) const -> void;

		auto clear(
		) -> void;
	private:
		class node_base {
		public:
			virtual ~node_base(
			) = default;

			virtual auto system_ptr(
			) -> void* = 0;

			virtual auto system_ptr(
			) const -> const void* = 0;

			virtual auto begin_frame(
			) -> bool = 0;

			virtual auto end_frame(
			) -> void = 0;
		};

		template <scheduled_system S>
		class node final : public node_base {
		public:
			template <typename... Args>
			explicit node(
				Args&&... args
			) : system(std::forward<Args>(args)...) {
			}

			auto system_ptr(
			) -> void* override {
				return std::addressof(system);
			}

			auto system_ptr(
			) const -> const void* override {
				return std::addressof(system);
			}

			auto begin_frame(
			) -> bool override {
				if constexpr (requires(S& s) { { s.begin_frame() } -> std::same_as<bool>; }) {
					return system.begin_frame();
				}
				return true;
			}

			auto end_frame(
			) -> void override {
				if constexpr (requires(S& s) { s.end_frame(); }) {
					system.end_frame();
				}
			}

			S system;
		};

		struct stage_node {
			node_base* owner = nullptr;
			void (*run)(
				node_base&
			) = nullptr;
			std::vector<std::type_index> reads;
			std::vector<std::type_index> writes;
		};

		template <typename Set>
		struct type_indices;

		template <is_component... Cs>
		struct type_indices<read_set<Cs...>> {
			static auto make(
			) -> std::vector<std::type_index> {
				std::vector<std::type_index> v;
				v.reserve(sizeof...(Cs));
				(v.push_back(std::type_index(typeid(Cs))), ...);
				return v;
			}
		};

		template <is_component... Cs>
		struct type_indices<write_set<Cs...>> {
			static auto make(
			) -> std::vector<std::type_index> {
				std::vector<std::type_index> v;
				v.reserve(sizeof...(Cs));
				(v.push_back(std::type_index(typeid(Cs))), ...);
				return v;
			}
		};

		static constexpr std::size_t stage_count = 4;

		std::vector<std::unique_ptr<node_base>> m_nodes;
		std::array<std::vector<stage_node>, stage_count> m_stage_nodes;
		std::array<std::vector<std::vector<stage_node*>>, stage_count> m_phase_nodes;
		std::unordered_map<std::type_index, node_base*> m_system_index;

		template <scheduled_system S>
		auto register_system_stages(
			node<S>& n
		) -> void;

		template <scheduled_system S, std::size_t... Is>
		auto register_system_stages_impl(
			node<S>& n,
			std::index_sequence<Is...>
		) -> void;

		template <scheduled_system S, typename StageDesc>
		auto register_single_stage(
			node<S>& n
		) -> void;

		static auto to_index(
			system_stage_kind k
		) -> std::size_t;

		static auto conflicts(
			const stage_node& a,
			const stage_node& b
		) -> bool;

		static auto build_phases(
			std::span<stage_node> nodes
		) -> std::vector<std::vector<stage_node*>>;

		template <typename Runner>
		static auto run_phase_set(
			const std::vector<std::vector<stage_node*>>& phases,
			Runner&& runner
		) -> void;

		auto run_stage(
			system_stage_kind k
		) const -> void;

		template <system_stage_kind Kind, typename S>
		static auto call_stage(
			S& s
		) -> void;
	};
}

gse::scheduler::scheduler() {
	system_access::set_system_provider(this);
}

template <gse::scheduled_system S, typename... Args>
auto gse::scheduler::emplace_system(Args&&... args) -> S& {
	auto ptr = std::make_unique<node<S>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();
	m_system_index.emplace(std::type_index(typeid(S)), raw);
	m_nodes.push_back(std::move(ptr));
	register_system_stages(*static_cast<node<S>*>(raw));
	return static_cast<node<S>*>(raw)->system;
}

template <gse::scheduled_system S>
auto gse::scheduler::has_system() const -> bool {
	return m_system_index.contains(std::type_index(typeid(S)));
}

template <gse::scheduled_system S>
auto gse::scheduler::system() -> S& {
	const auto it = m_system_index.find(std::type_index(typeid(S)));
	assert(it != m_system_index.end(), std::source_location::current(), "System not found");
	auto* base = it->second;
	auto* typed = dynamic_cast<node<S>*>(base);
	assert(typed, std::source_location::current(), "System type mismatch");
	return typed->system;
}

template <gse::scheduled_system S>
auto gse::scheduler::system() const -> const S& {
	const auto it = m_system_index.find(std::type_index(typeid(S)));
	assert(it != m_system_index.end(), std::source_location::current(), "System not found");
	auto* base = it->second;
	auto* typed = dynamic_cast<const node<S>*>(base);
	assert(typed, std::source_location::current(), "System type mismatch");
	return typed->system;
}

auto gse::scheduler::system(const std::type_index idx) -> void* {
	const auto it = m_system_index.find(idx);
	assert(it != m_system_index.end(), std::source_location::current(), "System not found");
	return it->second->system_ptr();
}

auto gse::scheduler::system(const std::type_index idx) const -> const void* {
	const auto it = m_system_index.find(idx);
	assert(it != m_system_index.end(), std::source_location::current(), "System not found");
	return it->second->system_ptr();
}

template <gse::scheduled_system S>
auto gse::scheduler::register_system_stages(node<S>& n) -> void {
	using schedule = S::schedule;
	using stages_tuple = schedule::stages;
	constexpr std::size_t n_stages = std::tuple_size_v<stages_tuple>;
	register_system_stages_impl<S>(n, std::make_index_sequence<n_stages>{});
}

template <gse::scheduled_system S, std::size_t... Is>
auto gse::scheduler::register_system_stages_impl(node<S>& n, std::index_sequence<Is...>) -> void {
	(register_single_stage<S, std::tuple_element_t<Is, typename S::schedule::stages>>(n), ...);
}

template <gse::scheduled_system S, typename StageDesc>
auto gse::scheduler::register_single_stage(node<S>& n) -> void {
	constexpr auto k = StageDesc::stage;
	const auto idx = to_index(k);

	stage_node sn;
	sn.owner = &n;

	sn.run = [](node_base& base) {
		auto& typed = static_cast<node<S>&>(base);
		call_stage<StageDesc::stage>(typed.system);
	};

	sn.reads = type_indices<typename StageDesc::read_set>::make();
	sn.writes = type_indices<typename StageDesc::write_set>::make();

	m_stage_nodes[idx].push_back(std::move(sn));
}

auto gse::scheduler::to_index(const system_stage_kind k) -> std::size_t {
	switch (k) {
		case system_stage_kind::initialize: return 0;
		case system_stage_kind::update: return 1;
		case system_stage_kind::render: return 2;
		case system_stage_kind::shutdown: return 3;
	}
	return 0;
}

auto gse::scheduler::conflicts(const stage_node& a, const stage_node& b) -> bool {
	auto any_overlap = [](const std::vector<std::type_index>& x, const std::vector<std::type_index>& y) -> bool {
		for (const auto& xi : x) {
			for (const auto& yi : y) {
				if (xi == yi) {
					return true;
				}
			}
		}
		return false;
	};

	if (any_overlap(a.writes, b.writes)) return true;
	if (any_overlap(a.writes, b.reads)) return true;
	if (any_overlap(a.reads, b.writes)) return true;

	return false;
}

auto gse::scheduler::build_phases(std::span<stage_node> nodes) -> std::vector<std::vector<stage_node*>> {
	std::vector<std::vector<stage_node*>> phases;

	for (auto& n : nodes) {
		stage_node* np = std::addressof(n);

		bool placed = false;

		for (auto& phase : phases) {
			bool ok = true;
			for (auto* existing : phase) {
				if (conflicts(*existing, *np)) {
					ok = false;
					break;
				}
			}
			if (ok) {
				phase.push_back(np);
				placed = true;
				break;
			}
		}

		if (!placed) {
			phases.push_back({ np });
		}
	}

	return phases;
}

template <typename Runner>
auto gse::scheduler::run_phase_set(
	const std::vector<std::vector<stage_node*>>& phases,
	Runner&& runner
) -> void {
	for (const auto& phase : phases) {
		for (auto* n : phase) {
			runner(*n);
		}
	}
}

auto gse::scheduler::build() -> void {
	for (std::size_t i = 0; i < stage_count; ++i) {
		m_phase_nodes[i] = build_phases(std::span(m_stage_nodes[i]));
	}
}

auto gse::scheduler::run_stage(const system_stage_kind k) const -> void {
	const auto idx = to_index(k);

	run_phase_set(
		m_phase_nodes[idx],
		[](stage_node& n) {
			n.run(*n.owner);
		}
	);
}

auto gse::scheduler::run_initialize() const -> void {
	run_stage(system_stage_kind::initialize);
}

auto gse::scheduler::run_update() const -> void {
	run_stage(system_stage_kind::update);
}

auto gse::scheduler::run_render() const -> void {
	if (m_nodes.empty()) {
		return;
	}

	std::vector started(m_nodes.size(), false);

	const bool frame_ok = m_nodes[0]->begin_frame();
	started[0] = frame_ok;

	if (!frame_ok) {
		for (std::size_t i = m_nodes.size(); i-- > 0;) {
			if (started[i]) {
				m_nodes[i]->end_frame();
			}
		}
		return;
	}

	for (std::size_t i = 1; i < m_nodes.size(); ++i) {
		started[i] = m_nodes[i]->begin_frame();
	}

	run_stage(system_stage_kind::render);

	for (std::size_t i = m_nodes.size(); i-- > 0;) {
		if (started[i]) {
			m_nodes[i]->end_frame();
		}
	}
}

auto gse::scheduler::run_shutdown() const -> void {
	const auto idx = to_index(system_stage_kind::shutdown);
	for (auto it = m_stage_nodes[idx].rbegin(); it != m_stage_nodes[idx].rend(); ++it) {
		it->run(*it->owner);
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_system_index.clear();

	for (auto& v : m_stage_nodes) {
		v.clear();
	}

	for (auto& v : m_phase_nodes) {
		v.clear();
	}
}

template <gse::system_stage_kind Kind, typename S>
auto gse::scheduler::call_stage(S& s) -> void {
	if constexpr (Kind == system_stage_kind::initialize) {
		s.initialize();
	} else if constexpr (Kind == system_stage_kind::update) {
		s.update();
	} else if constexpr (Kind == system_stage_kind::render) {
		s.render();
	} else if constexpr (Kind == system_stage_kind::shutdown) {
		s.shutdown();
	}
}

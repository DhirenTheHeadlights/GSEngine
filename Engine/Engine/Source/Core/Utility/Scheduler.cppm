export module gse.utility:scheduler;

import std;

import :system;
import :task;

export namespace gse {
	class scheduler {
	public:
		template <scheduled_system S, typename... Args>
		auto emplace_system(
			Args&&... args
		) -> S&;

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
		};

		template <scheduled_system S>
		class node final : public node_base {
		public:
			template <typename... Args>
			explicit node(
				Args&&... args
			) : system(std::forward<Args>(args)...) {
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
	};
}

template <gse::scheduled_system S, typename... Args>
auto gse::scheduler::emplace_system(Args&&... args) -> S& {
	auto ptr = std::make_unique<node<S>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();
	m_nodes.push_back(std::move(ptr));
	register_system_stages(*raw);
	return raw->system;
}

auto gse::scheduler::build() -> void {
	for (std::size_t i = 0; i < stage_count; ++i) {
		const auto span = std::span(m_stage_nodes[i].data(), m_stage_nodes[i].size());
		m_phase_nodes[i] = build_phases(span);
	}
}

auto gse::scheduler::run_initialize() const -> void {
	run_stage(system_stage_kind::initialize);
}

auto gse::scheduler::run_update() const -> void {
	run_stage(system_stage_kind::update);
}

auto gse::scheduler::run_render() const -> void {
	run_stage(system_stage_kind::render);
}

auto gse::scheduler::run_shutdown() const -> void {
	run_stage(system_stage_kind::shutdown);
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	for (auto& v : m_stage_nodes) {
		v.clear();
	}
	for (auto& v : m_phase_nodes) {
		v.clear();
	}
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
		(typed.system.*StageDesc::method)();
	};

	sn.reads = type_indices<typename StageDesc::read_set>::make();
	sn.writes = type_indices<typename StageDesc::write_set>::make();

	m_stage_nodes[idx].push_back(std::move(sn));
}

auto gse::scheduler::to_index(system_stage_kind k) -> std::size_t {
	return static_cast<std::size_t>(k);
}

auto gse::scheduler::conflicts(const stage_node& a, const stage_node& b) -> bool {
	for (const auto& wt : a.writes) {
		for (const auto& wt_b : b.writes) {
			if (wt == wt_b) {
				return true;
			}
		}
		for (const auto& rd_b : b.reads) {
			if (wt == rd_b) {
				return true;
			}
		}
	}
	return false;
}

auto gse::scheduler::build_phases(std::span<stage_node> nodes) -> std::vector<std::vector<stage_node*>> {
	std::vector<std::vector<stage_node*>> phases;

	for (auto& n : nodes) {
		std::size_t phase_index = 0;

		for (;;) {
			if (phase_index == phases.size()) {
				phases.emplace_back();
			}

			auto& phase = phases[phase_index];
			bool conflict = false;

			for (const auto* other : phase) {
				if (conflicts(n, *other) || conflicts(*other, n)) {
					conflict = true;
					break;
				}
			}

			if (!conflict) {
				phase.push_back(&n);
				break;
			}

			++phase_index;
		}
	}

	return phases;
}

template <typename Runner>
auto gse::scheduler::run_phase_set(const std::vector<std::vector<stage_node*>>& phases, Runner&& runner) -> void {
	for (const auto& phase : phases) {
		if (phase.empty()) {
			continue;
		}

		if (phase.size() == 1) {
			runner(*phase.front());
			continue;
		}

		task::group group;
		for (auto* n : phase) {
			group.post([n, fn = runner]() mutable {
				fn(*n);
			});
		}
	}
}

auto gse::scheduler::run_stage(const system_stage_kind k) const -> void {
	const auto idx = to_index(k);
	if (idx >= stage_count) {
		return;
	}

	run_phase_set(
		m_phase_nodes[idx],
		[](const stage_node& n) {
			n.run(*n.owner);
		}
	);
}

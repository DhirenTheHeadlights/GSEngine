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
		) -> S& override;

		template <scheduled_system S>
		auto system(
		) const -> const S& override;

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

template <gse::scheduled_system S, typename... Args>
auto gse::scheduler::emplace_system(Args&&... args) -> S& {
	auto ptr = std::make_unique<node<S>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();
	m_system_index.emplace(std::type_index(typeid(S)), raw);
	m_nodes.push_back(std::move(ptr));
	register_system_stages(*raw);
	return raw->system;
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

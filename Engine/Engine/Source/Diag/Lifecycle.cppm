export module gse.diag:lifecycle;

import std;

import gse.assert;
import gse.log;
import gse.math;
import gse.meta;
import gse.time;

export namespace gse {
	struct lifecycle_edges {
		std::uint64_t from = 0;
	};

	struct lifecycle_returns {
		std::uint64_t to = 0;
	};

	struct lifecycle_reentry {};

	consteval auto entered_from(
		std::initializer_list<std::uint64_t> states
	) -> lifecycle_edges;

	consteval auto returns_to(
		std::initializer_list<std::uint64_t> states
	) -> lifecycle_returns;

	consteval auto reentered() -> lifecycle_reentry;

	template <typename E>
	constexpr auto lifecycle_edge_declared(
		E from,
		E to
	) -> bool;

	template <typename E>
	class lifecycle {
	public:
		lifecycle(
			std::string_view machine,
			log::category category
		);

		auto enter(
			E next
		) -> void;

		[[nodiscard]] auto state() const -> E;

	private:
		std::string_view m_machine;
		log::category m_category;
		E m_state{};
		clock m_entered;
	};

	template <typename E>
	class atomic_lifecycle {
	public:
		atomic_lifecycle() = default;

		explicit atomic_lifecycle(
			E initial
		);

		atomic_lifecycle(
			atomic_lifecycle&& other
		) noexcept;

		auto operator=(
			atomic_lifecycle&& other
		) noexcept -> atomic_lifecycle&;

		auto enter(
			E next
		) -> void;

		auto try_enter(
			E from,
			E next
		) -> bool;

		[[nodiscard]] auto state() const -> E;

	private:
		std::atomic<E> m_state{};
	};
}

namespace gse {
	consteval auto lifecycle_state_mask(
		std::initializer_list<std::uint64_t> states
	) -> std::uint64_t;

	auto report_lifecycle_transition(
		std::string_view machine,
		log::category category,
		std::string_view from,
		std::string_view to,
		time dwell
	) -> void;
}

consteval auto gse::lifecycle_state_mask(const std::initializer_list<std::uint64_t> states) -> std::uint64_t {
	std::uint64_t mask = 0;
	for (const std::uint64_t state : states) {
		mask |= std::uint64_t{ 1 } << state;
	}
	return mask;
}

consteval auto gse::entered_from(const std::initializer_list<std::uint64_t> states) -> lifecycle_edges {
	return {
		.from = lifecycle_state_mask(states),
	};
}

consteval auto gse::returns_to(const std::initializer_list<std::uint64_t> states) -> lifecycle_returns {
	return {
		.to = lifecycle_state_mask(states),
	};
}

consteval auto gse::reentered() -> lifecycle_reentry {
	return {};
}

template <typename E>
constexpr auto gse::lifecycle_edge_declared(const E from, const E to) -> bool {
	if (from == to) {
		return enum_has_annotation<lifecycle_reentry>(to);
	}
	const std::uint64_t current = std::uint64_t{ 1 } << std::to_underlying(from);
	const std::uint64_t target = std::uint64_t{ 1 } << std::to_underlying(to);
	const lifecycle_edges edges = annotation_from_enum<lifecycle_edges>(to, {});
	const lifecycle_returns returns = annotation_from_enum<lifecycle_returns>(from, {});
	return (edges.from & current) != 0 || (returns.to & target) != 0;
}

template <typename E>
gse::lifecycle<E>::lifecycle(const std::string_view machine, const log::category category) : m_machine(machine), m_category(category) {}

template <typename E>
auto gse::lifecycle<E>::enter(const E next) -> void {
	assert(lifecycle_edge_declared(m_state, next), "{}: {} -> {} is not a declared edge", m_machine, enum_to_string(m_state), enum_to_string(next));

	report_lifecycle_transition(m_machine, m_category, enum_to_string(m_state), enum_to_string(next), m_entered.reset());
	m_state = next;
}

template <typename E>
auto gse::lifecycle<E>::state() const -> E {
	return m_state;
}

template <typename E>
gse::atomic_lifecycle<E>::atomic_lifecycle(const E initial) : m_state(initial) {}

template <typename E>
gse::atomic_lifecycle<E>::atomic_lifecycle(atomic_lifecycle&& other) noexcept : m_state(other.state()) {}

template <typename E>
auto gse::atomic_lifecycle<E>::operator=(atomic_lifecycle&& other) noexcept -> atomic_lifecycle& {
	m_state.store(other.state(), std::memory_order_release);
	return *this;
}

template <typename E>
auto gse::atomic_lifecycle<E>::enter(const E next) -> void {
	const E previous = m_state.exchange(next, std::memory_order_acq_rel);
	assert(lifecycle_edge_declared(previous, next), "{}::{}: {} -> {} is not a declared edge", meta::namespace_name<E>(), meta::unqualified_name<E>(), enum_to_string(previous), enum_to_string(next));
}

template <typename E>
auto gse::atomic_lifecycle<E>::try_enter(const E from, const E next) -> bool {
	assert(lifecycle_edge_declared(from, next), "{}::{}: {} -> {} is not a declared edge", meta::namespace_name<E>(), meta::unqualified_name<E>(), enum_to_string(from), enum_to_string(next));

	E expected = from;
	return m_state.compare_exchange_strong(expected, next, std::memory_order_acq_rel, std::memory_order_relaxed);
}

template <typename E>
auto gse::atomic_lifecycle<E>::state() const -> E {
	return m_state.load(std::memory_order_acquire);
}

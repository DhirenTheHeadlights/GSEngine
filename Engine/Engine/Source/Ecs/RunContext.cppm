export module gse.ecs:context;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.diag;
import std;

import :access;
import :registries;
import :registry;
import :system_anno;

export namespace gse {
	struct init_yield {
		async::manual_event resume;
		async::manual_event paused;
	};

	class context {
	public:
		context(
			system_hook hook,
			state_registry& states,
			resource_registry& resources,
			channel_registry& channels,
			registry& reg
		);

		context(
			init_yield& yield,
			state_registry& states,
			resource_registry& resources,
			channel_registry& channels,
			registry& reg
		);

		[[nodiscard]] auto yield_tick() -> async::task<>;

		template <typename T>
		auto add_component(
			id owner
		) -> void;

		template <typename T>
		auto add_component(
			id owner,
			T value
		) -> void;

		template <typename T>
		auto remove_component(
			id owner
		) -> void;

		auto defer_structural_change(
			std::move_only_function<void()> change
		) -> void;

		auto apply_structural_changes() -> void;

		[[nodiscard]] auto held_lock_count() const -> int;

		template <typename Access>
		auto make_access() -> Access;

		auto make_entities() -> entities;

		template <typename T>
		requires is_same_frame_channel_v<T>
		auto drain_channel() -> std::vector<T>;

		[[nodiscard]] auto pending_reveals() const -> std::vector<std::string_view>;

	private:
		template <typename Arg, typename S>
		friend auto resolve_run_arg(
			context& ctx,
			S& state
		) -> decltype(auto);

		template <typename Data>
		friend auto live_system_state(
			context& ctx
		) -> Data*;

		system_hook m_hook;
		init_yield* m_yield = nullptr;
		state_registry& m_states;
		resource_registry& m_resources;
		channel_registry& m_channels;
		registry& m_reg;
		std::atomic<int> m_held_locks{ 0 };
		std::vector<std::move_only_function<void()>> m_structural_changes;
	};
}

gse::context::context(const system_hook hook, state_registry& states, resource_registry& resources, channel_registry& channels, registry& reg) : m_hook(hook), m_states(states), m_resources(resources), m_channels(channels), m_reg(reg) {}

gse::context::context(init_yield& yield, state_registry& states, resource_registry& resources, channel_registry& channels, registry& reg) : m_hook(system_hook::init), m_yield(&yield), m_states(states), m_resources(resources), m_channels(channels), m_reg(reg) {}

template <typename Access>
auto gse::context::make_access() -> Access {
	return m_reg.acquire<Access>(m_held_locks);
}

auto gse::context::held_lock_count() const -> int {
	return m_held_locks.load(std::memory_order_acquire);
}

template <typename T>
auto gse::context::add_component(const id owner) -> void {
	defer_structural_change([&reg = m_reg, owner] {
		if (reg.exists(owner)) {
			reg.add_component<T>(owner);
		}
	});
}

template <typename T>
auto gse::context::add_component(const id owner, T value) -> void {
	defer_structural_change([&reg = m_reg, owner, value = std::move(value)]() mutable {
		if (reg.exists(owner)) {
			reg.add_component<T>(owner, std::move(value));
		}
	});
}

template <typename T>
auto gse::context::remove_component(const id owner) -> void {
	defer_structural_change([&reg = m_reg, owner] {
		reg.remove_component<T>(owner);
	});
}

auto gse::context::defer_structural_change(std::move_only_function<void()> change) -> void {
	assert(m_hook == system_hook::run, "structural changes can only be deferred from run(), and apply once every run() of the tick has finished; take gse::entities to change structure immediately");
	m_structural_changes.push_back(std::move(change));
}

auto gse::context::apply_structural_changes() -> void {
	for (auto& change : m_structural_changes) {
		change();
	}
	m_structural_changes.clear();
}

auto gse::context::make_entities() -> entities {
	return entities(&m_reg);
}

template <typename T>
requires gse::is_same_frame_channel_v<T>
auto gse::context::drain_channel() -> std::vector<T> {
	return m_channels.drain<T>();
}

auto gse::context::pending_reveals() const -> std::vector<std::string_view> {
	return m_channels.pending_reveals();
}

gse::entities::entities(registry* reg) : m_reg(reg) {
}

auto gse::entities::ensure_exists(const id owner) const -> void {
	m_reg->ensure_exists(owner);
}

auto gse::entities::exists(const id owner) const -> bool {
	return m_reg->exists(owner);
}

auto gse::entities::active(const id owner) const -> bool {
	return m_reg->active(owner);
}

auto gse::entities::has_components(const id owner) const -> bool {
	return m_reg->has_components(owner);
}

auto gse::entities::ensure_active(const id owner) const -> void {
	m_reg->ensure_active(owner);
}

auto gse::entities::remove(const id owner) const -> void {
	m_reg->remove(owner);
}

template <typename T>
auto gse::entities::add_component(const id owner, T value) const -> T* {
	return m_reg->add_component<T>(owner, std::move(value));
}

template <typename T>
auto gse::entities::remove_component(const id owner) const -> void {
	m_reg->remove_component<T>(owner);
}

template <typename T>
auto gse::entities::ensure_storage() const -> void {
	m_reg->ensure_storage<T>();
}
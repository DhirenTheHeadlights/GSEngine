export module gse.ecs:phase_context;

import std;

import gse.assert;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.assets;

import :component;
import :registries;
import :registry;

export namespace gse {
	class scheduler;

	struct init_context {
		void* gpu_ctx = nullptr;
		void* assets_ptr = nullptr;
		registry& reg;
		scheduler& sched;
		state_registry& states;
		resource_registry& resources_store;
		channel_registry& channels_store;
		channel_writer& channels;

		template <typename T>
		auto get(
		) const -> T&;

		template <typename T>
		auto try_get(
		) const -> T*;

		auto assets(
		) const -> asset::registry&;

		auto try_assets(
		) const -> asset::registry*;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;

		template <typename Resources>
		auto resources_of(
		) const -> const Resources&;

		template <typename Resources>
		auto try_resources_of(
		) const -> const Resources*;
	};

	struct shutdown_context {
		void* gpu_ctx = nullptr;
		void* assets_ptr = nullptr;
		registry& reg;

		template <typename T>
		auto get(
		) const -> T&;

		template <typename T>
		auto try_get(
		) const -> T*;

		auto assets(
		) const -> asset::registry&;

		auto try_assets(
		) const -> asset::registry*;
	};

	template <typename S, typename State>
	concept has_initialize = requires(init_context& p, State& s) {
		{ S::initialize(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State>
	concept has_shutdown = requires(shutdown_context& p, State& s) {
		{ S::shutdown(p, s) } -> std::same_as<void>;
	};
}

template <typename T>
auto gse::init_context::get() const -> T& {
	return *static_cast<T*>(gpu_ctx);
}

template <typename T>
auto gse::init_context::try_get() const -> T* {
	return static_cast<T*>(gpu_ctx);
}

template <typename State>
auto gse::init_context::state_of() const -> const State& {
	const auto* p = states.state_ptr(id_of<State>());
	assert(p != nullptr, std::source_location::current(), "state not found");
	return *static_cast<const State*>(p);
}

template <typename State>
auto gse::init_context::try_state_of() const -> const State* {
	return static_cast<const State*>(states.state_ptr(id_of<State>()));
}

template <typename Resources>
auto gse::init_context::resources_of() const -> const Resources& {
	const auto* p = resources_store.resources_ptr(id_of<Resources>());
	assert(p != nullptr, std::source_location::current(), "resources not found");
	return *static_cast<const Resources*>(p);
}

template <typename Resources>
auto gse::init_context::try_resources_of() const -> const Resources* {
	return static_cast<const Resources*>(resources_store.resources_ptr(id_of<Resources>()));
}

template <typename T>
auto gse::shutdown_context::get() const -> T& {
	return *static_cast<T*>(gpu_ctx);
}

template <typename T>
auto gse::shutdown_context::try_get() const -> T* {
	return static_cast<T*>(gpu_ctx);
}

inline auto gse::init_context::assets() const -> asset::registry& {
	return *static_cast<asset::registry*>(assets_ptr);
}

inline auto gse::init_context::try_assets() const -> asset::registry* {
	return static_cast<asset::registry*>(assets_ptr);
}

inline auto gse::shutdown_context::assets() const -> asset::registry& {
	return *static_cast<asset::registry*>(assets_ptr);
}

inline auto gse::shutdown_context::try_assets() const -> asset::registry* {
	return static_cast<asset::registry*>(assets_ptr);
}

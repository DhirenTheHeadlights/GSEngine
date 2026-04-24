export module gse.ecs:phase_context;

import std;

import gse.core;
import gse.containers;
import gse.concurrency;

import :component;
import :registry;

export namespace gse {
	class scheduler;

	class system_provider {
	public:
		virtual ~system_provider() = default;

		virtual auto system_ptr(
			std::type_index idx
		) -> void* = 0;

		virtual auto system_ptr(
			std::type_index idx
		) const -> const void* = 0;

		virtual auto ensure_channel(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base& = 0;
	};

	class state_snapshot_provider {
	public:
		virtual ~state_snapshot_provider() = default;

		virtual auto snapshot_ptr(
			std::type_index type
		) const -> const void* = 0;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;
	};

	class channel_writer {
	public:
		using push_fn = std::function<void(std::type_index, std::any, channel_factory_fn)>;

		explicit channel_writer(
			push_fn fn
		);

		template <typename T>
		auto push(
			T item
		) -> void;

		template <promiseable T>
		auto push(
			T item
		) -> channel_future<typename T::result_type>;

	private:
		push_fn m_push;
	};

	class channel_reader_provider {
	public:
		virtual ~channel_reader_provider() = default;

		virtual auto channel_snapshot_ptr(
			std::type_index type
		) const -> const void* = 0;

		template <typename T>
		auto read(
		) const -> const std::vector<T>&;
	};

	class resources_provider {
	public:
		virtual ~resources_provider() = default;

		virtual auto resources_ptr(
			std::type_index type
		) const -> const void* = 0;

		template <typename Resources>
		auto resources_of(
		) const -> const Resources&;

		template <typename Resources>
		auto try_resources_of(
		) const -> const Resources*;
	};

	struct phase_gpu_access {
		void* gpu_ctx = nullptr;

		template <typename T>
		auto get(
		) const -> T&;

		template <typename T>
		auto try_get(
		) const -> T*;
	};

	struct init_context : phase_gpu_access {
		registry& reg;
		scheduler& sched;
		const state_snapshot_provider& snapshots;
		const resources_provider& resource_provider;
		channel_writer& channels;

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

	struct shutdown_context : phase_gpu_access {
		registry& reg;
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

template <typename State>
auto gse::state_snapshot_provider::state_of() const -> const State& {
	const auto* ptr = snapshot_ptr(std::type_index(typeid(State)));
	return *static_cast<const State*>(ptr);
}

template <typename State>
auto gse::state_snapshot_provider::try_state_of() const -> const State* {
	const auto* ptr = snapshot_ptr(std::type_index(typeid(State)));
	return static_cast<const State*>(ptr);
}

gse::channel_writer::channel_writer(push_fn fn) : m_push(std::move(fn)) {}

template <typename T>
auto gse::channel_writer::push(T item) -> void {
	m_push(
		std::type_index(typeid(T)),
		std::any(std::move(item)),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
}

template <gse::promiseable T>
auto gse::channel_writer::push(T item) -> channel_future<typename T::result_type> {
	auto [future, promise] = make_promise<typename T::result_type>();
	item.promise = std::move(promise);
	m_push(
		std::type_index(typeid(T)),
		std::any(std::move(item)),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
	return future;
}

template <typename T>
auto gse::channel_reader_provider::read() const -> const std::vector<T>& {
	const auto* ptr = channel_snapshot_ptr(std::type_index(typeid(T)));
	if (!ptr) {
		static const std::vector<T> empty;
		return empty;
	}
	return *static_cast<const std::vector<T>*>(ptr);
}

template <typename T>
auto gse::phase_gpu_access::get() const -> T& {
	return *static_cast<T*>(gpu_ctx);
}

template <typename T>
auto gse::phase_gpu_access::try_get() const -> T* {
	return static_cast<T*>(gpu_ctx);
}

template <typename State>
auto gse::init_context::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::init_context::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename Resources>
auto gse::init_context::resources_of() const -> const Resources& {
	return resource_provider.resources_of<Resources>();
}

template <typename Resources>
auto gse::init_context::try_resources_of() const -> const Resources* {
	return resource_provider.try_resources_of<Resources>();
}

template <typename Resources>
auto gse::resources_provider::resources_of() const -> const Resources& {
	const auto* ptr = resources_ptr(std::type_index(typeid(Resources)));
	return *static_cast<const Resources*>(ptr);
}

template <typename Resources>
auto gse::resources_provider::try_resources_of() const -> const Resources* {
	const auto* ptr = resources_ptr(std::type_index(typeid(Resources)));
	return static_cast<const Resources*>(ptr);
}


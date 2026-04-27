export module gse.ecs:phase_context;

import std;

import gse.core;
import gse.containers;
import gse.concurrency;

import :component;
import :registry;

export namespace gse {
	class scheduler;

	class channel_writer {
	public:
		template <typename F>
		explicit channel_writer(
			F&& fn
		);

		channel_writer(
			channel_writer&&
		) noexcept;

		auto operator=(
			channel_writer&&
		) noexcept -> channel_writer&;

		~channel_writer(
		);

		channel_writer(
			const channel_writer&
		) = delete;

		auto operator=(
			const channel_writer&
		) -> channel_writer& = delete;

		template <typename T>
		auto push(
			T item
		) -> void;

		template <promiseable T>
		auto push(
			T item
		) -> channel_future<typename T::result_type>;

	private:
		auto invoke_push(
			id idx,
			std::any value,
			channel_factory_fn factory
		) -> void;

		void* m_ctx = nullptr;
		void(*m_invoke)(void*, id, std::any, channel_factory_fn) = nullptr;
		void(*m_destroy)(void*) noexcept = nullptr;
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

template <typename F>
gse::channel_writer::channel_writer(F&& fn) {
	using closure = std::decay_t<F>;
	m_ctx = new closure(std::forward<F>(fn));
	m_invoke = +[](void* p, id idx, std::any value, channel_factory_fn factory) {
		(*static_cast<closure*>(p))(idx, std::move(value), std::move(factory));
	};
	m_destroy = +[](void* p) noexcept {
		delete static_cast<closure*>(p);
	};
}

template <typename T>
auto gse::channel_writer::push(T item) -> void {
	invoke_push(
		id_of<T>(),
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
	invoke_push(
		id_of<T>(),
		std::any(std::move(item)),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
	return future;
}

template <typename T>
auto gse::phase_gpu_access::get() const -> T& {
	return *static_cast<T*>(gpu_ctx);
}

template <typename T>
auto gse::phase_gpu_access::try_get() const -> T* {
	return static_cast<T*>(gpu_ctx);
}

gse::channel_writer::channel_writer(channel_writer&& other) noexcept
	: m_ctx(other.m_ctx), m_invoke(other.m_invoke), m_destroy(other.m_destroy) {
	other.m_ctx = nullptr;
	other.m_invoke = nullptr;
	other.m_destroy = nullptr;
}

auto gse::channel_writer::operator=(channel_writer&& other) noexcept -> channel_writer& {
	if (this != &other) {
		if (m_destroy && m_ctx) {
			m_destroy(m_ctx);
		}
		m_ctx = other.m_ctx;
		m_invoke = other.m_invoke;
		m_destroy = other.m_destroy;
		other.m_ctx = nullptr;
		other.m_invoke = nullptr;
		other.m_destroy = nullptr;
	}
	return *this;
}

gse::channel_writer::~channel_writer() {
	if (m_destroy && m_ctx) {
		m_destroy(m_ctx);
	}
}

auto gse::channel_writer::invoke_push(const id idx, std::any value, channel_factory_fn factory) -> void {
	m_invoke(m_ctx, idx, std::move(value), std::move(factory));
}

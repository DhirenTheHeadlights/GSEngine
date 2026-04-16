export module gse.utility:channel_promise;

import std;

import :task;

export namespace gse {
	template <typename T>
	class channel_future;

	template <typename T>
	class channel_promise;

	template <typename T>
	struct promise_state {
		std::optional<T> value;
		std::atomic<bool> ready{false};
		std::mutex mutex;
		std::coroutine_handle<> continuation;
	};

	template <typename T>
	class channel_future {
	public:
		struct awaiter {
			std::shared_ptr<promise_state<T>> m_state;

			auto await_ready(
			) const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> h
			) -> bool;

			auto await_resume(
			) -> T;
		};

		auto ready(
		) const -> bool;

		auto try_get(
		) const -> const T*;

		auto get(
		) const -> const T&;

		auto operator co_await(
		) noexcept -> awaiter;

	private:
		std::shared_ptr<promise_state<T>> m_state;

		explicit channel_future(
			std::shared_ptr<promise_state<T>> state
		);

		template <typename U>
		friend auto make_promise(
		) -> std::pair<channel_future<U>, channel_promise<U>>;
	};

	template <typename T>
	class channel_promise {
	public:
		channel_promise(
		) = default;

		auto fulfill(
			T value
		) const -> void;

		auto valid(
		) const -> bool;

	private:
		std::shared_ptr<promise_state<T>> m_state;

		explicit channel_promise(
			std::shared_ptr<promise_state<T>> state
		);

		template <typename U>
		friend auto make_promise(
		) -> std::pair<channel_future<U>, channel_promise<U>>;
	};

	template <typename T>
	auto make_promise(
	) -> std::pair<channel_future<T>, channel_promise<T>>;

	template <typename T>
	concept promiseable = requires(T t) {
		typename T::result_type;
		{ t.promise } -> std::same_as<channel_promise<typename T::result_type>&>;
	};
}

template <typename T>
gse::channel_future<T>::channel_future(std::shared_ptr<promise_state<T>> state) : m_state(std::move(state)) {}

template <typename T>
auto gse::channel_future<T>::awaiter::await_ready() const noexcept -> bool {
	return m_state->ready.load(std::memory_order_acquire);
}

template <typename T>
auto gse::channel_future<T>::awaiter::await_suspend(std::coroutine_handle<> h) -> bool {
	std::lock_guard lock(m_state->mutex);
	if (m_state->ready.load(std::memory_order_acquire)) {
		return false;
	}
	m_state->continuation = h;
	return true;
}

template <typename T>
auto gse::channel_future<T>::awaiter::await_resume() -> T {
	return std::move(*m_state->value);
}

template <typename T>
auto gse::channel_future<T>::ready() const -> bool {
	return m_state->ready.load(std::memory_order_acquire);
}

template <typename T>
auto gse::channel_future<T>::try_get() const -> const T* {
	if (m_state->ready.load(std::memory_order_acquire)) {
		return &*m_state->value;
	}
	return nullptr;
}

template <typename T>
auto gse::channel_future<T>::get() const -> const T& {
	return *m_state->value;
}

template <typename T>
auto gse::channel_future<T>::operator co_await() noexcept -> awaiter {
	return {m_state};
}

template <typename T>
gse::channel_promise<T>::channel_promise(std::shared_ptr<promise_state<T>> state) : m_state(std::move(state)) {}

template <typename T>
auto gse::channel_promise<T>::fulfill(T value) const -> void {
	std::lock_guard lock(m_state->mutex);
	m_state->value = std::move(value);
	m_state->ready.store(true, std::memory_order_release);
	if (m_state->continuation) {
		task::post([h = m_state->continuation] {
			h.resume();
		});
	}
}

template <typename T>
auto gse::channel_promise<T>::valid() const -> bool {
	return m_state != nullptr;
}

template <typename T>
auto gse::make_promise() -> std::pair<channel_future<T>, channel_promise<T>> {
	auto state = std::make_shared<promise_state<T>>();
	return {
		channel_future<T>(state),
		channel_promise<T>(state)
	};
}

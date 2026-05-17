export module gse.concurrency:async_task;

import std;

import gse.assert;
import gse.core;
import gse.diag;
import gse.log;

import :frame_arena;
import :task;

export namespace gse::async {
	template <typename T = void>
	class task;

	auto pass_recording_scope_push() noexcept -> void;

	auto pass_recording_scope_pop() noexcept -> void;

	[[nodiscard]] auto pass_recording_scope_active() noexcept -> int;

	struct final_awaiter {
		static auto await_ready() noexcept -> bool;

		template <typename P>
		static auto await_suspend(
			std::coroutine_handle<P> h
		) noexcept -> std::coroutine_handle<>;

		static auto await_resume() noexcept -> void;
	};

	struct promise_base {
		std::coroutine_handle<> m_continuation{ std::noop_coroutine() };
		std::exception_ptr m_exception;
		std::atomic<bool> m_started{ false };

		static auto initial_suspend() noexcept -> std::suspend_always;

		static auto final_suspend() noexcept -> final_awaiter;

		auto unhandled_exception() -> void;

		auto operator new(
			std::size_t size
		) -> void*;

		auto operator delete(
			void* ptr,
			std::size_t size
		) -> void;

		template <typename Awaitable>
		auto await_transform(
			Awaitable&& aw
		) -> decltype(auto);
	};

	template <typename T>
	struct value_promise : promise_base {
		std::optional<T> m_result;

		auto get_return_object() -> task<T>;

		auto return_value(
			T value
		) -> void;

		auto result() -> T;
	};

	template <typename T>
	struct value_promise<T&> : promise_base {
		T* m_result = nullptr;

		auto get_return_object() -> task<T&>;

		auto return_value(
			T& value
		) -> void;

		auto result() -> T&;
	};

	struct void_promise : promise_base {
		auto get_return_object() -> task<>;

		static auto return_void() noexcept -> void;

		auto result() const -> void;
	};

	template <typename T>
	class task {
	public:
		using promise_type = std::conditional_t<std::is_void_v<T>, void_promise, value_promise<T>>;
		using handle_type = std::coroutine_handle<promise_type>;

		task() noexcept = default;

		explicit task(
			handle_type h
		) noexcept;

		~task();

		task(
			task&& other
		) noexcept;

		auto operator=(
			task&& other
		) noexcept -> task&;

		task(
			const task&
		) = delete;

		auto operator=(
			const task&
		) -> task& = delete;

		auto start() -> void;

		auto consume_start_handle() -> std::coroutine_handle<>;

		auto done() const -> bool;

		struct awaiter {
			handle_type m_handle;

			auto await_ready() const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> caller
			) noexcept -> std::coroutine_handle<>;

			auto await_resume() -> T;
		};

		auto operator co_await() noexcept -> awaiter;

	private:
		handle_type m_handle{};
	};

	auto when_all(
		task<> a,
		task<> b
	) -> task<>;

	auto when_all(
		task<> a,
		task<> b,
		task<> c
	) -> task<>;

	auto when_all(
		task<> a,
		task<> b,
		task<> c,
		task<> d
	) -> task<>;

	auto when_all(
		std::vector<task<>> tasks
	) -> task<>;

	auto sync_wait(
		task<>&& t
	) -> void;

	template <typename T>
	auto sync_wait(
		task<T>&& t
	) -> T;

	struct yield_to_worker_t {
		auto await_ready() const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) const -> void;

		auto await_resume() const noexcept -> void;
	};

	[[nodiscard]] auto yield_to_worker() noexcept -> yield_to_worker_t;
}

template <typename P>
auto gse::async::final_awaiter::await_suspend(std::coroutine_handle<P> h) noexcept -> std::coroutine_handle<> {
	return h.promise().m_continuation;
}

template <typename Awaitable>
decltype(auto) gse::async::promise_base::await_transform(Awaitable&& aw) {
	assert(pass_recording_scope_active() == 0, "co_await is forbidden while a recording_context is alive; co_await before or after the pass scope");
	return std::forward<Awaitable>(aw);
}

template <typename T>
auto gse::async::value_promise<T>::get_return_object() -> task<T> {
	return task<T>{ std::coroutine_handle<value_promise<T>>::from_promise(*this) };
}

template <typename T>
auto gse::async::value_promise<T>::return_value(T value) -> void {
	m_result.emplace(std::move(value));
}

template <typename T>
auto gse::async::value_promise<T>::result() -> T {
	if (m_exception) {
		std::rethrow_exception(m_exception);
	}
	return std::move(*m_result);
}

template <typename T>
auto gse::async::value_promise<T&>::get_return_object() -> task<T&> {
	return task<T&>{ std::coroutine_handle<value_promise<T&>>::from_promise(*this) };
}

template <typename T>
auto gse::async::value_promise<T&>::return_value(T& value) -> void {
	m_result = std::addressof(value);
}

template <typename T>
auto gse::async::value_promise<T&>::result() -> T& {
	if (m_exception) {
		std::rethrow_exception(m_exception);
	}
	return *m_result;
}

template <typename T>
gse::async::task<T>::task(handle_type h) noexcept : m_handle(h) {
}

template <typename T>
gse::async::task<T>::~task() {
	if (m_handle) {
		m_handle.destroy();
	}
}

template <typename T>
gse::async::task<T>::task(task&& other) noexcept : m_handle(other.m_handle) {
	other.m_handle = {};
}

template <typename T>
auto gse::async::task<T>::operator=(task&& other) noexcept -> task& {
	if (this != &other) {
		if (m_handle) {
			m_handle.destroy();
		}
		m_handle = other.m_handle;
		other.m_handle = {};
	}
	return *this;
}

template <typename T>
auto gse::async::task<T>::start() -> void {
	if (m_handle.promise().m_started.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	m_handle.resume();
}

template <typename T>
auto gse::async::task<T>::consume_start_handle() -> std::coroutine_handle<> {
	if (!m_handle || m_handle.promise().m_started.exchange(true, std::memory_order_acq_rel)) {
		return std::noop_coroutine();
	}
	return m_handle;
}

template <typename T>
auto gse::async::task<T>::done() const -> bool {
	return !m_handle || m_handle.done();
}

template <typename T>
auto gse::async::task<T>::awaiter::await_ready() const noexcept -> bool {
	return m_handle.done();
}

template <typename T>
auto gse::async::task<T>::awaiter::await_suspend(std::coroutine_handle<> caller) noexcept -> std::coroutine_handle<> {
	m_handle.promise().m_continuation = caller;
	if (m_handle.promise().m_started.exchange(true, std::memory_order_acq_rel)) {
		return std::noop_coroutine();
	}
	return m_handle;
}

template <typename T>
auto gse::async::task<T>::awaiter::await_resume() -> T {
	return m_handle.promise().result();
}

template <typename T>
auto gse::async::task<T>::operator co_await() noexcept -> awaiter {
	return awaiter{ m_handle };
}

template <typename T>
auto gse::async::sync_wait(task<T>&& t) -> T {
	std::atomic<bool> done_flag{ false };
	std::optional<T> result;
	std::exception_ptr ep;

	auto wrapper = [&]() -> task<> {
		try {
			result.emplace(co_await std::move(t));
		}
		catch (...) {
			ep = std::current_exception();
		}
		done_flag.store(true, std::memory_order_release);
	};

	auto w = wrapper();
	w.start();

	while (!done_flag.load(std::memory_order_acquire)) {
		if (!gse::task::try_run_one()) {
			std::this_thread::yield();
		}
	}

	while (!w.done()) {
		std::this_thread::yield();
	}

	if (ep) {
		std::rethrow_exception(ep);
	}
	return std::move(*result);
}

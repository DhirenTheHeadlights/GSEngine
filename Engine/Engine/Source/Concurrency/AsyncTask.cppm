export module gse.concurrency:async_task;

import std;

import gse.core;
import gse.diag;
import gse.log;

import :frame_arena;
import :task;

export namespace gse::async {
	template <typename T = void>
	class task;

	struct final_awaiter {
		static auto await_ready(
		) noexcept -> bool;

		template <typename P>
		static auto await_suspend(
			std::coroutine_handle<P> h
		) noexcept -> std::coroutine_handle<>;

		static auto await_resume(
		) noexcept -> void;
	};

	struct promise_base {
		std::coroutine_handle<> m_continuation{ std::noop_coroutine() };
		std::exception_ptr m_exception;

		static auto initial_suspend(
		) noexcept -> std::suspend_always;

		static auto final_suspend(
		) noexcept -> final_awaiter;

		auto unhandled_exception(
		) -> void;

		auto operator new(
			std::size_t size
		) -> void*;

		auto operator delete(
			void* ptr,
			std::size_t size
		) -> void;
	};

	template <typename T>
	struct value_promise : promise_base {
		std::optional<T> m_result;

		auto get_return_object(
		) -> task<T>;

		auto return_value(
			T value
		) -> void;

		auto result(
		) -> T;
	};

	struct void_promise : promise_base {
		auto get_return_object(
		) -> task<>;

		static auto return_void(
		) noexcept -> void;

		auto result(
		) const -> void;
	};

	template <typename T>
	class task {
	public:
		using promise_type = std::conditional_t<std::is_void_v<T>, void_promise, value_promise<T>>;
		using handle_type = std::coroutine_handle<promise_type>;

		task(
		) noexcept = default;

		explicit task(
			handle_type h
		) noexcept;

		~task(
		);

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

		auto start(
		) -> void;

		auto done(
		) const -> bool;

		struct awaiter {
			handle_type m_handle;

			auto await_ready(
			) const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> caller
			) noexcept -> handle_type;

			auto await_resume(
			) -> T;
		};

		auto operator co_await(
		) noexcept -> awaiter;

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
}

namespace gse::async {
	struct when_all_state {
		std::atomic<int> remaining;
		std::coroutine_handle<> continuation;
		std::atomic<bool> has_exception{ false };
		std::exception_ptr first_exception;
	};

	struct suspend_and_capture {
		std::coroutine_handle<>& target;
		std::vector<task<>>& helpers;

		static auto await_ready(
		) noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) const noexcept -> void;

		static auto await_resume(
		) noexcept -> void;
	};

	auto when_all_helper(
		task<> child,
		when_all_state* state
	) -> task<>;

	auto when_all_impl(
		std::vector<task<>> tasks
	) -> task<>;
}

auto gse::async::final_awaiter::await_ready() noexcept -> bool {
	return false;
}

template <typename P>
auto gse::async::final_awaiter::await_suspend(std::coroutine_handle<P> h) noexcept -> std::coroutine_handle<> {
	return h.promise().m_continuation;
}

auto gse::async::final_awaiter::await_resume() noexcept -> void {
	
}

auto gse::async::promise_base::initial_suspend() noexcept -> std::suspend_always {
	return {};
}

auto gse::async::promise_base::final_suspend() noexcept -> final_awaiter {
	return {};
}

auto gse::async::promise_base::unhandled_exception() -> void {
	m_exception = std::current_exception();
}

auto gse::async::promise_base::operator new(const std::size_t size) -> void* {
	return frame_arena::allocate(size);
}

auto gse::async::promise_base::operator delete(void* ptr, const std::size_t size) -> void {
	frame_arena::deallocate(ptr, size);
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

auto gse::async::void_promise::get_return_object() -> task<> {
	return task{ std::coroutine_handle<void_promise>::from_promise(*this) };
}

auto gse::async::void_promise::return_void() noexcept -> void {
	
}

auto gse::async::void_promise::result() const -> void {
	if (m_exception) {
		std::rethrow_exception(m_exception);
	}
}

template <typename T>
gse::async::task<T>::task(handle_type h) noexcept : m_handle(h) {}

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
	m_handle.resume();
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
auto gse::async::task<T>::awaiter::await_suspend(std::coroutine_handle<> caller) noexcept -> handle_type {
	m_handle.promise().m_continuation = caller;
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

auto gse::async::suspend_and_capture::await_ready() noexcept -> bool {
	return false;
}

auto gse::async::suspend_and_capture::await_suspend(const std::coroutine_handle<> h) const noexcept -> void {
	target = h;
	for (auto& helper : helpers) {
		gse::task::post([&helper] { helper.start(); });
	}
}

auto gse::async::suspend_and_capture::await_resume() noexcept -> void {
	
}

auto gse::async::when_all_helper(task<> child, when_all_state* state) -> task<> {
	try {
		co_await child;
	}
	catch (...) {
		bool expected = false;
		if (state->has_exception.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
			state->first_exception = std::current_exception();
		}
	}
	if (state->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		auto h = state->continuation;
		gse::task::post([h] { h.resume(); });
	}
}

auto gse::async::when_all_impl(std::vector<task<>> tasks) -> task<> {
	if (tasks.empty()) {
		co_return;
	}

	if (tasks.size() == 1) {
		co_await std::move(tasks[0]);
		co_return;
	}

	when_all_state state;
	state.remaining.store(static_cast<int>(tasks.size()), std::memory_order_relaxed);

	std::vector<task<>> helpers;
	helpers.reserve(tasks.size());
	for (auto& t : tasks) {
		helpers.push_back(when_all_helper(std::move(t), &state));
	}

	co_await suspend_and_capture{ state.continuation, helpers };

	if (state.has_exception.load(std::memory_order_acquire)) {
		std::rethrow_exception(state.first_exception);
	}
}

auto gse::async::when_all(task<> a, task<> b) -> task<> {
	std::vector<task<>> tasks;
	tasks.reserve(2);
	tasks.push_back(std::move(a));
	tasks.push_back(std::move(b));
	co_await when_all_impl(std::move(tasks));
}

auto gse::async::when_all(task<> a, task<> b, task<> c) -> task<> {
	std::vector<task<>> tasks;
	tasks.reserve(3);
	tasks.push_back(std::move(a));
	tasks.push_back(std::move(b));
	tasks.push_back(std::move(c));
	co_await when_all_impl(std::move(tasks));
}

auto gse::async::when_all(task<> a, task<> b, task<> c, task<> d) -> task<> {
	std::vector<task<>> tasks;
	tasks.reserve(4);
	tasks.push_back(std::move(a));
	tasks.push_back(std::move(b));
	tasks.push_back(std::move(c));
	tasks.push_back(std::move(d));
	co_await when_all_impl(std::move(tasks));
}

auto gse::async::when_all(std::vector<task<>> tasks) -> task<> {
	co_await when_all_impl(std::move(tasks));
}

auto gse::async::sync_wait(task<>&& t) -> void {
	std::binary_semaphore done{ 0 };
	bool has_exception = false;
	std::exception_ptr ep;

	auto wrapper = [&]() -> task<> {
		try {
			co_await std::move(t);
		}
		catch (...) {
			has_exception = true;
			ep = std::current_exception();
		}
		done.release();
	};

	auto w = wrapper();
	trace::scope(find_or_generate_id("sync_wait::start"), [&] {
		w.start();
	});
	trace::scope(find_or_generate_id("sync_wait::acquire"), [&] {
		done.acquire();
	});
	trace::scope(find_or_generate_id("sync_wait::final_yield"), [&] {
		while (!w.done()) {
			std::this_thread::yield();
		}
	});

	if (has_exception) {
		std::rethrow_exception(ep);
	}
}

template <typename T>
auto gse::async::sync_wait(task<T>&& t) -> T {
	std::binary_semaphore done{ 0 };
	std::optional<T> result;
	std::exception_ptr ep;

	auto wrapper = [&]() -> task<> {
		try {
			result.emplace(co_await std::move(t));
		}
		catch (...) {
			ep = std::current_exception();
		}
		done.release();
	};

	auto w = wrapper();
	w.start();
	done.acquire();

	while (!w.done()) {
		std::this_thread::yield();
	}

	if (ep) {
		std::rethrow_exception(ep);
	}
	return std::move(*result);
}

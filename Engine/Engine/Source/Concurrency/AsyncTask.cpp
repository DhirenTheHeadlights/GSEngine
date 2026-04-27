module gse.concurrency;

import std;

import gse.diag;
import gse.log;

import :async_task;
import :frame_arena;
import :task;

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

auto gse::async::final_awaiter::await_resume() noexcept -> void {}

auto gse::async::promise_base::initial_suspend() noexcept -> std::suspend_always {
	return {};
}

auto gse::async::promise_base::final_suspend() noexcept -> final_awaiter {
	return {};
}

auto gse::async::promise_base::unhandled_exception() -> void {
	try {
		throw;
	}
	catch (const std::exception& e) {
		log::println(log::level::error, log::category::task, "Coroutine exception: {}", e.what());
	}
	catch (...) {
		log::println(log::level::error, log::category::task, "Coroutine exception (unknown type)");
	}
}

auto gse::async::promise_base::operator new(const std::size_t size) -> void* {
	return frame_arena::allocate(size);
}

auto gse::async::promise_base::operator delete(void* ptr, const std::size_t size) -> void {
	frame_arena::deallocate(ptr, size);
}

auto gse::async::void_promise::get_return_object() -> task<> {
	return task{ std::coroutine_handle<void_promise>::from_promise(*this) };
}

auto gse::async::void_promise::return_void() noexcept -> void {}

auto gse::async::void_promise::result() const -> void {
	if (m_exception) {
		std::rethrow_exception(m_exception);
	}
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

auto gse::async::suspend_and_capture::await_resume() noexcept -> void {}

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
	{
		trace::scope_guard sg{ trace_id<"sync_wait::start">() };
		w.start();
	}
	{
		trace::scope_guard sg{ trace_id<"sync_wait::acquire">() };
		done.acquire();
	}
	{
		trace::scope_guard sg{ trace_id<"sync_wait::final_yield">() };
		while (!w.done()) {
			std::this_thread::yield();
		}
	}

	if (has_exception) {
		std::rethrow_exception(ep);
	}
}

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

		static auto await_ready() noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) const noexcept -> std::coroutine_handle<>;

		static auto await_resume() noexcept -> void;
	};

	struct symmetric_resume {
		std::coroutine_handle<> handle;

		static auto await_ready() noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) const noexcept -> std::coroutine_handle<>;

		static auto await_resume() noexcept -> void;
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

auto gse::async::final_awaiter::await_resume() noexcept -> void {
}

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

namespace gse::async {
	thread_local int t_pass_recording_active = 0;
}

auto gse::async::pass_recording_scope_push() noexcept -> void {
	++t_pass_recording_active;
}

auto gse::async::pass_recording_scope_pop() noexcept -> void {
	--t_pass_recording_active;
}

auto gse::async::pass_recording_scope_active() noexcept -> int {
	return t_pass_recording_active;
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

auto gse::async::suspend_and_capture::await_ready() noexcept -> bool {
	return false;
}

auto gse::async::suspend_and_capture::await_suspend(const std::coroutine_handle<> h) const noexcept -> std::coroutine_handle<> {
	target = h;
	if (helpers.empty()) {
		return std::noop_coroutine();
	}
	if (helpers.size() > 1) {
		std::vector<gse::job> jobs;
		jobs.reserve(helpers.size() - 1);
		for (std::size_t i = 1; i < helpers.size(); ++i) {
			const auto handle = helpers[i].consume_start_handle();
			if (!handle) {
				log::println(log::level::error, log::category::task, "when_all helper consume_start_handle returned empty handle (i={})", i);
				continue;
			}
			jobs.emplace_back([handle] {
				if (!handle) {
					return;
				}
				handle.resume();
			});
		}
		gse::task::post_range(jobs.begin(), jobs.end(), trace_id<"async::when_all::resume">());
	}
	return helpers[0].consume_start_handle();
}

auto gse::async::suspend_and_capture::await_resume() noexcept -> void {
}

auto gse::async::symmetric_resume::await_ready() noexcept -> bool {
	return false;
}

auto gse::async::symmetric_resume::await_suspend(std::coroutine_handle<>) const noexcept -> std::coroutine_handle<> {
	return handle ? handle : std::noop_coroutine();
}

auto gse::async::symmetric_resume::await_resume() noexcept -> void {
}

auto gse::async::yield_to_worker_t::await_ready() const noexcept -> bool {
	return false;
}

auto gse::async::yield_to_worker_t::await_suspend(std::coroutine_handle<> h) const -> void {
	if (!h) {
		log::println(log::level::error, log::category::task, "yield_to_worker: empty handle from coroutine machinery");
		return;
	}
	gse::task::post([h] {
		if (!h) {
			return;
		}
		h.resume();
	},
					trace_id<"async::yield_to_worker">());
}

auto gse::async::yield_to_worker_t::await_resume() const noexcept -> void {
}

auto gse::async::yield_to_worker() noexcept -> yield_to_worker_t {
	return {};
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
		co_await symmetric_resume{ state->continuation };
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
	std::atomic<bool> done_flag{ false };
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
		done_flag.store(true, std::memory_order_release);
	};

	auto w = wrapper();
	w.start();
	{
		trace::scope_guard sg{ trace_id<"sync_wait::acquire">() };
		while (!done_flag.load(std::memory_order_acquire)) {
			if (!gse::task::try_run_one()) {
				std::this_thread::yield();
			}
		}
	}
	while (!w.done()) {
		std::this_thread::yield();
	}

	if (has_exception) {
		std::rethrow_exception(ep);
	}
}

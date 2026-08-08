module gse.concurrency:async_task_impl;

import std;

import :async_task;
import :frame_arena;
import :task;

import gse.diag;
import gse.log;


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

		template <typename P>
		auto await_suspend(
			std::coroutine_handle<P> h
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
	untrack_frame(ptr);
	frame_arena::deallocate(ptr, size);
}

auto gse::async::track_frame(const std::coroutine_handle<> h) -> checked_handle {
	if (!h) {
		return {};
	}

	const std::uint64_t generation = tracked_generation.fetch_add(1, std::memory_order_relaxed) + 1;
	{
		const std::lock_guard lock(tracked_frames_mutex);
		tracked_frames.insert_or_assign(h.address(), generation);
		tracked_frame_count.store(tracked_frames.size(), std::memory_order_release);
		if (tracked_frames.size() > tracked_frame_warn_threshold && !tracked_frame_warned) {
			tracked_frame_warned = true;
			log::println(
				log::level::error,
				log::category::task,
				"async: {} coroutine frames are still marked live; frames are not being untracked on destruction, so resume_checked cannot detect dead frames",
				tracked_frames.size()
			);
		}
	}
	return {
		.handle = h,
		.generation = generation,
	};
}

auto gse::async::untrack_frame(void* frame) -> void {
	if (tracked_frame_count.load(std::memory_order_acquire) == 0) {
		return;
	}

	const std::lock_guard lock(tracked_frames_mutex);
	if (tracked_frames.erase(frame) != 0) {
		tracked_frame_count.store(tracked_frames.size(), std::memory_order_release);
	}
}

auto gse::async::resume_checked(const checked_handle& tracked) -> bool {
	if (!tracked.handle || tracked.generation == 0) {
		return false;
	}

	{
		const std::lock_guard lock(tracked_frames_mutex);
		const auto it = tracked_frames.find(tracked.handle.address());
		if (it == tracked_frames.end() || it->second != tracked.generation) {
			return false;
		}
		tracked_frames.erase(it);
		tracked_frame_count.store(tracked_frames.size(), std::memory_order_release);
	}

	tracked.handle.resume();
	return true;
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
			const checked_handle tracked = track_frame(helpers[i].consume_start_handle());
			if (!tracked.handle) {
				log::println(
					log::level::error,
					log::category::task,
					"when_all helper consume_start_handle returned empty handle (i={})",
					i
				);
				continue;
			}
			jobs.emplace_back([tracked] {
				if (!resume_checked(tracked)) {
					log::println(
						log::level::error,
						log::category::task,
						"when_all helper resume skipped: coroutine frame was destroyed before the job ran"
					);
				}
			});
		}
		gse::task::post_range(jobs.begin(), jobs.end(), trace_id<"async::when_all::resume">());
	}

	const std::coroutine_handle<> first = helpers[0].consume_start_handle();
	if (!first) {
		log::println(
			log::level::error,
			log::category::task,
			"when_all: first helper produced an empty start handle"
		);
		return std::noop_coroutine();
	}
	return first;
}

auto gse::async::suspend_and_capture::await_resume() noexcept -> void {
}

auto gse::async::symmetric_resume::await_ready() noexcept -> bool {
	return false;
}

template <typename P>
auto gse::async::symmetric_resume::await_suspend(const std::coroutine_handle<P> h) const noexcept -> std::coroutine_handle<> {
	const std::coroutine_handle<> next = handle ? handle : std::noop_coroutine();
	if (h.promise().m_detached.load(std::memory_order_acquire)) {
		h.destroy();
	}
	return next;
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
	const checked_handle tracked = track_frame(h);
	gse::task::post_io(
		[tracked] {
			if (!resume_checked(tracked)) {
				log::println(
					log::level::error,
					log::category::task,
					"yield_to_worker: coroutine frame was destroyed before the resume job ran"
				);
			}
		},
		trace_id<"async::yield_to_worker">()
	);
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

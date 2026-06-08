module gse.ecs:run_context_impl;

import std;

import :run_context;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.diag;


// All NON-TEMPLATE coroutine bodies for gse.ecs:run_context live here in a module
// IMPLEMENTATION unit, not in RunContext.cppm. GCC trunk (+ -freflection) emits a
// corrupt BMI for an interface partition that contains a non-template coroutine
// body: an importer (e.g. gse.ecs:system_node) then fails with
//   "error: failed to read compiled module cluster N: Bad file data"
//   "fatal error: failed to load binding 'gse::run_context@gse.ecs:run_context'"
// Template coroutines (run_context::acquire) are unaffected and stay in the .cppm.
// This matches the engine convention already used in Concurrency/AsyncTask.cpp.

// These three are declared (non-exported) in RunContext.cppm; defined here.
namespace gse {
	using lock_fn = async::task<>(*)(async::rw_mutex_registry&, id);

	auto acquire_shared(
		async::rw_mutex_registry& mutexes, 
		id type
	) -> async::task<>;

	auto acquire_exclusive(
		async::rw_mutex_registry& mutexes,
		id type
	) -> async::task<>;
	
	auto acquire_locks_in_sorted_order(
		async::rw_mutex_registry& mutexes,
		std::span<const id> type_ids,
		std::span<const lock_fn> fns,
		id trace_id
	) -> async::task<>;
}

auto gse::run_context::next_tick() -> async::task<> {
	const int locks = held_lock_count();
	assert(
		locks == 0,
		"system held {} component lock(s) across co_await ctx.next_tick(); scope your acquire<> so the locked handle "
		"is destroyed before next_tick",
		locks
	);
	m_is_in_update_loop = true;
	m_settled = true;
	m_paused_event.set();
	co_await m_resume_event.wait();
	m_resume_event.reset();
}

auto gse::run_context::yield_tick() -> async::task<> {
	const int locks = held_lock_count();
	assert(
		locks == 0,
		"system held {} component lock(s) across co_await ctx.yield_tick(); scope your acquire<> so the locked handle "
		"is destroyed before yield_tick",
		locks
	);
	m_paused_event.set();
	co_await m_resume_event.wait();
	m_resume_event.reset();
}

auto gse::acquire_shared(async::rw_mutex_registry& mutexes, const id type) -> async::task<> {
	auto& mutex = mutexes.mutex_for(type);
	co_await mutex.lock_shared();
}

auto gse::acquire_exclusive(async::rw_mutex_registry& mutexes, const id type) -> async::task<> {
	auto& mutex = mutexes.mutex_for(type);
	co_await mutex.lock_exclusive();
}

auto gse::acquire_locks_in_sorted_order(async::rw_mutex_registry& mutexes, const std::span<const id> type_ids, const std::span<const lock_fn> fns, const id trace_id) -> async::task<> {
	constexpr std::size_t max_arity = 16;
	const std::size_t count = type_ids.size();
	assert(count <= max_arity, "acquire arity {} exceeds max {}", count, max_arity);

	std::array<std::size_t, max_arity> order_buf{};
	const std::span order(order_buf.data(), count);
	std::ranges::iota(
		order,
		std::size_t{ 0 }
	);
	std::ranges::sort(
		order,
		[type_ids](const std::size_t a, const std::size_t b) {
			return type_ids[a] < type_ids[b];
		}
	);

	const auto key = trace::allocate_async_key();
	trace::begin_async(trace_id, key);
	for (const std::size_t i : order) {
		co_await fns[i](mutexes, type_ids[i]);
	}
	trace::end_async(trace_id, key);
}

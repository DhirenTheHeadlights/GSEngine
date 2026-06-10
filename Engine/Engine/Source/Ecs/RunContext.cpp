module gse.ecs:context_impl;

import std;

import :context;

import gse.assert;
import gse.core;
import gse.concurrency;

// yield_tick is a NON-TEMPLATE coroutine. GCC trunk (+ -freflection) emits a corrupt
// BMI for an interface partition that contains a non-template coroutine body, so it
// lives in this implementation unit rather than RunContext.cppm. An importer otherwise
// fails with "failed to read compiled module cluster N: Bad file data". This matches the
// convention already used in Concurrency/AsyncTask.cpp.

auto gse::context::yield_tick() -> async::task<> {
	const int held = held_lock_count();
	assert(
		held == 0,
		"system held {} component access handle(s) across co_await ctx.yield_tick(); scope your read<>/write<>/structural<> so the handle is destroyed before yield_tick",
		held
	);
	m_paused_event.set();
	co_await m_resume_event.wait();
	m_resume_event.reset();
}

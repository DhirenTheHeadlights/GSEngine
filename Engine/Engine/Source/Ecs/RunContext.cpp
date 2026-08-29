module gse.ecs:context_impl;

import std;

import :context;

import gse.assert;
import gse.core;
import gse.concurrency;

auto gse::context::yield_tick() -> async::task<> {
	assert(
		m_resume_event != nullptr && m_paused_event != nullptr,
		"ctx.yield_tick() is only available inside init(); run() and frame() contexts have no resume protocol, so awaiting it would park the system forever"
	);
	const int held = held_lock_count();
	assert(
		held == 0,
		"system held {} component access handle(s) across co_await ctx.yield_tick(); scope your read<>/write<>/structural<> so the handle is destroyed before yield_tick",
		held
	);
	m_paused_event->set();
	co_await m_resume_event->wait();
	m_resume_event->reset();
}

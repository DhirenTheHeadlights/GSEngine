module gse.concurrency:task_impl;

import gse.win32;

import :task;

auto gse::task::apply_priority(const thread_priority priority) -> void {
	using namespace win32;
	switch (priority) {
		case thread_priority::normal:
			SetThreadPriority(GetCurrentThread(), thread_priority_normal);
			return;
		case thread_priority::below_normal:
			SetThreadPriority(GetCurrentThread(), thread_priority_below_normal);
			return;
	}
}

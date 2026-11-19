module gse.diag:watchdog_impl;

import std;

import :watchdog;

import gse.core;
import gse.math;
import gse.time;
import gse.log;

namespace gse::watchdog {
	std::atomic<id> active_section{ id{} };
	std::atomic<time> active_budget{ time{} };
	std::atomic<time> last_progress{ time{} };
	std::atomic<std::uint64_t> dump_pulse_count{ 0 };
	std::atomic<std::thread::id> owner_thread;
	std::atomic<bool> running{ false };
	std::atomic<bool> warned_off_thread{ false };
	std::jthread monitor;

	auto monitor_loop(
		const std::stop_token& st
	) -> void;
}

auto gse::watchdog::monitor_loop(const std::stop_token& st) -> void {
	constexpr std::chrono::milliseconds poll_interval{ 100 };

	time last_dump_elapsed{};
	int dumps = 0;

	while (!st.stop_requested()) {
		std::this_thread::sleep_for(poll_interval);

		const id section = active_section.load(std::memory_order_acquire);
		const time budget = active_budget.load(std::memory_order_acquire);
		const time elapsed = system_clock::now<time>() - last_progress.load(std::memory_order_acquire);

		if (!section.exists() || elapsed < budget) {
			dumps = 0;
			last_dump_elapsed = time{};
			continue;
		}

		if (dumps != 0 && elapsed - last_dump_elapsed < budget) {
			continue;
		}

		++dumps;
		last_dump_elapsed = elapsed;
		dump_pulse_count.fetch_add(1, std::memory_order_release);

		log::println(
			log::level::error,
			log::category::runtime,
			"WATCHDOG STALL [section={} elapsed={::ms} dump=#{}] watched section has not progressed",
			section,
			elapsed,
			dumps
		);

		if (dumps == 1) {
			if (const auto stacks = capture_all_thread_stacks(); !stacks.empty()) {
				log::println(log::level::error, log::category::runtime, "WATCHDOG thread stacks:\n{}", stacks);
			}
		}

		log::flush();
	}
}

auto gse::watchdog::start() -> void {
	if (running.exchange(true, std::memory_order_acq_rel)) {
		return;
	}

	owner_thread.store(std::this_thread::get_id(), std::memory_order_release);
	active_section.store(id{}, std::memory_order_release);
	last_progress.store(system_clock::now<time>(), std::memory_order_release);
	open_stack_capture();

	monitor = std::jthread([](std::stop_token st) {
		log::name_thread(log::thread_role::watchdog);
		monitor_loop(st);
	});
}

auto gse::watchdog::stop() -> void {
	if (!running.exchange(false, std::memory_order_acq_rel)) {
		return;
	}

	monitor = {};
	close_stack_capture();
}

auto gse::watchdog::dump_pulse() -> std::uint64_t {
	return dump_pulse_count.load(std::memory_order_acquire);
}

gse::watchdog::section::section(const id section_id, const time budget) {
	if (!running.load(std::memory_order_acquire)) {
		return;
	}

	if (std::this_thread::get_id() != owner_thread.load(std::memory_order_acquire)) {
		if (!warned_off_thread.exchange(true, std::memory_order_relaxed)) {
			log::println(
				log::level::warning,
				log::category::runtime,
				"watchdog::section {} constructed off the loop thread; stall tracking skipped",
				section_id
			);
		}
		return;
	}

	m_owned = true;
	m_prev_section = active_section.load(std::memory_order_acquire);
	m_prev_budget = active_budget.load(std::memory_order_acquire);

	active_section.store(section_id, std::memory_order_release);
	active_budget.store(budget, std::memory_order_release);
	last_progress.store(system_clock::now<time>(), std::memory_order_release);
}

gse::watchdog::section::~section() {
	if (!m_owned) {
		return;
	}

	active_section.store(m_prev_section, std::memory_order_release);
	active_budget.store(m_prev_budget, std::memory_order_release);
	last_progress.store(system_clock::now<time>(), std::memory_order_release);
}

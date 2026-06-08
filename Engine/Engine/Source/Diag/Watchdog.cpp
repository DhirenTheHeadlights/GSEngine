module gse.diag:watchdog_impl;

import std;

import :watchdog;

import gse.core;
import gse.math;
import gse.time;
import gse.log;
import gse.win32;
import gse.cxxabi;

namespace gse::watchdog {
	constexpr std::size_t max_frames = 64;

	std::atomic<id> active_section{ id{} };
	std::atomic<time> active_budget{ time{} };
	std::atomic<time> last_progress{ time{} };
	std::atomic<std::uint64_t> dump_pulse_count{ 0 };
	std::jthread monitor;
	void* loop_thread = nullptr;

	auto monitor_loop(
		const std::stop_token& st
	) -> void;

	auto walk_thread(
		void* thread,
		std::array<std::uint64_t, max_frames>& program_counters
	) -> std::size_t;

	auto symbolize_frames(
		const std::array<std::uint64_t, max_frames>& program_counters,
		std::size_t count
	) -> std::string;

	auto capture_all_thread_stacks() -> std::string;
}

auto gse::watchdog::monitor_loop(const std::stop_token& st) -> void {
	time last_dump_elapsed{};
	int dumps = 0;

	while (!st.stop_requested()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
			"WATCHDOG STALL [section={} elapsed={::ms} dump=#{}] main loop has not progressed",
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
	active_section.store(id{}, std::memory_order_release);
	last_progress.store(system_clock::now<time>(), std::memory_order_release);

#ifdef _WIN32
	using namespace gse::win32;
	DuplicateHandle(
		GetCurrentProcess(),
		GetCurrentThread(),
		GetCurrentProcess(),
		reinterpret_cast<HANDLE*>(&loop_thread),
		0,
		0,
		duplicate_same_access
	);
	SymSetOptions(symopt_deferred_loads | symopt_load_lines);
	SymInitialize(GetCurrentProcess(), nullptr, 1);
#endif

	monitor = std::jthread([](std::stop_token st) {
		monitor_loop(st);
	});
}

auto gse::watchdog::stop() -> void {
	monitor = {};

#ifdef _WIN32
	using namespace gse::win32;
	if (loop_thread != nullptr) {
		CloseHandle(static_cast<HANDLE>(loop_thread));
		loop_thread = nullptr;
	}
	SymCleanup(GetCurrentProcess());
#endif
}

auto gse::watchdog::dump_pulse() -> std::uint64_t {
	return dump_pulse_count.load(std::memory_order_acquire);
}

gse::watchdog::section::section(const id section_id, const time budget)
	: m_prev_section(active_section.load(std::memory_order_acquire)), m_prev_budget(active_budget.load(std::memory_order_acquire)) {
	active_section.store(section_id, std::memory_order_release);
	active_budget.store(budget, std::memory_order_release);
	last_progress.store(system_clock::now<time>(), std::memory_order_release);
}

gse::watchdog::section::~section() {
	active_section.store(m_prev_section, std::memory_order_release);
	active_budget.store(m_prev_budget, std::memory_order_release);
	last_progress.store(system_clock::now<time>(), std::memory_order_release);
}

#ifdef _WIN32
auto gse::watchdog::walk_thread(void* thread, std::array<std::uint64_t, max_frames>& program_counters) -> std::size_t {
	using namespace gse::win32;

	const auto handle = static_cast<HANDLE>(thread);
	std::size_t count = 0;

	if (SuspendThread(handle) == static_cast<DWORD>(-1)) {
		return 0;
	}

	CONTEXT context{};
	context.ContextFlags = context_full;
	if (GetThreadContext(handle, &context)) {
		while (count < program_counters.size() && context.Rip != 0) {
			program_counters[count++] = context.Rip;

			DWORD64 image_base = 0;
			const auto function_entry = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
			if (function_entry == nullptr) {
				break;
			}

			PVOID handler_data = nullptr;
			DWORD64 establisher_frame = 0;
			RtlVirtualUnwind(unw_flag_nhandler, image_base, context.Rip, function_entry, &context, &handler_data, &establisher_frame, nullptr);
		}
	}

	ResumeThread(handle);
	return count;
}

auto gse::watchdog::symbolize_frames(const std::array<std::uint64_t, max_frames>& program_counters, const std::size_t count) -> std::string {
	using namespace gse::win32;

	const auto process = GetCurrentProcess();

	std::string out;
	for (std::size_t i = 0; i < count; ++i) {
		const auto pc = program_counters[i];
		std::string module_name = "?";
		std::uint64_t module_offset = pc;

		HMODULE module_handle = nullptr;
		if (GetModuleHandleExW(get_module_handle_ex_flag_from_address | get_module_handle_ex_flag_unchanged_refcount, reinterpret_cast<const wchar_t*>(pc), &module_handle) && module_handle != nullptr) {
			wchar_t path[max_path];
			if (const auto length = GetModuleFileNameW(module_handle, path, max_path); length > 0) {
				std::wstring_view view(path, length);
				if (const auto slash = view.find_last_of(L"\\/"); slash != std::wstring_view::npos) {
					view = view.substr(slash + 1);
				}
				module_name.clear();
				for (const wchar_t character : view) {
					module_name.push_back(static_cast<char>(character));
				}
			}

			module_offset = pc - reinterpret_cast<std::uintptr_t>(module_handle);
		}

		std::string symbol_text;
		alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + max_sym_name] = {};
		auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = max_sym_name;

		DWORD64 symbol_displacement = 0;
		if (SymFromAddr(process, pc, &symbol_displacement, symbol)) {
			int status = 0;
			char* demangled = cxxabi::__cxa_demangle(symbol->Name, nullptr, nullptr, &status);
			symbol_text = status == 0 && demangled != nullptr ? demangled : symbol->Name;
			std::free(demangled);

			IMAGEHLP_LINE64 line{};
			line.SizeOfStruct = sizeof(line);
			DWORD line_displacement = 0;
			if (SymGetLineFromAddr64(process, pc, &line_displacement, &line)) {
				symbol_text += std::format(" ({}:{})", line.FileName, line.LineNumber);
			}
		}

		if (symbol_text.empty()) {
			out += std::format("      #{:<2} {} +0x{:x}\n", i, module_name, module_offset);
		}
		else {
			out += std::format("      #{:<2} {} +0x{:x}  {}\n", i, module_name, module_offset, symbol_text);
		}
	}

	return out;
}

auto gse::watchdog::capture_all_thread_stacks() -> std::string {
	using namespace gse::win32;

	const auto current_thread = GetCurrentThreadId();
	const auto loop_tid = loop_thread != nullptr ? GetThreadId(static_cast<HANDLE>(loop_thread)) : DWORD{ 0 };

	const auto snapshot = CreateToolhelp32Snapshot(th32cs_snapthread, 0);
	if (!valid_handle(snapshot)) {
		return {};
	}

	const auto pid = GetCurrentProcessId();

	struct thread_capture {
		DWORD tid;
		bool is_loop;
		std::array<std::uint64_t, max_frames> program_counters;
		std::size_t count;
	};
	std::vector<thread_capture> captures;

	THREADENTRY32 entry{};
	entry.dwSize = sizeof(entry);
	if (Thread32First(snapshot, &entry)) {
		do {
			if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == current_thread) {
				continue;
			}

			const auto thread = OpenThread(thread_suspend_resume | thread_get_context | thread_query_information, 0, entry.th32ThreadID);
			if (thread == nullptr) {
				continue;
			}

			thread_capture capture{ .tid = entry.th32ThreadID, .is_loop = entry.th32ThreadID == loop_tid };
			capture.count = walk_thread(thread, capture.program_counters);
			CloseHandle(thread);
			captures.push_back(std::move(capture));
		} while (Thread32Next(snapshot, &entry));
	}

	CloseHandle(snapshot);

	std::string out;
	for (const auto& capture : captures) {
		out += std::format("  thread 0x{:x}{}:\n", capture.tid, capture.is_loop ? " (loop)" : "");
		out += symbolize_frames(capture.program_counters, capture.count);
	}

	return out;
}
#else
auto gse::watchdog::capture_all_thread_stacks() -> std::string {
	return {};
}
#endif

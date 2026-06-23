module gse.stacktrace;

import std;

import gse.log;
import gse.meta;
import gse.win32;

auto gse::capture_stacktrace(const std::size_t skip_frames) -> std::string {
	const auto trace = std::stacktrace::current(skip_frames);
	std::string out;
	out.reserve(trace.size() * 64);
	for (std::size_t i = 0; i < trace.size(); ++i) {
		out += std::format("  #{:>2} {}\n", i, std::to_string(trace[i]));
	}
	return out;
}

#ifdef _WIN32
namespace gse {
	using namespace gse::win32;

	constexpr std::size_t max_trace_frames = 64;

	std::atomic<bool> handler_active{ false };

	enum class seh_code : std::uint32_t {
		access_violation = exception_access_violation,
		array_bounds_exceeded = exception_array_bounds_exceeded,
		datatype_misalignment = exception_datatype_misalignment,
		flt_denormal_operand = exception_flt_denormal_operand,
		flt_divide_by_zero = exception_flt_divide_by_zero,
		flt_inexact_result = exception_flt_inexact_result,
		flt_invalid_operation = exception_flt_invalid_operation,
		flt_overflow = exception_flt_overflow,
		flt_stack_check = exception_flt_stack_check,
		flt_underflow = exception_flt_underflow,
		illegal_instruction = exception_illegal_instruction,
		in_page_error = exception_in_page_error,
		int_divide_by_zero = exception_int_divide_by_zero,
		int_overflow = exception_int_overflow,
		invalid_disposition = exception_invalid_disposition,
		noncontinuable_exception = exception_noncontinuable_exception,
		priv_instruction = exception_priv_instruction,
		stack_overflow = exception_stack_overflow,
	};

	constexpr DWORD cpp_exception_code = 0xE06D7363;
	constexpr DWORD breakpoint_code = 0x80000003;
	constexpr DWORD dbg_print_exception_code = 0x40010006;
	constexpr DWORD dbg_print_exception_wide_c = 0x4001000A;
	constexpr DWORD ms_thread_name_code = 0x406D1388;

	auto module_frame(const std::uintptr_t pc) -> std::string {
		std::string module_name = "?";
		std::uintptr_t module_offset = pc;

		HMODULE module_handle = nullptr;
		if (pc != 0 && GetModuleHandleExW(get_module_handle_ex_flag_from_address | get_module_handle_ex_flag_unchanged_refcount, reinterpret_cast<const wchar_t*>(pc), &module_handle) && module_handle != nullptr) {
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

		return std::format("{}+0x{:x}", module_name, module_offset);
	}

	auto log_context_stacktrace(CONTEXT context) -> void {
		std::size_t count = 0;

		while (count < max_trace_frames) {
			log::println(
				log::level::error,
				log::category::general,
				"  #{:>2} 0x{:016x}  {}",
				count,
				static_cast<std::uint64_t>(context.Rip),
				module_frame(context.Rip)
			);
			log::flush();
			++count;

			DWORD64 image_base = 0;
			const auto function_entry = context.Rip != 0
				? RtlLookupFunctionEntry(context.Rip, &image_base, nullptr)
				: nullptr;

			if (function_entry != nullptr) {
				PVOID handler_data = nullptr;
				DWORD64 establisher_frame = 0;
				RtlVirtualUnwind(unw_flag_nhandler, image_base, context.Rip, function_entry, &context, &handler_data, &establisher_frame, nullptr);
				if (context.Rip == 0) {
					break;
				}
				continue;
			}

			if (context.Rsp == 0) {
				break;
			}
			const auto return_address = *reinterpret_cast<const DWORD64*>(context.Rsp);
			context.Rsp += sizeof(DWORD64);
			if (return_address == 0) {
				break;
			}
			context.Rip = return_address;
		}
	}

	LONG vectored_handler(EXCEPTION_POINTERS* info) noexcept {
		const auto* rec = info->ExceptionRecord;
		const auto code = rec->ExceptionCode;
		if (code == cpp_exception_code || code == breakpoint_code || code == dbg_print_exception_code || code == dbg_print_exception_wide_c || code == ms_thread_name_code) {
			return exception_continue_search;
		}

		bool expected = false;
		if (!handler_active.compare_exchange_strong(expected, true)) {
			return exception_continue_search;
		}

		std::string detail;
		if (code == exception_access_violation && rec->NumberParameters >= 2) {
			const auto op = rec->ExceptionInformation[0];
			const auto addr = rec->ExceptionInformation[1];
			const std::string_view op_name = op == 0 ? "read" : op == 1 ? "write"
				: op == 8												? "execute"
																		: "unknown";
			detail = std::format(" ({} at 0x{:016x})", op_name, addr);
		}

		const auto pc = reinterpret_cast<std::uintptr_t>(rec->ExceptionAddress);
		std::uintptr_t module_offset = pc;
		HMODULE module_handle = nullptr;
		if (GetModuleHandleExW(get_module_handle_ex_flag_from_address | get_module_handle_ex_flag_unchanged_refcount, reinterpret_cast<const wchar_t*>(pc), &module_handle) && module_handle != nullptr) {
			module_offset = pc - reinterpret_cast<std::uintptr_t>(module_handle);
		}

		log::println(
			log::level::error,
			log::category::general,
			"[SEH] code=0x{:08x} ({}){} at PC=0x{:016x} (module+0x{:x}) thread={:#x}\nStack:",
			static_cast<std::uint32_t>(code),
			enum_to_string(static_cast<seh_code>(code)),
			detail,
			pc,
			module_offset,
			std::hash<std::thread::id>{}(std::this_thread::get_id())
		);
		log::flush();

		log_context_stacktrace(*info->ContextRecord);

		handler_active.store(false);
		return exception_continue_search;
	}
}
#endif

auto gse::install_crash_handlers() -> void {
#ifdef _WIN32
	using namespace gse::win32;
	const auto* handle = AddVectoredExceptionHandler(1, &vectored_handler);
	log::println(
		log::level::info,
		log::category::general,
		"install_crash_handlers: VEH registered (handle={}, last_error={})",
		handle,
		handle == nullptr ? GetLastError() : 0u
	);
#endif
}

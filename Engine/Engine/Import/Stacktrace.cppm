module;

#ifdef _WIN32
#include <x86intrin.h>
#endif

export module gse.stacktrace;

import std;
import gse.log;
import gse.meta;
import gse.win32;

export namespace gse {
	auto capture_stacktrace(
		std::size_t skip_frames = 1
	) -> std::string;

	auto install_crash_handlers() -> void;
}

auto gse::capture_stacktrace(const std::size_t skip_frames) -> std::string {
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
	const auto trace = std::stacktrace::current(skip_frames);
	std::string out;
	out.reserve(trace.size() * 64);
	for (std::size_t i = 0; i < trace.size(); ++i) {
		out += std::format("  #{:>2} {}\n", i, std::to_string(trace[i]));
	}
	return out;
#else
	(void)skip_frames;
	return std::string{ "  <std::stacktrace unavailable in this stdlib build>\n" };
#endif
}

#ifdef _WIN32
namespace gse {
	using namespace gse::win32;

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

	LONG vectored_handler(EXCEPTION_POINTERS* info) noexcept {
		const auto* rec = info->ExceptionRecord;
		const auto code = rec->ExceptionCode;
		if (code == cpp_exception_code || code == breakpoint_code || code == dbg_print_exception_code || code == dbg_print_exception_wide_c || code == ms_thread_name_code) {
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

		log::println(
			log::level::error,
			log::category::general,
			"[SEH] code=0x{:08x} ({}){} at PC=0x{:016x} thread={:#x}\nStack:\n{}",
			static_cast<std::uint32_t>(code),
			enum_to_string(static_cast<seh_code>(code)),
			detail,
			reinterpret_cast<std::uintptr_t>(rec->ExceptionAddress),
			std::hash<std::thread::id>{}(std::this_thread::get_id()),
			capture_stacktrace(1)
		);
		log::flush();
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

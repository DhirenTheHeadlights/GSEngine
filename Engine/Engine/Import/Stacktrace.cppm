module;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

export module gse.stacktrace;

import std;
import gse.log;
import gse.meta;

export namespace gse {
	auto capture_stacktrace(std::size_t skip_frames = 1) -> std::string;

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
	enum class seh_code : std::uint32_t {
		access_violation = EXCEPTION_ACCESS_VIOLATION,
		array_bounds_exceeded = EXCEPTION_ARRAY_BOUNDS_EXCEEDED,
		datatype_misalignment = EXCEPTION_DATATYPE_MISALIGNMENT,
		flt_denormal_operand = EXCEPTION_FLT_DENORMAL_OPERAND,
		flt_divide_by_zero = EXCEPTION_FLT_DIVIDE_BY_ZERO,
		flt_inexact_result = EXCEPTION_FLT_INEXACT_RESULT,
		flt_invalid_operation = EXCEPTION_FLT_INVALID_OPERATION,
		flt_overflow = EXCEPTION_FLT_OVERFLOW,
		flt_stack_check = EXCEPTION_FLT_STACK_CHECK,
		flt_underflow = EXCEPTION_FLT_UNDERFLOW,
		illegal_instruction = EXCEPTION_ILLEGAL_INSTRUCTION,
		in_page_error = EXCEPTION_IN_PAGE_ERROR,
		int_divide_by_zero = EXCEPTION_INT_DIVIDE_BY_ZERO,
		int_overflow = EXCEPTION_INT_OVERFLOW,
		invalid_disposition = EXCEPTION_INVALID_DISPOSITION,
		noncontinuable_exception = EXCEPTION_NONCONTINUABLE_EXCEPTION,
		priv_instruction = EXCEPTION_PRIV_INSTRUCTION,
		stack_overflow = EXCEPTION_STACK_OVERFLOW,
	};

	constexpr DWORD cpp_exception_code = 0xE06D7363;
	constexpr DWORD breakpoint_code = 0x80000003;
	constexpr DWORD dbg_print_exception_code = 0x40010006;
	constexpr DWORD dbg_print_exception_wide_c = 0x4001000A;
	constexpr DWORD ms_thread_name_code = 0x406D1388;

	LONG WINAPI vectored_handler(EXCEPTION_POINTERS* info) noexcept {
		const auto* rec = info->ExceptionRecord;
		const auto code = rec->ExceptionCode;
		if (
			code == cpp_exception_code || code == breakpoint_code || code == dbg_print_exception_code ||
			code == dbg_print_exception_wide_c || code == ms_thread_name_code
		) {
			return EXCEPTION_CONTINUE_SEARCH;
		}

		std::string detail;
		if (code == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
			const auto op = rec->ExceptionInformation[0];
			const auto addr = rec->ExceptionInformation[1];
			const std::string_view op_name = op == 0 ? "read" : op == 1 ? "write" : op == 8 ? "execute" : "unknown";
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
		return EXCEPTION_CONTINUE_SEARCH;
	}
}
#endif

auto gse::install_crash_handlers() -> void {
#ifdef _WIN32
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

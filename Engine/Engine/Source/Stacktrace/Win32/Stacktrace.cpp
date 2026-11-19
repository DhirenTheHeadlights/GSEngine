module gse.stacktrace;

import std;

import gse.log;
import gse.meta;
import gse.win32;

namespace gse {
	using namespace gse::win32;

	constexpr std::size_t max_trace_frames = 64;

	std::atomic<std::thread::id> fatal_owner{};

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

	std::atomic<crash_responder> installed_crash_responder = nullptr;
	std::atomic<bool> crash_responded{ false };

	auto context_frames(
		CONTEXT context
	) -> std::vector<std::string>;

	auto self_module_base() -> std::uintptr_t;

	auto run_crash_responder(
		const crash_record& record
	) -> void;

	auto unhandled_handler(
		EXCEPTION_POINTERS* info
	) noexcept -> LONG;

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

	auto step_context(CONTEXT& context) -> bool {
		DWORD64 image_base = 0;
		const auto function_entry = context.Rip != 0
			? RtlLookupFunctionEntry(context.Rip, &image_base, nullptr)
			: nullptr;

		if (function_entry != nullptr) {
			PVOID handler_data = nullptr;
			DWORD64 establisher_frame = 0;
			RtlVirtualUnwind(unw_flag_nhandler, image_base, context.Rip, function_entry, &context, &handler_data, &establisher_frame, nullptr);
			return context.Rip != 0;
		}

		if (context.Rsp == 0) {
			return false;
		}
		const auto return_address = *reinterpret_cast<const DWORD64*>(context.Rsp);
		context.Rsp += sizeof(DWORD64);
		if (return_address == 0) {
			return false;
		}
		context.Rip = return_address;
		return true;
	}

	constexpr std::array<DWORD, 10> hard_fault_codes = {
		exception_access_violation,
		exception_illegal_instruction,
		exception_priv_instruction,
		exception_in_page_error,
		exception_stack_overflow,
		exception_int_divide_by_zero,
		exception_array_bounds_exceeded,
		exception_datatype_misalignment,
		exception_noncontinuable_exception,
		exception_invalid_disposition,
	};

	auto respond_to_fatal_exception(const EXCEPTION_POINTERS* info) noexcept -> void {
		const std::thread::id self = std::this_thread::get_id();
		if (std::thread::id unowned{}; !fatal_owner.compare_exchange_strong(unowned, self)) {
			if (unowned == self) {
				std::_Exit(3);
			}
			while (true) {
				std::this_thread::sleep_for(std::chrono::hours(1));
			}
		}

		const auto* rec = info->ExceptionRecord;
		const auto code = rec->ExceptionCode;

		std::string access;
		if (code == exception_access_violation && rec->NumberParameters >= 2) {
			const auto op = rec->ExceptionInformation[0];
			const std::string_view op_name = op == 0 ? "read" : op == 1 ? "write"
				: op == 8												? "execute"
																		: "unknown";
			access = std::format(" ({} at 0x{:016x})", op_name, rec->ExceptionInformation[1]);
		}

		const crash_record record{
			.reason = std::format("{} (0x{:08x})", enum_to_string(static_cast<seh_code>(code)), static_cast<std::uint32_t>(code)),
			.detail = std::format("at {}{}", module_frame(reinterpret_cast<std::uintptr_t>(rec->ExceptionAddress)), access),
			.thread = std::format("{:#x}", std::hash<std::thread::id>{}(std::this_thread::get_id())),
			.frames = context_frames(*info->ContextRecord),
			.module_base = self_module_base(),
		};

		std::string stack;
		for (const std::string& frame : record.frames) {
			stack += std::format("  {}\n", frame);
		}
		log::println(log::level::error, log::category::general, "[SEH] {} {} thread={}\nStack:\n{}", record.reason, record.detail, record.thread, stack);
		log::flush();

		run_crash_responder(record);
		std::_Exit(3);
	}

	LONG vectored_handler(EXCEPTION_POINTERS* info) noexcept {
		if (!std::ranges::contains(hard_fault_codes, info->ExceptionRecord->ExceptionCode)) {
			return exception_continue_search;
		}

		respond_to_fatal_exception(info);
		return exception_continue_search;
	}
}

auto gse::context_frames(CONTEXT context) -> std::vector<std::string> {
	std::vector<std::string> out;
	while (out.size() < max_trace_frames) {
		out.push_back(std::format("#{:>2} {}", out.size(), module_frame(context.Rip)));
		if (!step_context(context)) {
			break;
		}
	}
	return out;
}

auto gse::self_module_base() -> std::uintptr_t {
	return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
}

auto gse::run_crash_responder(const crash_record& record) -> void {
	const crash_responder responder = installed_crash_responder.load(std::memory_order_acquire);
	if (!responder) {
		return;
	}

	bool expected = false;
	if (crash_responded.compare_exchange_strong(expected, true)) {
		responder(record);
	}
}

auto gse::unhandled_handler(EXCEPTION_POINTERS* info) noexcept -> LONG {
	respond_to_fatal_exception(info);
	return exception_execute_handler;
}

auto gse::capture_stacktrace(const std::size_t skip_frames) -> std::string {
	using namespace gse::win32;

	CONTEXT context{};
	RtlCaptureContext(&context);

	std::string out;
	std::size_t walked = 0;
	std::size_t shown = 0;
	while (walked < max_trace_frames && context.Rip != 0) {
		if (walked >= skip_frames) {
			out += std::format("  #{:>2} {}\n", shown, module_frame(context.Rip));
			++shown;
		}
		++walked;

		if (!step_context(context)) {
			break;
		}
	}
	return out;
}

auto gse::install_crash_handlers() -> void {
	using namespace gse::win32;
	const auto* handle = AddVectoredExceptionHandler(1, &vectored_handler);
	SetUnhandledExceptionFilter(&unhandled_handler);
	log::println(
		log::level::info,
		log::category::general,
		"install_crash_handlers: VEH registered (handle={}, last_error={})",
		handle,
		handle == nullptr ? GetLastError() : 0u
	);
}

auto gse::install_crash_responder(const crash_responder responder) -> void {
	installed_crash_responder.store(responder, std::memory_order_release);
}

auto gse::report_crash(std::string reason, std::string detail) -> void {
	CONTEXT context{};
	RtlCaptureContext(&context);

	run_crash_responder({
		.reason = std::move(reason),
		.detail = std::move(detail),
		.thread = std::format("{:#x}", std::hash<std::thread::id>{}(std::this_thread::get_id())),
		.frames = context_frames(context),
		.module_base = self_module_base(),
	});
}

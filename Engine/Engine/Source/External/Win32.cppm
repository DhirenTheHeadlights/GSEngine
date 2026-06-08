module;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

export module gse.win32;

#ifdef _WIN32
export namespace gse::win32 {
	using ::HWND;
	using ::WNDPROC;
	using ::LRESULT;
	using ::WPARAM;
	using ::LPARAM;
	using ::UINT;
	using ::DWORD;
	using ::LONG_PTR;
	using ::POINT;
	using ::RECT;
	using ::HMONITOR;
	using ::MONITORINFO;
	using ::NCCALCSIZE_PARAMS;

	using ::glfwGetWin32Window;
	using ::DefWindowProcW;
	using ::CallWindowProcW;
	using ::SetWindowLongPtrW;
	using ::SetWindowPos;
	using ::GetPropW;
	using ::SetPropW;
	using ::IsZoomed;
	using ::MonitorFromWindow;
	using ::GetMonitorInfoW;
	using ::ScreenToClient;
	using ::GetClientRect;

	constexpr UINT wm_nccalcsize = WM_NCCALCSIZE;
	constexpr UINT wm_nchittest = WM_NCHITTEST;
	constexpr LRESULT ht_left = HTLEFT;
	constexpr LRESULT ht_right = HTRIGHT;
	constexpr LRESULT ht_top = HTTOP;
	constexpr LRESULT ht_bottom = HTBOTTOM;
	constexpr LRESULT ht_top_left = HTTOPLEFT;
	constexpr LRESULT ht_top_right = HTTOPRIGHT;
	constexpr LRESULT ht_bottom_left = HTBOTTOMLEFT;
	constexpr LRESULT ht_bottom_right = HTBOTTOMRIGHT;
	constexpr LRESULT ht_caption = HTCAPTION;
	constexpr LRESULT ht_client = HTCLIENT;
	constexpr int gwlp_wndproc = GWLP_WNDPROC;
	constexpr UINT swp_frame_changed = SWP_FRAMECHANGED;
	constexpr UINT swp_no_move = SWP_NOMOVE;
	constexpr UINT swp_no_size = SWP_NOSIZE;
	constexpr UINT swp_no_zorder = SWP_NOZORDER;
	constexpr UINT swp_no_activate = SWP_NOACTIVATE;
	constexpr DWORD monitor_default_to_nearest = MONITOR_DEFAULTTONEAREST;

	auto get_x_lparam(LPARAM lparam) -> int {
		return GET_X_LPARAM(lparam);
	}

	auto get_y_lparam(LPARAM lparam) -> int {
		return GET_Y_LPARAM(lparam);
	}

	using ::HANDLE;
	using ::HMODULE;
	using ::LONG;
	using ::PVOID;
	using ::DWORD64;
	using ::CONTEXT;
	using ::PRUNTIME_FUNCTION;
	using ::EXCEPTION_POINTERS;
	using ::EXCEPTION_RECORD;
	using ::STARTUPINFOW;
	using ::PROCESS_INFORMATION;

	using ::DuplicateHandle;
	using ::GetCurrentProcess;
	using ::GetCurrentThread;
	using ::CloseHandle;
	using ::SuspendThread;
	using ::ResumeThread;
	using ::GetThreadContext;
	using ::RtlLookupFunctionEntry;
	using ::RtlVirtualUnwind;
	using ::GetModuleHandleExW;
	using ::GetModuleFileNameW;
	using ::AddVectoredExceptionHandler;
	using ::GetLastError;
	using ::CreateProcessW;
	using ::GetCommandLineW;
	using ::ExitProcess;

	constexpr DWORD context_full = CONTEXT_FULL;
	constexpr DWORD unw_flag_nhandler = UNW_FLAG_NHANDLER;
	constexpr DWORD duplicate_same_access = DUPLICATE_SAME_ACCESS;
	constexpr DWORD get_module_handle_ex_flag_from_address = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
	constexpr DWORD get_module_handle_ex_flag_unchanged_refcount = GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	constexpr int max_path = MAX_PATH;

	constexpr LONG exception_continue_search = EXCEPTION_CONTINUE_SEARCH;
	constexpr DWORD exception_access_violation = EXCEPTION_ACCESS_VIOLATION;
	constexpr DWORD exception_array_bounds_exceeded = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
	constexpr DWORD exception_datatype_misalignment = EXCEPTION_DATATYPE_MISALIGNMENT;
	constexpr DWORD exception_flt_denormal_operand = EXCEPTION_FLT_DENORMAL_OPERAND;
	constexpr DWORD exception_flt_divide_by_zero = EXCEPTION_FLT_DIVIDE_BY_ZERO;
	constexpr DWORD exception_flt_inexact_result = EXCEPTION_FLT_INEXACT_RESULT;
	constexpr DWORD exception_flt_invalid_operation = EXCEPTION_FLT_INVALID_OPERATION;
	constexpr DWORD exception_flt_overflow = EXCEPTION_FLT_OVERFLOW;
	constexpr DWORD exception_flt_stack_check = EXCEPTION_FLT_STACK_CHECK;
	constexpr DWORD exception_flt_underflow = EXCEPTION_FLT_UNDERFLOW;
	constexpr DWORD exception_illegal_instruction = EXCEPTION_ILLEGAL_INSTRUCTION;
	constexpr DWORD exception_in_page_error = EXCEPTION_IN_PAGE_ERROR;
	constexpr DWORD exception_int_divide_by_zero = EXCEPTION_INT_DIVIDE_BY_ZERO;
	constexpr DWORD exception_int_overflow = EXCEPTION_INT_OVERFLOW;
	constexpr DWORD exception_invalid_disposition = EXCEPTION_INVALID_DISPOSITION;
	constexpr DWORD exception_noncontinuable_exception = EXCEPTION_NONCONTINUABLE_EXCEPTION;
	constexpr DWORD exception_priv_instruction = EXCEPTION_PRIV_INSTRUCTION;
	constexpr DWORD exception_stack_overflow = EXCEPTION_STACK_OVERFLOW;

	using ::SYMBOL_INFO;
	using ::IMAGEHLP_LINE64;

	using ::SymInitialize;
	using ::SymSetOptions;
	using ::SymFromAddr;
	using ::SymGetLineFromAddr64;
	using ::SymCleanup;

	constexpr DWORD symopt_load_lines = SYMOPT_LOAD_LINES;
	constexpr DWORD symopt_deferred_loads = SYMOPT_DEFERRED_LOADS;
	constexpr int max_sym_name = MAX_SYM_NAME;

	using ::THREADENTRY32;

	using ::CreateToolhelp32Snapshot;
	using ::Thread32First;
	using ::Thread32Next;
	using ::OpenThread;
	using ::GetThreadId;
	using ::GetCurrentThreadId;
	using ::GetCurrentProcessId;

	constexpr DWORD th32cs_snapthread = TH32CS_SNAPTHREAD;
	constexpr DWORD thread_suspend_resume = THREAD_SUSPEND_RESUME;
	constexpr DWORD thread_get_context = THREAD_GET_CONTEXT;
	constexpr DWORD thread_query_information = THREAD_QUERY_INFORMATION;

	auto valid_handle(HANDLE handle) -> bool {
		return handle != nullptr && handle != INVALID_HANDLE_VALUE;
	}
}
#endif

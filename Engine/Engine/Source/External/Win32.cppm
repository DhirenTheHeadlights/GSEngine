module;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <compressapi.h>
#endif

export module gse.win32;

#ifdef _WIN32
export namespace gse::win32 {
	[[nodiscard]] auto performance_counter_frequency() -> unsigned long long;

	[[nodiscard]] auto performance_counter() -> unsigned long long;

	[[nodiscard]] auto compressed_size_bound(
		const void* input,
		std::size_t input_size
	) -> std::size_t;

	[[nodiscard]] auto compress_lzms(
		const void* input,
		std::size_t input_size,
		void* output,
		std::size_t output_capacity,
		std::size_t* written
	) -> bool;

	[[nodiscard]] auto decompress_lzms(
		const void* input,
		std::size_t input_size,
		void* output,
		std::size_t output_size
	) -> bool;

	auto write_user_registry_string(
		const wchar_t* subkey,
		const wchar_t* name,
		const wchar_t* value
	) -> bool;

	auto delete_user_registry_key(
		const wchar_t* subkey
	) -> bool;

	auto show_error_box(
		const wchar_t* title,
		const wchar_t* text
	) -> void;

	auto enable_per_monitor_dpi_awareness() -> bool;

	[[nodiscard]] auto dpi_for_window(
		::HWND window
	) -> unsigned int;

	auto adjust_window_rect_for_dpi(
		::RECT* rect,
		::DWORD style,
		::DWORD ex_style,
		unsigned int dpi
	) -> void;

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
	using ::HCURSOR;
	using ::HRAWINPUT;
	using ::RAWINPUT;
	using ::RAWINPUTDEVICE;
	using ::USHORT;

	using ::DefWindowProcW;
	using ::SetWindowLongPtrW;
	using ::SetWindowPos;
	using ::IsZoomed;
	using ::IsIconic;
	using ::MonitorFromWindow;
	using ::MonitorFromRect;
	using ::GetMonitorInfoW;
	using ::ScreenToClient;
	using ::GetClientRect;
	using ::GetWindowRect;
	using ::WINDOWPLACEMENT;
	using ::GetWindowPlacement;
	using ::IsWindowVisible;
	using ::DwmGetWindowAttribute;
	using ::DwmSetWindowAttribute;
	using ::WindowFromPoint;
	using ::GetAncestor;
	using ::ClientToScreen;
	using ::ClipCursor;
	using ::GetCursorPos;
	using ::SetCursor;
	using ::LoadCursorW;
	using ::SendMessageW;
	using ::RegisterRawInputDevices;
	using ::GetRawInputData;

	constexpr UINT wm_nccalcsize = WM_NCCALCSIZE;
	constexpr UINT wm_nchittest = WM_NCHITTEST;
	constexpr UINT wm_ncmousemove = WM_NCMOUSEMOVE;
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
	constexpr UINT swp_frame_changed = SWP_FRAMECHANGED;
	constexpr UINT swp_no_move = SWP_NOMOVE;
	constexpr UINT swp_no_size = SWP_NOSIZE;
	constexpr UINT swp_no_zorder = SWP_NOZORDER;
	constexpr UINT swp_no_activate = SWP_NOACTIVATE;
	constexpr DWORD monitor_default_to_nearest = MONITOR_DEFAULTTONEAREST;
	constexpr UINT sw_show_maximized = SW_SHOWMAXIMIZED;
	constexpr UINT wpf_restore_to_maximized = WPF_RESTORETOMAXIMIZED;
	constexpr DWORD dwmwa_cloaked = DWMWA_CLOAKED;
	constexpr DWORD dwmwa_cloak = DWMWA_CLOAK;
	constexpr DWORD dwmwa_use_immersive_dark_mode = DWMWA_USE_IMMERSIVE_DARK_MODE;
	constexpr DWORD dwmwa_window_corner_preference = DWMWA_WINDOW_CORNER_PREFERENCE;
	constexpr DWORD dwmwcp_round = DWMWCP_ROUND;
	constexpr UINT ga_root = GA_ROOT;
	constexpr UINT wm_entersizemove = WM_ENTERSIZEMOVE;
	constexpr UINT wm_exitsizemove = WM_EXITSIZEMOVE;
	constexpr UINT wm_setcursor = WM_SETCURSOR;
	constexpr UINT wm_mousemove = WM_MOUSEMOVE;
	constexpr UINT wm_input = WM_INPUT;
	constexpr UINT rid_input = RID_INPUT;
	constexpr DWORD rim_type_mouse = RIM_TYPEMOUSE;
	constexpr USHORT mouse_move_absolute = MOUSE_MOVE_ABSOLUTE;
	constexpr DWORD ridev_remove = RIDEV_REMOVE;
	constexpr USHORT hid_usage_page_generic = 0x01;
	constexpr USHORT hid_usage_generic_mouse = 0x02;
	constexpr UINT raw_input_header_size = sizeof(RAWINPUTHEADER);

	using ::MSG;
	using ::WNDCLASSEXW;
	using ::MONITORINFOEXW;
	using ::DEVMODEW;
	using ::CREATESTRUCTW;
	using ::HDC;
	using ::MONITORENUMPROC;

	using ::GetModuleHandleW;
	using ::RegisterClassExW;
	using ::CreateWindowExW;
	using ::DestroyWindow;
	using ::ShowWindow;
	using ::PeekMessageW;
	using ::TranslateMessage;
	using ::DispatchMessageW;
	using ::MsgWaitForMultipleObjectsEx;
	using ::PostThreadMessageW;
	using ::GetWindowLongPtrW;
	using ::SetCursorPos;
	using ::SetCapture;
	using ::ReleaseCapture;
	using ::SetForegroundWindow;
	using ::MapVirtualKeyW;
	using ::EnumDisplayMonitors;
	using ::EnumDisplaySettingsW;
	using ::ChangeDisplaySettingsExW;
	using ::EmptyClipboard;
	using ::SetClipboardData;
	using ::GlobalAlloc;
	using ::WideCharToMultiByte;

	constexpr UINT cs_hredraw = CS_HREDRAW;
	constexpr UINT cs_vredraw = CS_VREDRAW;
	constexpr DWORD ws_overlappedwindow = WS_OVERLAPPEDWINDOW;
	constexpr DWORD ws_popup = WS_POPUP;
	constexpr DWORD ws_clipsiblings = WS_CLIPSIBLINGS;
	constexpr DWORD ws_clipchildren = WS_CLIPCHILDREN;
	constexpr DWORD ws_visible = WS_VISIBLE;
	constexpr int cw_usedefault = CW_USEDEFAULT;
	constexpr UINT sw_show = SW_SHOW;
	constexpr UINT sw_minimize = SW_MINIMIZE;
	constexpr UINT sw_restore = SW_RESTORE;
	constexpr UINT pm_remove = PM_REMOVE;
	constexpr DWORD qs_allinput = QS_ALLINPUT;
	constexpr DWORD mwmo_inputavailable = MWMO_INPUTAVAILABLE;
	constexpr int gwlp_userdata = GWLP_USERDATA;
	constexpr int gwl_style = GWL_STYLE;
	constexpr int gwl_exstyle = GWL_EXSTYLE;
	constexpr DWORD monitorinfof_primary = MONITORINFOF_PRIMARY;
	constexpr DWORD enum_current_settings = ENUM_CURRENT_SETTINGS;
	constexpr DWORD cds_fullscreen = CDS_FULLSCREEN;
	constexpr LONG disp_change_successful = DISP_CHANGE_SUCCESSFUL;
	constexpr UINT mapvk_vsc_to_vk_ex = MAPVK_VSC_TO_VK_EX;
	constexpr DWORD dm_pelswidth = DM_PELSWIDTH;
	constexpr DWORD dm_pelsheight = DM_PELSHEIGHT;
	constexpr DWORD dm_displayfrequency = DM_DISPLAYFREQUENCY;
	constexpr DWORD dm_bitsperpel = DM_BITSPERPEL;
	constexpr UINT gmem_moveable = GMEM_MOVEABLE;
	constexpr int wheel_delta = WHEEL_DELTA;

	constexpr UINT wm_nccreate = WM_NCCREATE;
	constexpr UINT wm_destroy = WM_DESTROY;
	constexpr UINT wm_close = WM_CLOSE;
	constexpr UINT wm_size = WM_SIZE;
	constexpr UINT wm_setfocus = WM_SETFOCUS;
	constexpr UINT wm_killfocus = WM_KILLFOCUS;
	constexpr UINT wm_keydown = WM_KEYDOWN;
	constexpr UINT wm_keyup = WM_KEYUP;
	constexpr UINT wm_syskeydown = WM_SYSKEYDOWN;
	constexpr UINT wm_syskeyup = WM_SYSKEYUP;
	constexpr UINT wm_char = WM_CHAR;
	constexpr UINT wm_syschar = WM_SYSCHAR;
	constexpr UINT wm_lbuttondown = WM_LBUTTONDOWN;
	constexpr UINT wm_lbuttonup = WM_LBUTTONUP;
	constexpr UINT wm_rbuttondown = WM_RBUTTONDOWN;
	constexpr UINT wm_rbuttonup = WM_RBUTTONUP;
	constexpr UINT wm_mbuttondown = WM_MBUTTONDOWN;
	constexpr UINT wm_mbuttonup = WM_MBUTTONUP;
	constexpr UINT wm_xbuttondown = WM_XBUTTONDOWN;
	constexpr UINT wm_xbuttonup = WM_XBUTTONUP;
	constexpr UINT wm_mousewheel = WM_MOUSEWHEEL;
	constexpr UINT wm_mousehwheel = WM_MOUSEHWHEEL;
	constexpr UINT wm_dpichanged = WM_DPICHANGED;
	constexpr UINT wm_erasebkgnd = WM_ERASEBKGND;
	constexpr UINT wm_null = WM_NULL;
	constexpr UINT wm_syscommand = WM_SYSCOMMAND;
	constexpr UINT wm_displaychange = WM_DISPLAYCHANGE;
	constexpr WPARAM sc_keymenu = SC_KEYMENU;
	constexpr int xbutton1 = XBUTTON1;

	constexpr int vk_space = VK_SPACE;
	constexpr int vk_oem_7 = VK_OEM_7;
	constexpr int vk_oem_comma = VK_OEM_COMMA;
	constexpr int vk_oem_minus = VK_OEM_MINUS;
	constexpr int vk_oem_period = VK_OEM_PERIOD;
	constexpr int vk_oem_2 = VK_OEM_2;
	constexpr int vk_oem_1 = VK_OEM_1;
	constexpr int vk_oem_plus = VK_OEM_PLUS;
	constexpr int vk_oem_4 = VK_OEM_4;
	constexpr int vk_oem_5 = VK_OEM_5;
	constexpr int vk_oem_6 = VK_OEM_6;
	constexpr int vk_oem_3 = VK_OEM_3;
	constexpr int vk_oem_102 = VK_OEM_102;
	constexpr int vk_escape = VK_ESCAPE;
	constexpr int vk_return = VK_RETURN;
	constexpr int vk_tab = VK_TAB;
	constexpr int vk_back = VK_BACK;
	constexpr int vk_insert = VK_INSERT;
	constexpr int vk_delete = VK_DELETE;
	constexpr int vk_right = VK_RIGHT;
	constexpr int vk_left = VK_LEFT;
	constexpr int vk_down = VK_DOWN;
	constexpr int vk_up = VK_UP;
	constexpr int vk_prior = VK_PRIOR;
	constexpr int vk_next = VK_NEXT;
	constexpr int vk_home = VK_HOME;
	constexpr int vk_end = VK_END;
	constexpr int vk_capital = VK_CAPITAL;
	constexpr int vk_scroll = VK_SCROLL;
	constexpr int vk_numlock = VK_NUMLOCK;
	constexpr int vk_snapshot = VK_SNAPSHOT;
	constexpr int vk_pause = VK_PAUSE;
	constexpr int vk_f1 = VK_F1;
	constexpr int vk_f24 = VK_F24;
	constexpr int vk_numpad0 = VK_NUMPAD0;
	constexpr int vk_numpad9 = VK_NUMPAD9;
	constexpr int vk_decimal = VK_DECIMAL;
	constexpr int vk_divide = VK_DIVIDE;
	constexpr int vk_multiply = VK_MULTIPLY;
	constexpr int vk_subtract = VK_SUBTRACT;
	constexpr int vk_add = VK_ADD;
	constexpr int vk_shift = VK_SHIFT;
	constexpr int vk_control = VK_CONTROL;
	constexpr int vk_menu = VK_MENU;
	constexpr int vk_lshift = VK_LSHIFT;
	constexpr int vk_rshift = VK_RSHIFT;
	constexpr int vk_lcontrol = VK_LCONTROL;
	constexpr int vk_rcontrol = VK_RCONTROL;
	constexpr int vk_lmenu = VK_LMENU;
	constexpr int vk_rmenu = VK_RMENU;
	constexpr int vk_lwin = VK_LWIN;
	constexpr int vk_rwin = VK_RWIN;
	constexpr int vk_apps = VK_APPS;

	constexpr int ocr_normal = 32512;
	constexpr int ocr_sizenwse = 32642;
	constexpr int ocr_sizenesw = 32643;
	constexpr int ocr_sizewe = 32644;
	constexpr int ocr_sizens = 32645;
	constexpr int ocr_hand = 32649;

	auto load_standard_cursor(int id) -> HCURSOR {
		return LoadCursorW(nullptr, MAKEINTRESOURCEW(id));
	}

	auto wheel_delta_of(WPARAM wparam) -> int {
		return GET_WHEEL_DELTA_WPARAM(wparam);
	}

	auto xbutton_of(WPARAM wparam) -> int {
		return GET_XBUTTON_WPARAM(wparam);
	}

	auto extended_key(LPARAM lparam) -> bool {
		return (lparam & 0x01000000) != 0;
	}

	auto scancode_of(LPARAM lparam) -> UINT {
		return static_cast<UINT>((lparam >> 16) & 0xFF);
	}

	auto get_x_lparam(LPARAM lparam) -> int {
		return GET_X_LPARAM(lparam);
	}

	auto get_y_lparam(LPARAM lparam) -> int {
		return GET_Y_LPARAM(lparam);
	}

	auto low_word(LPARAM lparam) -> int {
		return LOWORD(lparam);
	}

	auto make_lparam(int low, int high) -> LPARAM {
		return MAKELPARAM(low, high);
	}

	using ::HANDLE;
	using ::HMODULE;
	using ::LONG;
	using ::BOOL;
	using ::SIZE_T;
	using ::DWORD_PTR;
	using ::PVOID;
	using ::DWORD64;
	using ::CONTEXT;
	using ::PRUNTIME_FUNCTION;
	using ::EXCEPTION_POINTERS;
	using ::EXCEPTION_RECORD;
	using ::STARTUPINFOW;
	using ::STARTUPINFOEXW;
	using ::PROCESS_INFORMATION;
	using ::SECURITY_ATTRIBUTES;
	using ::LPPROC_THREAD_ATTRIBUTE_LIST;
	using ::JOBOBJECTINFOCLASS;
	using ::JOBOBJECT_EXTENDED_LIMIT_INFORMATION;

	using ::DuplicateHandle;
	using ::OpenProcess;
	using ::GetCurrentProcess;
	using ::GetCurrentProcessId;
	using ::GetCurrentThread;
	using ::SetThreadPriority;
	using ::SetThreadDescription;
	using ::CloseHandle;
	using ::SuspendThread;
	using ::ResumeThread;
	using ::GetThreadContext;
	using ::RtlCaptureContext;
	using ::RtlLookupFunctionEntry;
	using ::RtlVirtualUnwind;
	using ::GetModuleHandleExW;
	using ::GetModuleFileNameW;
	using ::SHGetKnownFolderPath;
	using ::CoTaskMemFree;
	using ::AddVectoredExceptionHandler;
	using ::IsDebuggerPresent;
	using ::DebugBreak;
	using ::GetLastError;
	using ::CreateProcessW;
	using ::TerminateProcess;
	using ::InitializeProcThreadAttributeList;
	using ::UpdateProcThreadAttribute;
	using ::DeleteProcThreadAttributeList;
	using ::CreateJobObjectW;
	using ::AssignProcessToJobObject;
	using ::TerminateJobObject;
	using ::SetInformationJobObject;
	using ::GetCommandLineW;
	using ::CommandLineToArgvW;
	using ::LocalFree;
	using ::ExitProcess;
	using ::CreatePipe;
	using ::ReadFile;
	using ::SetHandleInformation;
	using ::GetHandleInformation;
	using ::WaitForSingleObject;
	using ::CreateMutexW;
	using ::ReleaseMutex;
	using ::GetExitCodeProcess;
	using ::MoveFileExW;
	using ::GetEnvironmentStringsW;
	using ::FreeEnvironmentStringsW;
	using ::GetEnvironmentVariableW;
	using ::SetEnvironmentVariableW;
	using ::ExpandEnvironmentStringsW;
	using ::HKEY;
	using ::RegQueryInfoKeyW;
	using ::RegEnumValueW;
	using ::RegCloseKey;
	using ::MultiByteToWideChar;
	using ::LARGE_INTEGER;
	using ::LONGLONG;
	using ::CreateWaitableTimerExW;
	using ::SetWaitableTimer;
	using ::CreateNamedPipeW;
	using ::ConnectNamedPipe;
	using ::DisconnectNamedPipe;
	using ::CreateFileW;
	using ::GetFinalPathNameByHandleW;
	using ::WriteFile;
	using ::PeekNamedPipe;
	using ::GetStdHandle;
	using ::GetFileType;
	using ::GetConsoleMode;
	using ::SetConsoleMode;
	using ::ShellExecuteW;
	using ::VirtualQuery;
	using ::MEMORY_BASIC_INFORMATION;
	using ::OVERLAPPED;
	using ::ULONG_PTR;
	using ::FILE_NOTIFY_INFORMATION;
	using ::ReadDirectoryChangesW;
	using ::CreateIoCompletionPort;
	using ::GetQueuedCompletionStatus;
	using ::CancelIoEx;
	using ::GUID;
	using ::FOLDERID_RoamingAppData;
	using ::FOLDERID_LocalAppData;
	using ::FOLDERID_Profile;

	constexpr DWORD mem_commit = MEM_COMMIT;
	constexpr DWORD mem_reserve = MEM_RESERVE;
	constexpr DWORD mem_image = MEM_IMAGE;
	constexpr DWORD mem_mapped = MEM_MAPPED;
	constexpr DWORD mem_private = MEM_PRIVATE;
	constexpr DWORD context_full = CONTEXT_FULL;
	constexpr DWORD unw_flag_nhandler = UNW_FLAG_NHANDLER;
	constexpr DWORD duplicate_same_access = DUPLICATE_SAME_ACCESS;
	constexpr DWORD process_dup_handle = PROCESS_DUP_HANDLE;
	constexpr DWORD process_query_limited_information = PROCESS_QUERY_LIMITED_INFORMATION;
	constexpr DWORD get_module_handle_ex_flag_from_address = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
	constexpr DWORD get_module_handle_ex_flag_unchanged_refcount = GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	constexpr int max_path = MAX_PATH;
	constexpr DWORD kf_flag_default = KF_FLAG_DEFAULT;
	constexpr DWORD startf_use_std_handles = STARTF_USESTDHANDLES;
	constexpr DWORD create_no_window = CREATE_NO_WINDOW;
	constexpr DWORD create_unicode_environment = CREATE_UNICODE_ENVIRONMENT;
	constexpr DWORD create_suspended = CREATE_SUSPENDED;
	constexpr DWORD extended_startupinfo_present = EXTENDED_STARTUPINFO_PRESENT;
	constexpr DWORD_PTR proc_thread_attribute_handle_list = PROC_THREAD_ATTRIBUTE_HANDLE_LIST;
	constexpr JOBOBJECTINFOCLASS job_object_extended_limit_information = JobObjectExtendedLimitInformation;
	constexpr DWORD job_object_limit_kill_on_job_close = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	constexpr DWORD infinite = INFINITE;
	constexpr DWORD wait_timeout = WAIT_TIMEOUT;
	constexpr DWORD wait_object_0 = WAIT_OBJECT_0;
	constexpr DWORD wait_abandoned = WAIT_ABANDONED;
	constexpr DWORD create_waitable_timer_high_resolution = CREATE_WAITABLE_TIMER_HIGH_RESOLUTION;
	constexpr DWORD timer_all_access = TIMER_ALL_ACCESS;
	constexpr DWORD handle_flag_inherit = HANDLE_FLAG_INHERIT;
	constexpr DWORD movefile_replace_existing = MOVEFILE_REPLACE_EXISTING;
	constexpr DWORD movefile_write_through = MOVEFILE_WRITE_THROUGH;
	constexpr UINT cp_utf8 = CP_UTF8;
	constexpr DWORD pipe_access_duplex = PIPE_ACCESS_DUPLEX;
	constexpr DWORD pipe_access_inbound = PIPE_ACCESS_INBOUND;
	constexpr DWORD pipe_type_byte = PIPE_TYPE_BYTE;
	constexpr DWORD pipe_wait = PIPE_WAIT;
	constexpr DWORD pipe_nowait = PIPE_NOWAIT;
	constexpr DWORD pipe_unlimited_instances = PIPE_UNLIMITED_INSTANCES;
	constexpr DWORD generic_read = GENERIC_READ;
	constexpr DWORD generic_write = GENERIC_WRITE;
	constexpr DWORD open_existing = OPEN_EXISTING;
	constexpr DWORD create_always = CREATE_ALWAYS;
	constexpr DWORD file_share_read = FILE_SHARE_READ;
	constexpr DWORD file_share_write = FILE_SHARE_WRITE;
	constexpr DWORD file_share_delete = FILE_SHARE_DELETE;
	constexpr DWORD file_list_directory = FILE_LIST_DIRECTORY;
	constexpr DWORD file_flag_backup_semantics = FILE_FLAG_BACKUP_SEMANTICS;
	constexpr DWORD file_flag_overlapped = FILE_FLAG_OVERLAPPED;
	constexpr DWORD file_notify_change_file_name = FILE_NOTIFY_CHANGE_FILE_NAME;
	constexpr DWORD file_notify_change_dir_name = FILE_NOTIFY_CHANGE_DIR_NAME;
	constexpr DWORD file_notify_change_last_write = FILE_NOTIFY_CHANGE_LAST_WRITE;
	constexpr DWORD file_notify_change_size = FILE_NOTIFY_CHANGE_SIZE;
	constexpr DWORD file_action_removed = FILE_ACTION_REMOVED;
	constexpr DWORD file_action_renamed_old_name = FILE_ACTION_RENAMED_OLD_NAME;
	constexpr DWORD file_attribute_normal = FILE_ATTRIBUTE_NORMAL;
	constexpr DWORD std_output_handle = STD_OUTPUT_HANDLE;
	constexpr DWORD file_type_char = FILE_TYPE_CHAR;
	constexpr DWORD enable_virtual_terminal_processing = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	constexpr DWORD std_input_handle = STD_INPUT_HANDLE;
	constexpr DWORD enable_quick_edit_mode = ENABLE_QUICK_EDIT_MODE;
	constexpr DWORD enable_extended_flags = ENABLE_EXTENDED_FLAGS;
	constexpr DWORD error_pipe_connected = ERROR_PIPE_CONNECTED;
	constexpr DWORD error_pipe_listening = ERROR_PIPE_LISTENING;
	constexpr DWORD error_no_data = ERROR_NO_DATA;
	constexpr DWORD error_broken_pipe = ERROR_BROKEN_PIPE;
	constexpr LONG error_success = ERROR_SUCCESS;
	constexpr int sw_show_normal = SW_SHOWNORMAL;

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
	constexpr int thread_priority_normal = THREAD_PRIORITY_NORMAL;
	constexpr int thread_priority_below_normal = THREAD_PRIORITY_BELOW_NORMAL;

	using ::HGLOBAL;
	using ::HDROP;
	using ::BITMAPINFOHEADER;

	using ::IsClipboardFormatAvailable;
	using ::GetClipboardSequenceNumber;
	using ::OpenClipboard;
	using ::CloseClipboard;
	using ::GetClipboardData;
	using ::GlobalLock;
	using ::GlobalUnlock;
	using ::GlobalSize;
	using ::GlobalFree;
	using ::DragQueryFileW;

	constexpr UINT cf_dib = CF_DIB;
	constexpr UINT cf_dibv5 = CF_DIBV5;
	constexpr UINT cf_hdrop = CF_HDROP;
	constexpr UINT cf_unicodetext = CF_UNICODETEXT;
	constexpr DWORD bi_rgb = BI_RGB;
	constexpr DWORD bi_bitfields = BI_BITFIELDS;
	constexpr DWORD bitmap_info_header_size = sizeof(BITMAPINFOHEADER);
	constexpr UINT drag_query_count = 0xFFFFFFFFu;

	auto valid_handle(HANDLE handle) -> bool {
		return handle != nullptr && handle != INVALID_HANDLE_VALUE;
	}

	auto read_user_environment(const wchar_t* name, wchar_t* out_value, const DWORD out_capacity) -> bool {
		if (name == nullptr || out_value == nullptr || out_capacity == 0) {
			return false;
		}
		out_value[0] = L'\0';

		HKEY key = nullptr;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &key) != ERROR_SUCCESS) {
			return false;
		}

		DWORD type = 0;
		DWORD size = out_capacity * static_cast<DWORD>(sizeof(wchar_t));
		const LSTATUS status = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(out_value), &size);
		RegCloseKey(key);

		if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
			out_value[0] = L'\0';
			return false;
		}

		const DWORD count = size / static_cast<DWORD>(sizeof(wchar_t));
		out_value[count < out_capacity ? count : out_capacity - 1] = L'\0';
		return out_value[0] != L'\0';
	}

	auto open_registry_key(const bool local_machine, const wchar_t* subkey) -> HKEY {
		HKEY key = nullptr;
		if (RegOpenKeyExW(local_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
			return nullptr;
		}
		return key;
	}

	auto open_file_dialog(
		HWND owner,
		const wchar_t* title,
		const wchar_t* filter,
		wchar_t* out_path,
		DWORD out_capacity
	) -> bool {
		if (out_path == nullptr || out_capacity == 0) {
			return false;
		}
		out_path[0] = L'\0';

		OPENFILENAMEW ofn{
			.lStructSize = sizeof(OPENFILENAMEW),
			.hwndOwner = owner,
			.lpstrFilter = filter,
			.lpstrFile = out_path,
			.nMaxFile = out_capacity,
			.lpstrTitle = title,
			.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER,
		};

		return GetOpenFileNameW(&ofn) != 0;
	}
}

auto gse::win32::performance_counter_frequency() -> unsigned long long {
	LARGE_INTEGER frequency{};
	return QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 ? static_cast<unsigned long long>(frequency.QuadPart) : 0;
}

auto gse::win32::performance_counter() -> unsigned long long {
	LARGE_INTEGER counter{};
	return QueryPerformanceCounter(&counter) && counter.QuadPart > 0 ? static_cast<unsigned long long>(counter.QuadPart) : 0;
}

auto gse::win32::compressed_size_bound(const void* input, const std::size_t input_size) -> std::size_t {
	COMPRESSOR_HANDLE compressor = nullptr;
	if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &compressor)) {
		return 0;
	}
	SIZE_T needed = 0;
	Compress(compressor, const_cast<void*>(input), input_size, nullptr, 0, &needed);
	CloseCompressor(compressor);
	return needed;
}

auto gse::win32::compress_lzms(const void* input, const std::size_t input_size, void* output, const std::size_t output_capacity, std::size_t* written) -> bool {
	COMPRESSOR_HANDLE compressor = nullptr;
	if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &compressor)) {
		return false;
	}
	SIZE_T produced = 0;
	const BOOL ok = Compress(compressor, const_cast<void*>(input), input_size, output, output_capacity, &produced);
	CloseCompressor(compressor);
	*written = produced;
	return ok != 0;
}

auto gse::win32::decompress_lzms(const void* input, const std::size_t input_size, void* output, const std::size_t output_size) -> bool {
	DECOMPRESSOR_HANDLE decompressor = nullptr;
	if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &decompressor)) {
		return false;
	}
	SIZE_T produced = 0;
	const BOOL ok = Decompress(decompressor, const_cast<void*>(input), input_size, output, output_size, &produced);
	CloseDecompressor(decompressor);
	return ok != 0 && produced == output_size;
}

auto gse::win32::write_user_registry_string(const wchar_t* subkey, const wchar_t* name, const wchar_t* value) -> bool {
	HKEY key = nullptr;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
		return false;
	}
	std::size_t length = 0;
	while (value[length] != L'\0') {
		++length;
	}
	const DWORD bytes = static_cast<DWORD>((length + 1) * sizeof(wchar_t));
	const LSTATUS status = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), bytes);
	RegCloseKey(key);
	return status == ERROR_SUCCESS;
}

auto gse::win32::delete_user_registry_key(const wchar_t* subkey) -> bool {
	const LSTATUS status = RegDeleteTreeW(HKEY_CURRENT_USER, subkey);
	return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

auto gse::win32::show_error_box(const wchar_t* title, const wchar_t* text) -> void {
	MessageBoxW(nullptr, text, title, MB_OK | MB_ICONERROR);
}

auto gse::win32::enable_per_monitor_dpi_awareness() -> bool {
	return SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != 0;
}

auto gse::win32::dpi_for_window(const HWND window) -> unsigned int {
	constexpr unsigned int fallback_dpi = USER_DEFAULT_SCREEN_DPI;
	const UINT dpi = GetDpiForWindow(window);
	return dpi != 0 ? dpi : fallback_dpi;
}

auto gse::win32::adjust_window_rect_for_dpi(RECT* rect, const DWORD style, const DWORD ex_style, const unsigned int dpi) -> void {
	AdjustWindowRectExForDpi(rect, style, FALSE, ex_style, dpi);
}
#endif

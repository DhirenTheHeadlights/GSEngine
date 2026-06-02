module;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
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
}
#endif

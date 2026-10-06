module gse.os:window_impl;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.log;
import gse.math;
import gse.meta;
import gse.time;
import gse.win32;
import gse.win32.environment;
import std;

import :clipboard;
import :input_events;
import :keys;
import :window;

namespace gse {
	using namespace gse::win32;

	constexpr const wchar_t* window_class_name = L"gse_window";
	constexpr std::string_view native_resolution_option = "Native";
	constexpr vec2i default_window_size{ 1920, 1080 };
	constexpr vec2i minimum_restore_size{ 320, 240 };
	constexpr std::uint32_t housekeeping_interval_frames = 8;
	constexpr long minimized_rect_coordinate = -30000;
	constexpr int frame_resize_border = 8;
	constexpr unsigned int reference_dpi = 96;
	constexpr DWORD windowed_style = ws_overlappedwindow | ws_clipsiblings | ws_clipchildren;
	constexpr DWORD fullscreen_style = ws_popup | ws_clipsiblings | ws_clipchildren;

	struct monitor_record {
		std::wstring device;
		std::string name;
		rect_t<vec2i> bounds;
		rect_t<vec2i> work_area;
		int width = 0;
		int height = 0;
		int refresh_rate = 0;
		bool primary = false;
	};

	std::optional<std::vector<monitor_record>> monitor_cache;
	std::wstring exclusive_device;
	std::atomic<DWORD> main_thread_id = 0;
	cursor_shape current_cursor_shape = cursor_shape::arrow;
	std::array<HCURSOR, enum_values<cursor_shape>().size()> cursor_cache{};

	auto to_hwnd(
		native_window_handle handle
	) -> HWND;

	auto to_native_handle(
		HWND handle
	) -> native_window_handle;

	auto surface_of(
		HWND handle
	) -> window::window_surface*;

	auto window_proc(
		HWND handle,
		UINT msg,
		WPARAM wparam,
		LPARAM lparam
	) -> LRESULT;

	auto monitor_enum_proc(
		HMONITOR monitor,
		HDC context,
		RECT* bounds,
		LPARAM lparam
	) -> BOOL;

	auto invalidate_monitors() -> void;

	[[nodiscard]] auto monitors() -> std::span<const monitor_record>;

	auto display_label(
		std::wstring_view device
	) -> std::string;

	auto enter_exclusive_mode(
		std::wstring device,
		std::string name,
		const resolution_info& resolution
	) -> bool;

	auto leave_exclusive_mode() -> void;

	auto window_visible(
		native_window_handle handle
	) -> bool;

	auto content_scale_of(
		native_window_handle handle
	) -> float;

	auto client_rect_in_screen(
		HWND handle
	) -> rect_t<vec2i>;

	auto monitor_key_for_index(
		int index
	) -> std::string;

	auto refresh_monitor_settings(
		window::data& d
	) -> void;

	auto refresh_resolution_settings(
		window::data& d
	) -> void;

	auto selected_monitor_index(
		const window::data& d
	) -> int;

	auto selected_resolution(
		const window::data& d,
		int monitor_index
	) -> std::optional<resolution_info>;

	auto desired_present_mode(
		const window::data& d
	) -> gpu::present_mode;

	auto apply_cursor_mode(
		window::data& d
	) -> void;

	auto set_cursor_capture(
		window::window_surface& s,
		bool capture
	) -> void;

	auto cursor_for_shape(
		cursor_shape shape
	) -> HCURSOR;

	auto apply_display_mode(
		window::data& d,
		display_mode mode
	) -> void;

	auto apply_fullscreen_placement(
		const window::data& d,
		int monitor_index
	) -> void;

	auto center_window_in_work_area(
		const window::data& d,
		const rect_t<vec2i>& work_area
	) -> void;

	[[nodiscard]] auto place_window_on_monitor(
		const window::data& d,
		int monitor_index
	) -> bool;

	auto monitor_index_for_window(
		vec2i position,
		vec2i size
	) -> int;

	[[nodiscard]] auto refresh_interval_for_window(
		HWND handle
	) -> time_t<double>;

	auto monitor_work_area(
		int monitor_index
	) -> std::optional<rect_t<vec2i>>;

	auto set_window_style(
		HWND handle,
		DWORD style
	) -> void;

	auto apply_frame_appearance(
		HWND handle
	) -> void;

	auto reanchor_saved_geometry(
		window::data& d,
		int monitor_index
	) -> void;

	auto set_client_rect(
		const window::window_surface& s,
		vec2i position,
		vec2i size
	) -> void;

	[[nodiscard]] auto surface_at(
		window::data& d,
		vec2i screen_point
	) -> window::window_surface*;

	[[nodiscard]] auto pointer_event_rank(
		const input::event& e
	) -> int;

	auto os_restore_geometry(
		const window::data& d
	) -> std::optional<window::geometry>;

	auto probe_composition(
		const window::data& d
	) -> window::composition_probe;

	auto cloak_window(
		native_window_handle handle
	) -> void;

	auto plausible_restore_geometry(
		const window::geometry& g
	) -> bool;

	auto effective_restore_geometry(
		const window::data& d
	) -> window::geometry;

	auto restore_window_geometry(
		window::data& d
	) -> void;

	auto record_window_geometry(
		window::data& d
	) -> void;

	auto to_input_key(
		WPARAM wparam,
		LPARAM lparam
	) -> std::optional<key>;

	auto handle_mouse_button(
		window::window_surface& surface,
		mouse_button button,
		LPARAM lparam,
		bool pressed
	) -> void;

	auto handle_mouse_move(
		window::window_surface& surface,
		int x,
		int y
	) -> void;

	auto handle_text(
		window::window_surface& surface,
		WPARAM wparam
	) -> void;

	auto handle_raw_input(
		window::window_surface& surface,
		LPARAM lparam
	) -> void;

	auto handle_hit_test(
		const window::window_surface& surface,
		HWND handle,
		LPARAM lparam
	) -> LRESULT;

	auto handle_calc_size(
		HWND handle,
		LPARAM lparam
	) -> bool;

	auto create_surface_window(
		window::window_surface& surface,
		const std::string& title,
		vec2i size,
		bool custom_frame
	) -> bool;

	auto create_window(
		window::data& d
	) -> void;

	auto apply_launcher_mode(
		window::data& d,
		vec2i size
	) -> void;
}

namespace gse::window {
	std::function<void()> resize_pump;
	bool modal_resize_active = false;
	bool resize_pump_running = false;
	time_t<double> modal_resize_interval{};
	time_t<double> modal_resize_last_pump{};

	struct secondary_window_desc {
		std::string title;
		vec2i size{ 800, 600 };
		vec2i position{ 0, 0 };
		bool use_position = false;
	};

	auto run_resize_pump(
		bool force
	) -> void;

	auto poll_events() -> void;

	[[nodiscard]] auto apply_requests(
		scheduler& sched,
		data& d
	) -> std::optional<window_open_file_request>;

	auto sync_primary_geometry(
		data& d
	) -> void;

	auto route_input(
		data& d
	) -> void;

	auto sync_secondaries(
		scheduler& sched,
		data& d
	) -> void;

	[[nodiscard]] auto create_secondary(
		data& d,
		const secondary_window_desc& desc
	) -> window_surface*;

	auto destroy_secondary(
		data& d,
		window_surface* surface
	) -> void;

	auto set_ui_focus(
		data& d,
		bool focus
	) -> void;

	[[nodiscard]] auto prompt_for_file(
		const window_surface& s,
		const window_open_file_request& request
	) -> std::filesystem::path;

	[[nodiscard]] auto enumerate_resolutions(
		int monitor_index
	) -> std::vector<resolution_info>;
}

auto gse::to_hwnd(const native_window_handle handle) -> HWND {
	return static_cast<HWND>(handle.value);
}

auto gse::to_native_handle(const HWND handle) -> native_window_handle {
	return {
		.value = handle
	};
}

auto gse::surface_of(const HWND handle) -> window::window_surface* {
	return reinterpret_cast<window::window_surface*>(GetWindowLongPtrW(handle, gwlp_userdata));
}

auto gse::invalidate_monitors() -> void {
	monitor_cache.reset();
}

auto gse::display_label(const std::wstring_view device) -> std::string {
	const std::size_t separator = device.find_last_of(L'\\');
	return narrow(separator == std::wstring_view::npos ? device : device.substr(separator + 1));
}

auto gse::monitor_enum_proc(const HMONITOR monitor, HDC, RECT*, const LPARAM lparam) -> BOOL {
	auto* records = reinterpret_cast<std::vector<monitor_record>*>(lparam);

	MONITORINFOEXW info{};
	info.cbSize = sizeof(info);
	if (GetMonitorInfoW(monitor, &info) == 0) {
		return 1;
	}

	DEVMODEW mode{};
	mode.dmSize = sizeof(mode);
	const bool have_mode = EnumDisplaySettingsW(info.szDevice, enum_current_settings, &mode) != 0;

	records->push_back({
		.device = info.szDevice,
		.name = display_label(info.szDevice),
		.bounds = rect_t<vec2i>::from_position_size(
			{ static_cast<int>(info.rcMonitor.left), static_cast<int>(info.rcMonitor.top) },
			{ static_cast<int>(info.rcMonitor.right - info.rcMonitor.left), static_cast<int>(info.rcMonitor.bottom - info.rcMonitor.top) }
		),
		.work_area = rect_t<vec2i>::from_position_size(
			{ static_cast<int>(info.rcWork.left), static_cast<int>(info.rcWork.top) },
			{ static_cast<int>(info.rcWork.right - info.rcWork.left), static_cast<int>(info.rcWork.bottom - info.rcWork.top) }
		),
		.width = have_mode ? static_cast<int>(mode.dmPelsWidth) : static_cast<int>(info.rcMonitor.right - info.rcMonitor.left),
		.height = have_mode ? static_cast<int>(mode.dmPelsHeight) : static_cast<int>(info.rcMonitor.bottom - info.rcMonitor.top),
		.refresh_rate = have_mode ? static_cast<int>(mode.dmDisplayFrequency) : 0,
		.primary = (info.dwFlags & monitorinfof_primary) != 0,
	});
	return 1;
}

auto gse::monitors() -> std::span<const monitor_record> {
	if (monitor_cache) {
		return *monitor_cache;
	}

	std::vector<monitor_record> records;
	EnumDisplayMonitors(nullptr, nullptr, reinterpret_cast<MONITORENUMPROC>(&monitor_enum_proc), reinterpret_cast<LPARAM>(&records));
	std::ranges::stable_partition(records, [](const monitor_record& record) {
		return record.primary;
	});
	return *(monitor_cache = std::move(records));
}

auto gse::refresh_interval_for_window(const HWND handle) -> time_t<double> {
	constexpr auto fallback = seconds(1.0 / 60.0);

	RECT bounds{};
	if (GetWindowRect(handle, &bounds) == 0) {
		return fallback;
	}

	const auto records = monitors();
	const int index = monitor_index_for_window(
		{ static_cast<int>(bounds.left), static_cast<int>(bounds.top) },
		{ static_cast<int>(bounds.right - bounds.left), static_cast<int>(bounds.bottom - bounds.top) }
	);
	if (index < 0 || index >= static_cast<int>(records.size()) || records[index].refresh_rate <= 0) {
		return fallback;
	}
	return seconds(1.0 / static_cast<double>(records[index].refresh_rate));
}

auto gse::window::run_resize_pump(const bool force) -> void {
	if (!modal_resize_active || !resize_pump || resize_pump_running) {
		return;
	}
	if (!force && system_clock::now<time_t<double>>() - modal_resize_last_pump < modal_resize_interval) {
		return;
	}
	resize_pump_running = true;
	resize_pump();
	resize_pump_running = false;
	modal_resize_last_pump = system_clock::now<time_t<double>>();
}

auto gse::window::install_resize_pump(std::function<void()> pump) -> void {
	resize_pump = std::move(pump);
}

auto gse::to_input_key(const WPARAM wparam, const LPARAM lparam) -> std::optional<key> {
	auto virtual_key = static_cast<int>(wparam);

	if (virtual_key == vk_shift) {
		virtual_key = static_cast<int>(MapVirtualKeyW(scancode_of(lparam), mapvk_vsc_to_vk_ex));
	}
	else if (virtual_key == vk_control) {
		virtual_key = extended_key(lparam) ? vk_rcontrol : vk_lcontrol;
	}
	else if (virtual_key == vk_menu) {
		virtual_key = extended_key(lparam) ? vk_rmenu : vk_lmenu;
	}

	if (virtual_key == vk_space || (virtual_key >= '0' && virtual_key <= '9') || (virtual_key >= 'A' && virtual_key <= 'Z')) {
		return static_cast<key>(virtual_key);
	}
	if (virtual_key >= vk_f1 && virtual_key <= vk_f24) {
		return static_cast<key>(static_cast<int>(key::f1) + (virtual_key - vk_f1));
	}
	if (virtual_key >= vk_numpad0 && virtual_key <= vk_numpad9) {
		return static_cast<key>(static_cast<int>(key::kp_0) + (virtual_key - vk_numpad0));
	}

	switch (virtual_key) {
		case vk_oem_7:
			return key::apostrophe;
		case vk_oem_comma:
			return key::comma;
		case vk_oem_minus:
			return key::minus;
		case vk_oem_period:
			return key::period;
		case vk_oem_2:
			return key::slash;
		case vk_oem_1:
			return key::semicolon;
		case vk_oem_plus:
			return key::equal;
		case vk_oem_4:
			return key::left_bracket;
		case vk_oem_5:
			return key::backslash;
		case vk_oem_6:
			return key::right_bracket;
		case vk_oem_3:
			return key::grave_accent;
		case vk_oem_102:
			return key::world_2;
		case vk_escape:
			return key::escape;
		case vk_return:
			return extended_key(lparam) ? key::kp_enter : key::enter;
		case vk_tab:
			return key::tab;
		case vk_back:
			return key::backspace;
		case vk_insert:
			return key::insert;
		case vk_delete:
			return key::del;
		case vk_right:
			return key::right;
		case vk_left:
			return key::left;
		case vk_down:
			return key::down;
		case vk_up:
			return key::up;
		case vk_prior:
			return key::page_up;
		case vk_next:
			return key::page_down;
		case vk_home:
			return key::home;
		case vk_end:
			return key::end;
		case vk_capital:
			return key::caps_lock;
		case vk_scroll:
			return key::scroll_lock;
		case vk_numlock:
			return key::num_lock;
		case vk_snapshot:
			return key::print_screen;
		case vk_pause:
			return key::pause;
		case vk_decimal:
			return key::kp_decimal;
		case vk_divide:
			return key::kp_divide;
		case vk_multiply:
			return key::kp_multiply;
		case vk_subtract:
			return key::kp_subtract;
		case vk_add:
			return key::kp_add;
		case vk_lshift:
			return key::left_shift;
		case vk_rshift:
			return key::right_shift;
		case vk_lcontrol:
			return key::left_control;
		case vk_rcontrol:
			return key::right_control;
		case vk_lmenu:
			return key::left_alt;
		case vk_rmenu:
			return key::right_alt;
		case vk_lwin:
			return key::left_super;
		case vk_rwin:
			return key::right_super;
		case vk_apps:
			return key::menu;
		default:
			return std::nullopt;
	}
}

auto gse::handle_mouse_button(window::window_surface& surface, const mouse_button button, const LPARAM lparam, const bool pressed) -> void {
	frame_demand::request_redraw();

	const auto bit = static_cast<std::uint8_t>(1u << static_cast<int>(button));
	if (pressed) {
		if (surface.mouse_button_mask == 0) {
			SetCapture(to_hwnd(surface.handle));
		}
		surface.mouse_button_mask |= bit;
	}
	else {
		surface.mouse_button_mask &= static_cast<std::uint8_t>(~bit);
		if (surface.mouse_button_mask == 0) {
			ReleaseCapture();
		}
	}

	const auto x = static_cast<double>(get_x_lparam(lparam));
	auto y = static_cast<double>(get_y_lparam(lparam));
	if (surface.ui_focus) {
		y = static_cast<double>(window::viewport(surface.handle).y()) - y;
	}

	if (pressed) {
		surface.input_events.push(input::mouse_button_pressed{ button, x, y });
	}
	else {
		surface.input_events.push(input::mouse_button_released{ button, x, y });
	}
}

auto gse::handle_mouse_move(window::window_surface& surface, const int x, const int y) -> void {
	frame_demand::request_redraw();

	if (!surface.ui_focus) {
		surface.input_events.push(input::mouse_moved{ static_cast<double>(x), static_cast<double>(y) });
		return;
	}

	const auto dims = window::viewport(surface.handle);
	const auto height = static_cast<double>(dims.y());

	if (surface.cursor_captured || (surface.mouse_button_mask & 1u) != 0) {
		surface.input_events.push(input::mouse_moved{ static_cast<double>(x), height - static_cast<double>(y) });
		return;
	}

	const double clamped_x = std::clamp(static_cast<double>(x), 0.0, static_cast<double>(dims.x()));
	const double clamped_y = std::clamp(static_cast<double>(y), 0.0, height);

	if (clamped_x != static_cast<double>(x) || clamped_y != static_cast<double>(y)) {
		POINT target{
			.x = static_cast<LONG>(clamped_x),
			.y = static_cast<LONG>(clamped_y),
		};
		ClientToScreen(to_hwnd(surface.handle), &target);
		SetCursorPos(target.x, target.y);
	}

	surface.input_events.push(input::mouse_moved{ clamped_x, height - clamped_y });
}

auto gse::handle_text(window::window_surface& surface, const WPARAM wparam) -> void {
	const auto unit = static_cast<std::uint32_t>(wparam);

	if (unit >= 0xD800 && unit <= 0xDBFF) {
		surface.pending_high_surrogate = unit;
		return;
	}

	std::uint32_t codepoint = unit;
	if (unit >= 0xDC00 && unit <= 0xDFFF) {
		if (surface.pending_high_surrogate == 0) {
			return;
		}
		codepoint = 0x10000 + ((surface.pending_high_surrogate - 0xD800) << 10) + (unit - 0xDC00);
	}
	surface.pending_high_surrogate = 0;

	if (codepoint < 32 || (codepoint > 126 && codepoint < 160)) {
		return;
	}

	frame_demand::request_redraw();
	surface.input_events.push(input::text_entered{ codepoint });
}

auto gse::handle_raw_input(window::window_surface& surface, const LPARAM lparam) -> void {
	RAWINPUT raw{};
	UINT size = sizeof(raw);
	const UINT written = GetRawInputData(
		reinterpret_cast<HRAWINPUT>(lparam),
		rid_input,
		&raw,
		&size,
		raw_input_header_size
	);

	if (written == static_cast<UINT>(-1) || raw.header.dwType != rim_type_mouse || (raw.data.mouse.usFlags & mouse_move_absolute) != 0) {
		return;
	}

	surface.input_events.push(input::mouse_raw_moved{
		.x_delta = static_cast<double>(raw.data.mouse.lLastX),
		.y_delta = static_cast<double>(raw.data.mouse.lLastY),
	});
}

auto gse::handle_calc_size(const HWND handle, const LPARAM lparam) -> bool {
	auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
	const bool offscreen_rect = params->rgrc[0].left <= minimized_rect_coordinate
		|| params->rgrc[0].top <= minimized_rect_coordinate;

	if (IsIconic(handle) != 0 || offscreen_rect) {
		return false;
	}

	if (IsZoomed(handle) != 0) {
		if (const HMONITOR monitor = MonitorFromRect(&params->rgrc[0], monitor_default_to_nearest)) {
			MONITORINFO info{};
			info.cbSize = sizeof(info);
			if (GetMonitorInfoW(monitor, &info)) {
				params->rgrc[0].left = std::max(params->rgrc[0].left, info.rcWork.left);
				params->rgrc[0].top = std::max(params->rgrc[0].top, info.rcWork.top);
				params->rgrc[0].right = std::min(params->rgrc[0].right, info.rcWork.right);
				params->rgrc[0].bottom = std::min(params->rgrc[0].bottom, info.rcWork.bottom);
			}
		}
	}
	return true;
}

auto gse::handle_hit_test(const window::window_surface& surface, const HWND handle, const LPARAM lparam) -> LRESULT {
	POINT cursor{ get_x_lparam(lparam), get_y_lparam(lparam) };
	ScreenToClient(handle, &cursor);
	RECT client{};
	GetClientRect(handle, &client);

	const bool left = cursor.x < frame_resize_border;
	const bool right = cursor.x >= client.right - frame_resize_border;
	const bool top = cursor.y < frame_resize_border;
	const bool bottom = cursor.y >= client.bottom - frame_resize_border;

	if (IsZoomed(handle) == 0) {
		if (top && left) {
			return ht_top_left;
		}
		if (top && right) {
			return ht_top_right;
		}
		if (bottom && left) {
			return ht_bottom_left;
		}
		if (bottom && right) {
			return ht_bottom_right;
		}
		if (left) {
			return ht_left;
		}
		if (right) {
			const int exclude_y0 = surface.chrome_resize_exclude_y0;
			const int exclude_y1 = surface.chrome_resize_exclude_y1;
			if (exclude_y1 > exclude_y0 && cursor.y >= exclude_y0 && cursor.y < exclude_y1) {
				return ht_client;
			}
			return ht_right;
		}
		if (top) {
			return ht_top;
		}
		if (bottom) {
			return ht_bottom;
		}
	}

	if (cursor.y < surface.chrome_caption_height) {
		if (cursor.x >= client.right - surface.chrome_controls_width) {
			return ht_client;
		}
		return ht_caption;
	}
	return ht_client;
}

auto gse::window_proc(const HWND handle, const UINT msg, const WPARAM wparam, const LPARAM lparam) -> LRESULT {
	if (msg == wm_nccreate) {
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
		SetWindowLongPtrW(handle, gwlp_userdata, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
		return DefWindowProcW(handle, msg, wparam, lparam);
	}

	auto* surface = surface_of(handle);
	if (surface == nullptr) {
		return DefWindowProcW(handle, msg, wparam, lparam);
	}

	switch (msg) {
		case wm_close:
			surface->should_close = true;
			return 0;
		case wm_erasebkgnd:
			return 1;
		case wm_syscommand:
			if ((wparam & 0xFFF0) == sc_keymenu) {
				return 0;
			}
			break;
		case wm_size:
			surface->framebuffer_resized = true;
			frame_demand::request_redraw();
			window::run_resize_pump(false);
			break;
		case wm_setfocus:
			surface->focused = true;
			frame_demand::request_redraw();
			break;
		case wm_killfocus:
			surface->focused = false;
			surface->mouse_button_mask = 0;
			if (surface->cursor_capture_active) {
				ClipCursor(nullptr);
			}
			frame_demand::request_redraw();
			break;
		case wm_displaychange:
			invalidate_monitors();
			frame_demand::request_redraw();
			break;
		case wm_dpichanged: {
			surface->content_scale = static_cast<float>(low_word(static_cast<LPARAM>(wparam))) / static_cast<float>(reference_dpi);
			const auto* suggested = reinterpret_cast<const RECT*>(lparam);
			SetWindowPos(
				handle,
				nullptr,
				suggested->left,
				suggested->top,
				suggested->right - suggested->left,
				suggested->bottom - suggested->top,
				swp_no_zorder | swp_no_activate
			);
			invalidate_monitors();
			return 0;
		}
		case wm_entersizemove:
			window::modal_resize_active = true;
			window::modal_resize_interval = refresh_interval_for_window(handle);
			window::modal_resize_last_pump = {};
			break;
		case wm_exitsizemove:
			window::run_resize_pump(true);
			window::modal_resize_active = false;
			break;
		case wm_setcursor:
			if (low_word(lparam) == ht_client) {
				SetCursor(surface->cursor_capture_active ? nullptr : cursor_for_shape(current_cursor_shape));
				return 1;
			}
			break;
		case wm_input:
			if (surface->cursor_capture_active) {
				handle_raw_input(*surface, lparam);
			}
			break;
		case wm_keydown:
		case wm_syskeydown:
			if (const auto mapped = to_input_key(wparam, lparam)) {
				frame_demand::request_redraw();
				surface->input_events.push(input::key_pressed{
					.key_code = *mapped
				});
			}
			break;
		case wm_keyup:
		case wm_syskeyup:
			if (const auto mapped = to_input_key(wparam, lparam)) {
				frame_demand::request_redraw();
				surface->input_events.push(input::key_released{
					.key_code = *mapped
				});
			}
			break;
		case wm_char:
			handle_text(*surface, wparam);
			return 0;
		case wm_syschar:
			return 0;
		case wm_mousemove:
			handle_mouse_move(*surface, get_x_lparam(lparam), get_y_lparam(lparam));
			break;
		case wm_lbuttondown:
			handle_mouse_button(*surface, mouse_button::button_1, lparam, true);
			return 0;
		case wm_lbuttonup:
			handle_mouse_button(*surface, mouse_button::button_1, lparam, false);
			return 0;
		case wm_rbuttondown:
			handle_mouse_button(*surface, mouse_button::button_2, lparam, true);
			return 0;
		case wm_rbuttonup:
			handle_mouse_button(*surface, mouse_button::button_2, lparam, false);
			return 0;
		case wm_mbuttondown:
			handle_mouse_button(*surface, mouse_button::button_3, lparam, true);
			return 0;
		case wm_mbuttonup:
			handle_mouse_button(*surface, mouse_button::button_3, lparam, false);
			return 0;
		case wm_xbuttondown:
			handle_mouse_button(*surface, xbutton_of(wparam) == xbutton1 ? mouse_button::button_4 : mouse_button::button_5, lparam, true);
			return 1;
		case wm_xbuttonup:
			handle_mouse_button(*surface, xbutton_of(wparam) == xbutton1 ? mouse_button::button_4 : mouse_button::button_5, lparam, false);
			return 1;
		case wm_mousewheel:
			frame_demand::request_redraw();
			surface->input_events.push(input::mouse_scrolled{ 0.0, static_cast<double>(wheel_delta_of(wparam)) / static_cast<double>(wheel_delta) });
			return 0;
		case wm_mousehwheel:
			frame_demand::request_redraw();
			surface->input_events.push(input::mouse_scrolled{ -static_cast<double>(wheel_delta_of(wparam)) / static_cast<double>(wheel_delta), 0.0 });
			return 0;
		case wm_nccalcsize:
			if (surface->custom_frame && wparam != 0 && handle_calc_size(handle, lparam)) {
				return 0;
			}
			break;
		case wm_nchittest:
			if (surface->custom_frame) {
				return handle_hit_test(*surface, handle, lparam);
			}
			break;
		case wm_ncmousemove:
			if (surface->custom_frame && surface->ui_focus) {
				POINT cursor{ get_x_lparam(lparam), get_y_lparam(lparam) };
				ScreenToClient(handle, &cursor);
				const auto dims = window::viewport(surface->handle);
				surface->input_events.push(input::mouse_moved{
					.x_pos = static_cast<double>(cursor.x),
					.y_pos = static_cast<double>(dims.y() - cursor.y),
				});
			}
			break;
		default:
			break;
	}

	return DefWindowProcW(handle, msg, wparam, lparam);
}

auto gse::cursor_for_shape(const cursor_shape shape) -> HCURSOR {
	const auto slot = static_cast<std::size_t>(shape);
	if (cursor_cache[slot] != nullptr) {
		return cursor_cache[slot];
	}

	const int id = [shape] {
		switch (shape) {
			case cursor_shape::hand:
				return ocr_hand;
			case cursor_shape::resize_ew:
				return ocr_sizewe;
			case cursor_shape::resize_ns:
				return ocr_sizens;
			case cursor_shape::resize_nwse:
				return ocr_sizenwse;
			case cursor_shape::resize_nesw:
				return ocr_sizenesw;
			case cursor_shape::arrow:
			default:
				return ocr_normal;
		}
	}();

	cursor_cache[slot] = load_standard_cursor(id);
	return cursor_cache[slot];
}

auto gse::refresh_monitor_settings(window::data& d) -> void {
	d.monitor.options.clear();
	for (const auto& [index, record] : std::views::enumerate(monitors())) {
		std::string label = std::format("{}: {}x{}", record.name, record.width, record.height);
		if (std::ranges::contains(d.monitor.options, label)) {
			label = std::format("{} #{}", label, index + 1);
		}
		d.monitor.options.push_back(std::move(label));
	}

	if (!std::ranges::contains(d.monitor.options, d.monitor.value)) {
		if (!d.monitor.value.empty()) {
			log::println(
				log::level::warning,
				log::category::general,
				"monitor '{}' is not in the current monitor list; falling back to '{}'",
				d.monitor.value,
				d.monitor.options.empty() ? std::string("<none>") : d.monitor.options.front()
			);
		}
		d.monitor.value = d.monitor.options.empty() ? std::string{} : d.monitor.options.front();
	}
}

auto gse::refresh_resolution_settings(window::data& d) -> void {
	const auto resolutions = window::enumerate_resolutions(selected_monitor_index(d));

	d.resolution.options.clear();
	d.resolution.options.emplace_back(native_resolution_option);
	for (const auto& resolution : resolutions) {
		d.resolution.options.push_back(std::format("{}", resolution));
	}

	if (!std::ranges::contains(d.resolution.options, d.resolution.value)) {
		d.resolution.value = native_resolution_option;
	}
}

auto gse::selected_monitor_index(const window::data& d) -> int {
	const auto it = std::ranges::find(d.monitor.options, d.monitor.value);
	return it == d.monitor.options.end() ? 0 : static_cast<int>(std::ranges::distance(d.monitor.options.begin(), it));
}

auto gse::selected_resolution(const window::data& d, const int monitor_index) -> std::optional<resolution_info> {
	if (d.resolution.value == native_resolution_option) {
		return std::nullopt;
	}
	const auto resolutions = window::enumerate_resolutions(monitor_index);
	const auto it = std::ranges::find_if(
		resolutions,
		[&](const resolution_info& candidate) {
			return std::format("{}", candidate) == d.resolution.value;
		}
	);
	return it == resolutions.end() ? std::nullopt : std::optional(*it);
}

auto gse::desired_present_mode(const window::data& d) -> gpu::present_mode {
	return d.attached ? gpu::present_mode::mailbox : d.present_mode;
}

auto gse::apply_cursor_mode(window::data& d) -> void {
	const bool want_capture = d.primary.focused
		&& window_visible(d.primary.handle)
		&& !d.cursor_suppressed
		&& (d.primary.cursor_captured || !d.mouse_visible);

	set_cursor_capture(d.primary, want_capture);
	d.current_cursor_captured = want_capture;
}

auto gse::set_cursor_capture(window::window_surface& s, const bool capture) -> void {
	const HWND handle = to_hwnd(s.handle);

	if (s.cursor_capture_active != capture) {
		s.cursor_capture_active = capture;

		const RAWINPUTDEVICE mouse{
			.usUsagePage = hid_usage_page_generic,
			.usUsage = hid_usage_generic_mouse,
			.dwFlags = capture ? 0u : ridev_remove,
			.hwndTarget = capture ? handle : nullptr,
		};
		RegisterRawInputDevices(&mouse, 1, sizeof(mouse));

		if (capture) {
			SetCursor(nullptr);
		}
		else {
			ClipCursor(nullptr);
			SendMessageW(handle, wm_setcursor, reinterpret_cast<WPARAM>(handle), make_lparam(static_cast<int>(ht_client), static_cast<int>(wm_mousemove)));
		}
	}

	if (!capture) {
		return;
	}

	RECT client{};
	GetClientRect(handle, &client);

	POINT top_left{
		.x = client.left,
		.y = client.top,
	};
	POINT bottom_right{
		.x = client.right,
		.y = client.bottom,
	};
	ClientToScreen(handle, &top_left);
	ClientToScreen(handle, &bottom_right);

	const RECT clip{
		.left = top_left.x,
		.top = top_left.y,
		.right = bottom_right.x,
		.bottom = bottom_right.y,
	};
	ClipCursor(&clip);
}

auto gse::monitor_work_area(const int monitor_index) -> std::optional<rect_t<vec2i>> {
	const auto records = monitors();
	if (monitor_index < 0 || monitor_index >= static_cast<int>(records.size())) {
		return std::nullopt;
	}
	return records[monitor_index].work_area;
}

auto gse::set_window_style(const HWND handle, const DWORD style) -> void {
	const auto current = static_cast<DWORD>(GetWindowLongPtrW(handle, gwl_style));
	SetWindowLongPtrW(handle, gwl_style, static_cast<LONG_PTR>(style | (current & ws_visible)));
	SetWindowPos(handle, nullptr, 0, 0, 0, 0, swp_frame_changed | swp_no_move | swp_no_size | swp_no_zorder | swp_no_activate);
}

auto gse::apply_frame_appearance(const HWND handle) -> void {
	const int dark = 1;
	(void)DwmSetWindowAttribute(handle, dwmwa_use_immersive_dark_mode, &dark, static_cast<DWORD>(sizeof(dark)));

	const DWORD corners = dwmwcp_round;
	(void)DwmSetWindowAttribute(handle, dwmwa_window_corner_preference, &corners, static_cast<DWORD>(sizeof(corners)));
}

auto gse::center_window_in_work_area(const window::data& d, const rect_t<vec2i>& work_area) -> void {
	const HWND handle = to_hwnd(d.primary.handle);
	const bool maximized = IsZoomed(handle) != 0;
	if (maximized) {
		ShowWindow(handle, sw_restore);
	}

	const auto current = client_rect_in_screen(handle);
	const vec2i origin = work_area.top_left();
	const vec2i extent = work_area.size();
	const int width = std::min(current.size().x(), extent.x());
	const int height = std::min(current.size().y(), extent.y());

	set_client_rect(
		d.primary,
		{ origin.x() + (extent.x() - width) / 2, origin.y() + (extent.y() - height) / 2 },
		{ width, height }
	);

	if (maximized) {
		ShowWindow(handle, sw_show_maximized);
	}
}

auto gse::monitor_index_for_window(const vec2i position, const vec2i size) -> int {
	const auto records = monitors();

	int best_index = -1;
	int best_overlap = 0;

	for (const auto& [index, record] : std::views::enumerate(records)) {
		const vec2i origin = record.work_area.top_left();
		const vec2i extent = record.work_area.size();

		const int overlap_x = std::min(position.x() + size.x(), origin.x() + extent.x()) - std::max(position.x(), origin.x());
		const int overlap_y = std::min(position.y() + size.y(), origin.y() + extent.y()) - std::max(position.y(), origin.y());
		if (overlap_x <= 0 || overlap_y <= 0) {
			continue;
		}

		if (const int overlap = overlap_x * overlap_y; overlap > best_overlap) {
			best_overlap = overlap;
			best_index = static_cast<int>(index);
		}
	}

	return best_index;
}

auto gse::client_rect_in_screen(const HWND handle) -> rect_t<vec2i> {
	RECT client{};
	GetClientRect(handle, &client);

	POINT origin{
		.x = client.left,
		.y = client.top,
	};
	ClientToScreen(handle, &origin);

	return rect_t<vec2i>::from_position_size(
		{ static_cast<int>(origin.x), static_cast<int>(origin.y) },
		{ static_cast<int>(client.right - client.left), static_cast<int>(client.bottom - client.top) }
	);
}

auto gse::set_client_rect(const window::window_surface& s, const vec2i position, const vec2i size) -> void {
	const HWND handle = to_hwnd(s.handle);

	if (s.custom_frame) {
		SetWindowPos(handle, nullptr, position.x(), position.y(), size.x(), size.y(), swp_no_zorder | swp_no_activate);
		return;
	}

	RECT outer{
		.left = position.x(),
		.top = position.y(),
		.right = position.x() + size.x(),
		.bottom = position.y() + size.y(),
	};
	adjust_window_rect_for_dpi(
		&outer,
		static_cast<DWORD>(GetWindowLongPtrW(handle, gwl_style)),
		static_cast<DWORD>(GetWindowLongPtrW(handle, gwl_exstyle)),
		dpi_for_window(handle)
	);

	SetWindowPos(handle, nullptr, outer.left, outer.top, outer.right - outer.left, outer.bottom - outer.top, swp_no_zorder | swp_no_activate);
}

auto gse::surface_at(window::data& d, const vec2i screen_point) -> window::window_surface* {
	const HWND under = WindowFromPoint({
		.x = screen_point.x(),
		.y = screen_point.y(),
	});
	if (under == nullptr) {
		return nullptr;
	}
	const HWND root = GetAncestor(under, ga_root);
	for (const auto& surface : d.secondaries) {
		if (to_hwnd(surface->handle) == root) {
			return surface.get();
		}
	}
	return to_hwnd(d.primary.handle) == root ? &d.primary : nullptr;
}

auto gse::pointer_event_rank(const input::event& e) -> int {
	if (std::holds_alternative<input::mouse_scrolled>(e)
		|| std::holds_alternative<input::mouse_button_pressed>(e)
		|| std::holds_alternative<input::mouse_button_released>(e)) {
		return 2;
	}
	return std::holds_alternative<input::mouse_moved>(e) ? 1 : 0;
}

auto gse::os_restore_geometry(const window::data& d) -> std::optional<window::geometry> {
	if (!d.primary.custom_frame) {
		return std::nullopt;
	}

	WINDOWPLACEMENT placement{ .length = static_cast<UINT>(sizeof(WINDOWPLACEMENT)) };
	if (!GetWindowPlacement(to_hwnd(d.primary.handle), &placement)) {
		return std::nullopt;
	}

	const auto& normal = placement.rcNormalPosition;
	return window::geometry{
		.x = static_cast<int>(normal.left),
		.y = static_cast<int>(normal.top),
		.width = static_cast<int>(normal.right - normal.left),
		.height = static_cast<int>(normal.bottom - normal.top),
		.maximized = placement.showCmd == sw_show_maximized
			|| (placement.flags & wpf_restore_to_maximized) != 0,
	};
}

auto gse::cloak_window(const native_window_handle handle) -> void {
	const int cloak = 1;
	(void)DwmSetWindowAttribute(to_hwnd(handle), dwmwa_cloak, &cloak, static_cast<DWORD>(sizeof(cloak)));
}

auto gse::probe_composition(const window::data& d) -> window::composition_probe {
	const HWND handle = to_hwnd(d.primary.handle);

	DWORD cloaked = 0;
	(void)DwmGetWindowAttribute(handle, dwmwa_cloaked, &cloaked, static_cast<DWORD>(sizeof(cloaked)));

	return {
		.iconified = IsIconic(handle) != 0,
		.visible = IsWindowVisible(handle) != 0,
		.cloaked = static_cast<unsigned int>(cloaked),
	};
}

auto gse::plausible_restore_geometry(const window::geometry& g) -> bool {
	return g.width >= minimum_restore_size.x() && g.height >= minimum_restore_size.y();
}

auto gse::effective_restore_geometry(const window::data& d) -> window::geometry {
	window::geometry g = d.saved_geometry;
	if (!plausible_restore_geometry(g)) {
		g.width = default_window_size.x();
		g.height = default_window_size.y();
	}
	return g;
}

auto gse::restore_window_geometry(window::data& d) -> void {
	const window::geometry saved = effective_restore_geometry(d);
	d.restore_maximized = saved.maximized;

	const auto work_area = monitor_work_area(monitor_index_for_window({ saved.x, saved.y }, { saved.width, saved.height }));
	if (!work_area) {
		return;
	}

	const vec2i origin = work_area->top_left();
	const vec2i extent = work_area->size();

	const int width = std::min(saved.width, extent.x());
	const int height = std::min(saved.height, extent.y());
	const int x = std::clamp(saved.x, origin.x(), origin.x() + extent.x() - width);
	const int y = std::clamp(saved.y, origin.y(), origin.y() + extent.y() - height);

	set_client_rect(d.primary, { x, y }, { width, height });
}

auto gse::reanchor_saved_geometry(window::data& d, const int monitor_index) -> void {
	window::geometry& saved = d.saved_geometry;
	if (saved.width <= 0 || saved.height <= 0) {
		return;
	}

	const int saved_index = monitor_index_for_window({ saved.x, saved.y }, { saved.width, saved.height });
	if (saved_index == monitor_index) {
		return;
	}

	const auto target = monitor_work_area(monitor_index);
	if (!target) {
		return;
	}

	const vec2i origin = target->top_left();
	const vec2i extent = target->size();
	const int width = std::min(saved.width, extent.x());
	const int height = std::min(saved.height, extent.y());

	vec2i offset{ (extent.x() - width) / 2, (extent.y() - height) / 2 };
	if (const auto source = monitor_work_area(saved_index)) {
		offset = vec2i{ saved.x, saved.y } - source->top_left();
	}

	saved.width = width;
	saved.height = height;
	saved.x = std::clamp(origin.x() + offset.x(), origin.x(), origin.x() + extent.x() - width);
	saved.y = std::clamp(origin.y() + offset.y(), origin.y(), origin.y() + extent.y() - height);
}

auto gse::record_window_geometry(window::data& d) -> void {
	if (d.restore_maximized || d.current_display_mode != display_mode::windowed) {
		return;
	}

	const int monitor_index = monitor_index_for_window(d.primary.position, d.primary.size);

	if (const auto os = os_restore_geometry(d)) {
		if (plausible_restore_geometry(*os)) {
			d.saved_geometry = *os;
		}
	}
	else if (!window::minimized(d.primary.handle)) {
		if (IsZoomed(to_hwnd(d.primary.handle)) != 0) {
			d.saved_geometry.maximized = true;
			reanchor_saved_geometry(d, monitor_index);
		}
		else {
			const window::geometry current{
				.x = d.primary.position.x(),
				.y = d.primary.position.y(),
				.width = d.primary.size.x(),
				.height = d.primary.size.y(),
				.maximized = false,
			};
			if (plausible_restore_geometry(current)) {
				d.saved_geometry = current;
			}
		}
	}

	if (const int selected = selected_monitor_index(d);
		monitor_index >= 0 && monitor_index != selected && selected == d.last_monitor_index) {
		set_choice_index(d.monitor, static_cast<std::size_t>(monitor_index));
		d.last_monitor_index = monitor_index;
	}
}

auto gse::enter_exclusive_mode(std::wstring device, std::string name, const resolution_info& resolution) -> bool {
	DEVMODEW mode{};
	mode.dmSize = sizeof(mode);
	mode.dmPelsWidth = static_cast<DWORD>(resolution.width);
	mode.dmPelsHeight = static_cast<DWORD>(resolution.height);
	mode.dmDisplayFrequency = static_cast<DWORD>(resolution.refresh_rate);
	mode.dmBitsPerPel = 32;
	mode.dmFields = dm_pelswidth | dm_pelsheight | dm_displayfrequency | dm_bitsperpel;

	if (ChangeDisplaySettingsExW(device.c_str(), &mode, nullptr, cds_fullscreen, nullptr) != disp_change_successful) {
		log::println(
			log::level::warning,
			log::category::render,
			"[window] could not switch {} to {}; keeping the current mode",
			name,
			resolution
		);
		return false;
	}

	exclusive_device = std::move(device);
	invalidate_monitors();
	return true;
}

auto gse::leave_exclusive_mode() -> void {
	if (exclusive_device.empty()) {
		return;
	}
	ChangeDisplaySettingsExW(exclusive_device.c_str(), nullptr, nullptr, 0, nullptr);
	exclusive_device.clear();
	invalidate_monitors();
}

auto gse::apply_display_mode(window::data& d, const display_mode mode) -> void {
	if (d.current_display_mode == mode) {
		return;
	}

	if (d.current_display_mode == display_mode::windowed && mode != display_mode::windowed) {
		record_window_geometry(d);
	}

	d.current_display_mode = mode;

	if (mode != display_mode::windowed) {
		apply_fullscreen_placement(d, selected_monitor_index(d));
		return;
	}

	leave_exclusive_mode();

	const HWND handle = to_hwnd(d.primary.handle);
	set_window_style(handle, windowed_style);

	const window::geometry saved = effective_restore_geometry(d);
	set_client_rect(d.primary, { saved.x, saved.y }, { saved.width, saved.height });

	if (saved.maximized) {
		ShowWindow(handle, sw_show_maximized);
	}
}

auto gse::apply_fullscreen_placement(const window::data& d, const int monitor_index) -> void {
	const auto records = monitors();
	assert(!records.empty(), "Failed to get monitors!");

	const int selected = std::clamp(monitor_index, 0, static_cast<int>(records.size()) - 1);
	const rect_t<vec2i> bounds = records[selected].bounds;
	std::wstring device = records[selected].device;
	std::string name = records[selected].name;

	rect_t<vec2i> target = bounds;

	if (d.current_display_mode == display_mode::exclusive_fullscreen) {
		if (const auto chosen = selected_resolution(d, selected); chosen && enter_exclusive_mode(std::move(device), std::move(name), *chosen)) {
			target = rect_t<vec2i>::from_position_size(bounds.top_left(), { chosen->width, chosen->height });
		}
	}
	else {
		leave_exclusive_mode();
	}

	const HWND handle = to_hwnd(d.primary.handle);
	set_window_style(handle, fullscreen_style);
	SetWindowPos(
		handle,
		nullptr,
		target.top_left().x(),
		target.top_left().y(),
		target.size().x(),
		target.size().y(),
		swp_no_zorder | swp_no_activate
	);
}

auto gse::place_window_on_monitor(const window::data& d, const int monitor_index) -> bool {
	const auto work_area = monitor_work_area(monitor_index);
	if (!work_area) {
		return false;
	}

	if (d.current_display_mode == display_mode::windowed) {
		center_window_in_work_area(d, *work_area);
	}
	else {
		apply_fullscreen_placement(d, monitor_index);
	}

	return true;
}

auto gse::create_surface_window(window::window_surface& surface, const std::string& title, const vec2i size, const bool custom_frame) -> bool {
	surface.custom_frame = custom_frame;

	RECT outer{
		.left = 0,
		.top = 0,
		.right = size.x(),
		.bottom = size.y(),
	};
	if (!custom_frame) {
		adjust_window_rect_for_dpi(&outer, windowed_style, 0, reference_dpi);
	}

	const std::wstring wide_title = widen(title);
	const HWND handle = CreateWindowExW(
		0,
		window_class_name,
		wide_title.c_str(),
		windowed_style,
		cw_usedefault,
		cw_usedefault,
		outer.right - outer.left,
		outer.bottom - outer.top,
		nullptr,
		nullptr,
		GetModuleHandleW(nullptr),
		&surface
	);

	if (handle == nullptr) {
		log::println(
			log::level::error,
			log::category::render,
			"[window] could not create the window '{}' at {}: win32 error {}",
			title,
			size,
			GetLastError()
		);
		return false;
	}

	surface.handle = to_native_handle(handle);
	surface.content_scale = content_scale_of(surface.handle);
	apply_frame_appearance(handle);

	if (custom_frame) {
		SetWindowPos(handle, nullptr, 0, 0, 0, 0, swp_frame_changed | swp_no_move | swp_no_size | swp_no_zorder | swp_no_activate);
	}

	return true;
}

auto gse::create_window(window::data& d) -> void {
	main_thread_id.store(GetCurrentThreadId(), std::memory_order_release);

	const WNDCLASSEXW description{
		.cbSize = sizeof(WNDCLASSEXW),
		.style = cs_hredraw | cs_vredraw,
		.lpfnWndProc = reinterpret_cast<WNDPROC>(&window_proc),
		.hInstance = GetModuleHandleW(nullptr),
		.hCursor = nullptr,
		.lpszClassName = window_class_name,
	};
	assert(RegisterClassExW(&description) != 0, "Failed to register the window class");

	if (d.title.empty()) {
		d.title = "GSEngine";
	}

	const window::geometry initial = effective_restore_geometry(d);
	d.primary.id = find_or_generate_id("primary_window");
	const bool created = create_surface_window(d.primary, d.title, { initial.width, initial.height }, d.native_frame);
	assert(created, "Failed to create the main window!");

	refresh_monitor_settings(d);
	d.last_monitor_index = selected_monitor_index(d);
	refresh_resolution_settings(d);

	d.current_present_mode = desired_present_mode(d);

	restore_window_geometry(d);

	if (d.launch_launcher_size.x() > 0 && d.launch_launcher_size.y() > 0 && d.current_display_mode == display_mode::windowed) {
		d.launcher_active = true;
		apply_launcher_mode(d, {
			static_cast<int>(static_cast<float>(d.launch_launcher_size.x()) * d.primary.content_scale),
			static_cast<int>(static_cast<float>(d.launch_launcher_size.y()) * d.primary.content_scale),
		});
	}
}

auto gse::apply_launcher_mode(window::data& d, const vec2i size) -> void {
	const HWND handle = to_hwnd(d.primary.handle);

	if (d.launcher_active) {
		const bool zoomed = IsZoomed(handle) != 0;
		const bool was_maximized = zoomed || std::exchange(d.restore_maximized, false);
		if (zoomed) {
			ShowWindow(handle, sw_restore);
		}

		const auto current = client_rect_in_screen(handle);

		if (!d.launcher_restore) {
			d.launcher_restore = {
				.x = current.top_left().x(),
				.y = current.top_left().y(),
				.width = current.size().x(),
				.height = current.size().y(),
				.maximized = was_maximized,
			};
		}

		const int width = std::max(1, size.x());
		const int height = std::max(1, size.y());
		set_client_rect(
			d.primary,
			{
				current.top_left().x() + (current.size().x() - width) / 2,
				current.top_left().y() + (current.size().y() - height) / 2,
			},
			{ width, height }
		);
	}
	else if (const auto restore = std::exchange(d.launcher_restore, std::nullopt)) {
		set_client_rect(d.primary, { restore->x, restore->y }, { restore->width, restore->height });
		if (restore->maximized) {
			ShowWindow(handle, sw_show_maximized);
		}
	}
}

auto gse::window::tick(scheduler& sched, data& d) -> void {
	if (!d.primary.handle) {
		create_window(d);
	}

	{
		trace::scope_guard _{ trace_id<"window::poll">() };
		poll_events();
	}

	if (++d.housekeeping_frame % housekeeping_interval_frames == 0) {
		{
			trace::scope_guard _{ trace_id<"window::clipboard">() };
			clipboard::sync(to_hwnd(d.primary.handle));
		}

		{
			trace::scope_guard _{ trace_id<"window::content_scale">() };
			d.primary.content_scale = content_scale_of(d.primary.handle);
		}
	}

	const std::optional<window_open_file_request> open_file = apply_requests(sched, d);

	{
		trace::scope_guard _{ trace_id<"window::cursor_mode">() };
		apply_cursor_mode(d);
	}

	if (d.primary.focused) {
		trace::scope_guard _{ trace_id<"window::modes">() };
		if (const display_mode wanted = d.attached ? display_mode::windowed : d.display_mode; d.current_display_mode != wanted) {
			apply_display_mode(d, wanted);
		}

		if (const gpu::present_mode desired = desired_present_mode(d); d.current_present_mode != desired) {
			d.current_present_mode = desired;
			d.primary.framebuffer_resized = true;
		}
	}

	d.primary.present_mode = d.current_present_mode;
	d.primary.attached = d.attached;

	{
		trace::scope_guard _{ trace_id<"window::geometry">() };
		sync_primary_geometry(d);
	}

	if (const int selected = selected_monitor_index(d); selected != d.last_monitor_index) {
		if (place_window_on_monitor(d, selected)) {
			d.last_monitor_index = selected;
		}
		else {
			set_choice_index(d.monitor, static_cast<std::size_t>(d.last_monitor_index));
		}
	}

	{
		trace::scope_guard _{ trace_id<"window::monitor_scan">() };
		if (const int monitor_index = monitor_index_for_window(d.primary.position, d.primary.size); monitor_index != d.current_monitor_index) {
			d.current_monitor_index = monitor_index;
			d.primary.monitor_key = monitor_key_for_index(monitor_index);
		}
	}

	if (open_file) {
		sched.make_channel_writer().push<window_open_file_result>({
			.path = prompt_for_file(d.primary, *open_file),
		});
	}

	{
		trace::scope_guard _{ trace_id<"window::focus">() };
		route_input(d);
	}

	{
		trace::scope_guard _{ trace_id<"window::secondaries">() };
		sync_secondaries(sched, d);
	}
}

auto gse::window::apply_requests(scheduler& sched, data& d) -> std::optional<window_open_file_request> {
	for (const auto& [focus] : sched.read_channel<ui_focus_request>()) {
		set_ui_focus(d, focus);
	}

	for (const auto& [capture] : sched.read_channel<cursor_capture_request>()) {
		d.primary.cursor_captured = capture;
	}

	for (const auto& req : sched.read_channel<window_popout_request>()) {
		window_surface* created = create_secondary(d, {
			.title = req.title.empty() ? req.menu_name : req.title,
			.size = req.size,
			.position = req.screen_position,
			.use_position = true,
		});
		if (!created) {
			sched.make_channel_writer().push<window_popout_failed>({ .for_menu = req.menu_name });
			continue;
		}

		sched.make_channel_writer().push<window_opened>({
			.id = created->id,
			.handle = created->handle,
			.position = created->position,
			.size = created->size,
			.present_mode = created->present_mode,
			.for_menu = req.menu_name,
		});
	}

	for (const auto& req : sched.read_channel<window_minimize_request>()) {
		if (const window_surface* surface = find_surface(d, req.window); surface && surface->handle) {
			win32::ShowWindow(to_hwnd(surface->handle), win32::sw_minimize);
		}
	}

	for (const auto& req : sched.read_channel<window_toggle_maximize_request>()) {
		if (const window_surface* surface = find_surface(d, req.window); surface && surface->handle) {
			const win32::HWND handle = to_hwnd(surface->handle);
			win32::ShowWindow(handle, win32::IsZoomed(handle) != 0 ? win32::sw_restore : win32::sw_show_maximized);
		}
	}

	for (const auto& req : sched.read_channel<window_close_request>()) {
		if (window_surface* surface = find_surface(d, req.window)) {
			surface->should_close = true;
		}
	}

	for (const auto& req : sched.read_channel<window_focus_request>()) {
		if (const window_surface* surface = find_surface(d, req.window); surface && surface->handle) {
			win32::SetForegroundWindow(to_hwnd(surface->handle));
		}
	}

	for (const auto& req : sched.read_channel<window_resize_request>()) {
		if (const window_surface* surface = find_surface(d, req.window); surface && surface->handle) {
			set_client_rect(
				*surface,
				client_rect_in_screen(to_hwnd(surface->handle)).top_left(),
				{ std::max(1, req.size.x()), std::max(1, req.size.y()) }
			);
		}
	}

	for (const auto& req : sched.read_channel<window_launcher_mode_request>()) {
		d.launcher_active = req.active;
		apply_launcher_mode(d, { req.width, req.height });
	}

	for (const auto& req : sched.read_channel<window_chrome_metrics_request>()) {
		window_surface* surface = find_surface(d, req.window);
		if (!surface) {
			continue;
		}
		surface->chrome_caption_height = req.caption_height;
		surface->chrome_controls_width = req.controls_width;
		surface->chrome_resize_exclude_y0 = req.resize_exclude_y0;
		surface->chrome_resize_exclude_y1 = req.resize_exclude_y1;
	}

	for (const auto& req : sched.read_channel<window_locate_cursor_request>()) {
		const window_surface* source = find_surface(d, req.source);
		if (!source) {
			continue;
		}

		const auto source_client = window::viewport(source->handle);
		const vec2i screen{
			source->position.x() + static_cast<int>(req.client_cursor.x()),
			source->position.y() + (source_client.y() - static_cast<int>(req.client_cursor.y())),
		};

		window_cursor_located located{ .source = req.source, .screen_cursor = screen };
		if (const window_surface* under = surface_at(d, screen)) {
			const auto client = window::viewport(under->handle);
			const vec2i local = screen - under->position;
			located.window = under == &d.primary ? id() : under->id;
			located.client_cursor = vec2f{
				static_cast<float>(local.x()),
				static_cast<float>(client.y() - local.y()),
			};
		}

		sched.make_channel_writer().push<window_cursor_located>(located);
	}

	cursor_shape desired_cursor = cursor_shape::arrow;
	for (const auto& req : sched.read_channel<set_cursor_shape_request>()) {
		if (req.shape != cursor_shape::arrow) {
			desired_cursor = req.shape;
		}
	}
	if (desired_cursor != current_cursor_shape) {
		current_cursor_shape = desired_cursor;
		if (!d.primary.cursor_capture_active) {
			win32::SetCursor(cursor_for_shape(desired_cursor));
		}
	}

	std::optional<window_open_file_request> open_file;
	for (const auto& req : sched.read_channel<window_open_file_request>()) {
		open_file = req;
	}
	return open_file;
}

auto gse::window::route_input(data& d) -> void {
	const window_surface* focused = &d.primary;
	for (const auto& surface : d.secondaries) {
		if (surface->handle && surface->focused) {
			focused = surface.get();
			break;
		}
	}
	d.focused_window = focused == &d.primary ? id{} : focused->id;

	std::vector<std::pair<const window_surface*, std::vector<input::event>>> drained;
	drained.emplace_back(&d.primary, d.primary.input_events.drain());
	for (const auto& surface : d.secondaries) {
		drained.emplace_back(surface.get(), surface->input_events.drain());
	}

	const window_surface* pointer = nullptr;
	int best_rank = 0;
	for (const auto& [surface, events] : drained) {
		int rank = 0;
		for (const auto& event : events) {
			rank = std::max(rank, pointer_event_rank(event));
		}
		if (rank > 0 && rank >= best_rank) {
			pointer = surface;
			best_rank = rank;
		}
	}

	if (pointer) {
		d.cursor_window = pointer == &d.primary ? id{} : pointer->id;
	}
	else if (const window_surface* held = find_surface(d, d.cursor_window)) {
		pointer = held;
	}
	else {
		pointer = &d.primary;
		d.cursor_window = id{};
	}

	for (const auto& [surface, events] : drained) {
		for (const auto& event : events) {
			if (surface == (pointer_event_rank(event) > 0 ? pointer : focused)) {
				d.primary.input_events.push(event);
			}
		}
	}
}

auto gse::window::sync_secondaries(scheduler& sched, data& d) -> void {
	for (std::size_t i = d.secondaries.size(); i-- > 0;) {
		if (d.secondaries[i]->should_close) {
			sched.make_channel_writer().push<window_closed>({ .id = d.secondaries[i]->id });
			destroy_secondary(d, d.secondaries[i].get());
		}
	}

	for (const auto& surface : d.secondaries) {
		if (!surface->handle) {
			continue;
		}
		const auto frame = client_rect_in_screen(to_hwnd(surface->handle));
		if (const vec2i position = frame.top_left(); position != surface->position) {
			surface->position = position;
			sched.make_channel_writer().push<window_moved>({ .id = surface->id, .position = position });
		}
		if (const vec2i size = frame.size(); size != surface->size) {
			surface->size = size;
			sched.make_channel_writer().push<window_resized>({ .id = surface->id, .size = size });
		}
		surface->content_scale = content_scale_of(surface->handle);
	}
}

auto gse::window::prompt_for_file(const window_surface& s, const window_open_file_request& request) -> std::filesystem::path {
	std::wstring filter = win32::widen(request.filter_name);
	filter.push_back(L'\0');
	filter.append(win32::widen(request.filter_pattern));
	filter.push_back(L'\0');
	filter.push_back(L'\0');

	const std::wstring title = win32::widen(request.title);

	std::wstring buffer(win32::max_path, L'\0');
	if (!win32::open_file_dialog(
		to_hwnd(s.handle),
		title.c_str(),
		filter.c_str(),
		buffer.data(),
		static_cast<win32::DWORD>(buffer.size())
	)) {
		return {};
	}

	buffer.resize(std::wcslen(buffer.c_str()));
	return buffer;
}

auto gse::window::shutdown(data& d) -> void {
	for (const auto& surface : d.secondaries) {
		if (surface->handle) {
			win32::DestroyWindow(to_hwnd(surface->handle));
			surface->handle = {};
		}
	}
	d.secondaries.clear();

	if (d.primary.handle) {
		win32::DestroyWindow(to_hwnd(d.primary.handle));
		d.primary.handle = {};
	}

	leave_exclusive_mode();
}

auto gse::window::poll_events() -> void {
	win32::MSG msg{};
	while (win32::PeekMessageW(&msg, nullptr, 0, 0, win32::pm_remove) != 0) {
		win32::TranslateMessage(&msg);
		win32::DispatchMessageW(&msg);
	}
}

auto gse::window::wait_events(const time timeout) -> void {
	const auto budget = static_cast<win32::DWORD>(std::max(0.f, timeout.as<milliseconds>()));
	win32::MsgWaitForMultipleObjectsEx(0, nullptr, budget, win32::qs_allinput, win32::mwmo_inputavailable);
	poll_events();
}

auto gse::window::post_wake() -> void {
	if (const win32::DWORD thread = main_thread_id.load(std::memory_order_acquire); thread != 0) {
		win32::PostThreadMessageW(thread, win32::wm_null, 0, 0);
	}
}

auto gse::window::sync_primary_geometry(data& d) -> void {
	const win32::HWND handle = to_hwnd(d.primary.handle);

	const auto frame = client_rect_in_screen(handle);
	const vec2i position = frame.top_left();
	const vec2i size = frame.size();
	const bool changed = position != d.primary.position || size != d.primary.size;

	if (const composition_probe composition = probe_composition(d); composition != d.primary.last_composition) {
		const auto framebuffer = window::viewport(d.primary.handle);
		log::println(
			log::category::render,
			"[window] composition iconified {}->{} visible {}->{} cloaked {}->{} rect={},{} {}x{} fb={}x{}",
			d.primary.last_composition.iconified,
			composition.iconified,
			d.primary.last_composition.visible,
			composition.visible,
			d.primary.last_composition.cloaked,
			composition.cloaked,
			position.x(),
			position.y(),
			size.x(),
			size.y(),
			framebuffer.x(),
			framebuffer.y()
		);
		d.primary.last_composition = composition;
		if (d.attached && composition.visible && composition.cloaked == 0) {
			cloak_window(d.primary.handle);
		}
	}

	const int previous_monitor = monitor_index_for_window(d.primary.position, d.primary.size);
	const int current_monitor = monitor_index_for_window(position, size);

	if (size != d.primary.size || current_monitor != previous_monitor) {
		const auto framebuffer = window::viewport(d.primary.handle);
		log::println(
			log::category::render,
			"[window] rect {},{} {}x{} -> {},{} {}x{} fb={}x{} iconified={} zoomed={} monitor={}->{} setting={}",
			d.primary.position.x(),
			d.primary.position.y(),
			d.primary.size.x(),
			d.primary.size.y(),
			position.x(),
			position.y(),
			size.x(),
			size.y(),
			framebuffer.x(),
			framebuffer.y(),
			win32::IsIconic(handle) != 0,
			win32::IsZoomed(handle) != 0,
			previous_monitor,
			current_monitor,
			d.monitor.value
		);
	}

	d.primary.position = position;
	d.primary.size = size;

	if (changed && !d.launcher_restore) {
		record_window_geometry(d);
	}
}

auto gse::window::frame_buffer_resized(window_surface& s) -> bool {
	if (s.framebuffer_resized) {
		s.framebuffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window::create_secondary(data& d, const secondary_window_desc& desc) -> window_surface* {
	auto surface = std::make_unique<window_surface>();
	surface->id = generate_temp_id(stable_id(desc.title));
	surface->size = desc.size;
	surface->ui_focus = true;

	if (!create_surface_window(*surface, desc.title, desc.size, true)) {
		return nullptr;
	}

	if (desc.use_position) {
		set_client_rect(*surface, desc.position, desc.size);
		surface->position = desc.position;
	}

	ShowWindow(to_hwnd(surface->handle), sw_show);
	surface->shown = true;

	window_surface* raw = surface.get();
	d.secondaries.push_back(std::move(surface));
	return raw;
}

auto gse::window::find_surface(data& d, const id id) -> window_surface* {
	if (!id.exists() || d.primary.id == id) {
		return &d.primary;
	}
	const auto it = std::ranges::find_if(d.secondaries, [id](const auto& held) {
		return held->id == id;
	});
	return it == d.secondaries.end() ? nullptr : it->get();
}

auto gse::window::destroy_secondary(data& d, window_surface* surface) -> void {
	const auto it = std::ranges::find_if(d.secondaries, [surface](const auto& held) {
		return held.get() == surface;
	});
	if (it == d.secondaries.end()) {
		return;
	}
	if ((*it)->handle) {
		win32::DestroyWindow(to_hwnd((*it)->handle));
		(*it)->handle = {};
	}
	d.secondaries.erase(it);
}

auto gse::window::show(data& d) -> void {
	win32::ShowWindow(to_hwnd(d.primary.handle), win32::sw_show);
	d.primary.shown = true;

	if (std::exchange(d.restore_maximized, false)) {
		win32::ShowWindow(to_hwnd(d.primary.handle), win32::sw_show_maximized);
	}
}

auto gse::window::minimized(const native_window_handle handle) -> bool {
	if (IsIconic(to_hwnd(handle)) != 0) {
		return true;
	}
	const auto client = window::viewport(handle);
	return client.x() == 0 || client.y() == 0;
}

auto gse::window_visible(const native_window_handle handle) -> bool {
	return handle && IsWindowVisible(to_hwnd(handle)) != 0;
}

auto gse::window::viewport(const native_window_handle handle) -> vec2i {
	RECT client{};
	GetClientRect(to_hwnd(handle), &client);
	return { static_cast<int>(client.right - client.left), static_cast<int>(client.bottom - client.top) };
}

auto gse::monitor_key_for_index(const int index) -> std::string {
	if (index < 0) {
		return {};
	}

	const auto records = monitors();
	if (index >= static_cast<int>(records.size())) {
		return {};
	}

	const auto& record = records[index];
	return std::format("{} {}x{}", record.name, record.width, record.height);
}

auto gse::content_scale_of(const native_window_handle handle) -> float {
	if (!handle) {
		return 1.f;
	}
	return static_cast<float>(dpi_for_window(to_hwnd(handle))) / static_cast<float>(reference_dpi);
}

auto gse::window::set_ui_focus(data& d, const bool focus) -> void {
	const bool was = d.primary.ui_focus;
	d.primary.ui_focus = focus;

	if (was || !focus) {
		return;
	}

	const auto dims = window::viewport(d.primary.handle);
	const double center_x = dims.x() / 2.0;
	const double center_y = dims.y() / 2.0;

	win32::POINT target{
		.x = static_cast<win32::LONG>(center_x),
		.y = static_cast<win32::LONG>(center_y),
	};
	win32::ClientToScreen(to_hwnd(d.primary.handle), &target);
	win32::SetCursorPos(target.x, target.y);

	d.primary.input_events.push(input::mouse_moved{ center_x, dims.y() - center_y });
}

auto gse::window::enumerate_resolutions(const int monitor_index) -> std::vector<resolution_info> {
	const auto records = monitors();
	if (monitor_index < 0 || monitor_index >= static_cast<int>(records.size())) {
		return {};
	}

	std::set<std::tuple<int, int, int>> seen;
	win32::DEVMODEW mode{};
	mode.dmSize = sizeof(mode);

	for (win32::DWORD index = 0; win32::EnumDisplaySettingsW(records[monitor_index].device.c_str(), index, &mode) != 0; ++index) {
		if (mode.dmBitsPerPel != 32) {
			continue;
		}
		seen.emplace(
			static_cast<int>(mode.dmPelsWidth),
			static_cast<int>(mode.dmPelsHeight),
			static_cast<int>(mode.dmDisplayFrequency)
		);
	}

	std::vector<resolution_info> result;
	result.reserve(seen.size());
	for (const auto& [width, height, refresh] : std::views::reverse(seen)) {
		result.push_back({
			.width = width,
			.height = height,
			.refresh_rate = refresh,
		});
	}
	return result;
}

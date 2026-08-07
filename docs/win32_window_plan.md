# Win32 Window Plan — removing GLFW

Status: scoped, not started.

Replace GLFW with a hand-written Win32 windowing/input backend. The engine is
Windows-only in practice (only `x64-mingw-*` presets exist; there is no
`__linux__`/`__APPLE__` guard anywhere in `Engine/Engine/Source`), and the
editor already fights GLFW for control of the window procedure.

## Verdict

Tractable. This is an implementation swap behind an interface that is already
in the right place, not an architectural change. The public module interface
does not change at all.

---

## 1. Current surface

GLFW reaches into six places. Everything else in the engine is already blind to
it.

| Location | Usage | Disposition |
|---|---|---|
| `Engine/Source/Os/GLFW/Window.cpp` | 1184 lines, ~80 call sites | Rewritten — this is the job |
| `Engine/Source/Vulkan/Instance.cppm:76-90` | `glfwVulkanSupported`, `glfwGetRequiredInstanceExtensions`, `glfwCreateWindowSurface` | Replaced with `vkCreateWin32SurfaceKHR` (see H4) |
| `Engine/Source/External/Win32.cppm:41,82` | `glfwGetWin32Window` / `hwnd_from_glfw_window` | Collapses to a cast; `Dx12/Device.cpp:161` is the only external caller |
| `Engine/Source/External/Glfw.cppm` | 286-line re-export wrapper | Deleted |
| `Engine/Source/Graphics/2D/Gui/Types.cpp:18` | `import gse.glfw;`, zero API usage | Dead line, delete |
| `Engine/CMakeLists.txt:61,118` | `find_package(glfw3)`, link `glfw` | Deleted |

`Engine/Source/Os/GLFW/{Keys,Input,InputEvents,InputState,Actions}.cppm`
(1664 lines) contain **no GLFW references**. They are platform-neutral and are
not touched by this work beyond the enclosing directory rename.

## 2. What is already in our favour

- **The interface is GLFW-free.** `Os/GLFW/Window.cppm` exposes
  `native_window_handle { void* }`, a settings/ECS-shaped `window::data`, and an
  `input::event` variant queue. No consumer above the OS layer knows GLFW
  exists, so the blast radius stops at one `.cpp`.
- **Module identity does not change.** The implementation stays
  `module gse.os:window_impl;`. `Engine/Import/Os.cppm` keeps
  `export import :window;` unchanged. CMake uses
  `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so moving files needs no CMakeLists edit
  beyond dropping the dependency.
- **We already own a window procedure.** `install_native_frame`
  (`Window.cpp:975`) subclasses via `SetWindowLongPtrW` and handles
  `WM_NCCALCSIZE` / `WM_NCHITTEST` in `native_frame_proc`. That logic ports over
  verbatim, and the subclass indirection (`SetPropW`, `CallWindowProcW`, the
  never-freed `new native_frame_state`) disappears entirely.
- **`gse.win32` already exports most of what we need** — `SetWindowPos`,
  `GetClientRect`, `MonitorFromWindow`, `GetMonitorInfoW`, the `HT*`/`SWP_*`
  constants, plus the file dialog which is already native.
- **`gse::key` values do not have to move.** They are GLFW codes, but GLFW's
  codes are ASCII across the printable range. Keep the enum byte-identical and
  translate `VK_*` to it in the keyboard handler. This protects `Actions.cppm`
  (1151 lines) and every persisted keybind. Same for `mouse_button`.
- **No gamepad/joystick usage at all** — the whole GLFW input subsystem beyond
  keyboard/mouse is dead weight we simply drop.

## 3. Target structure

```
Engine/Source/Os/Win32/
  Window.cppm        unchanged interface (moved)
  Window.cpp         new Win32 implementation, module gse.os:window_impl
  Keys.cppm          unchanged (moved)
  Input.cppm         unchanged (moved)
  InputEvents.cppm   unchanged (moved)
  InputState.cppm    unchanged (moved)
  Actions.cppm       unchanged (moved)
```

`native_window_handle::value` becomes the `HWND` directly, so
`win32::hwnd_from_glfw_window(h)` becomes `static_cast<HWND>(h.value)` and the
GLFW native header include leaves `Win32.cppm`.

Deliver the directory change as a `git mv` rather than an in-place edit, per the
module-rename lesson — module identity is unchanged here, which is the safe
case, but a clean move keeps it that way.

---

## 4. Work breakdown

### Stage 0 — Window, class, pump (~250 lines)

`RegisterClassExW` → `CreateWindowExW` (no `WS_VISIBLE`; `ShowWindow` later, as
today) → `PeekMessageW`/`Translate`/`Dispatch` in `window::poll_events`.

Fold `native_frame_proc` into the primary window procedure. Route `WM_CLOSE` to
an owned `should_close` bool, replacing `glfwWindowShouldClose`
(`Window.cpp:1047`). `WM_DESTROY`/`shutdown` replace `glfwDestroyWindow`.

Must also handle here:
- **DPI awareness** before anything else (H1).
- **Modal pump** — `WM_ENTERSIZEMOVE`/`WM_EXITSIZEMOVE` + `SetTimer` (H6).
- `window_handle_minimized` → `IsIconic`, `window_handle_viewport` →
  `GetClientRect` (GLFW's framebuffer size is the client area for a
  `NO_API` window, so this maps directly).

### Stage 1 — Rendering back up (~60 lines)

Vulkan surface via dynamically-loaded `vkCreateWin32SurfaceKHR` (H4). Required
instance extensions become the fixed pair `VK_KHR_surface` +
`VK_KHR_win32_surface`, replacing `glfwGetRequiredInstanceExtensions`.

DX12 is a one-line change at `Dx12/Device.cpp:161`.

### Stage 2 — Keyboard, mouse, text (~300 lines)

- `WM_KEYDOWN`/`WM_SYSKEYDOWN`/`WM_KEYUP` → `VK_*` → `gse::key` table, pushing
  `input::key_pressed`/`key_released`.
- `WM_LBUTTONDOWN` family → `input::mouse_button_*` with cursor position.
- `WM_MOUSEMOVE` → `input::mouse_moved`, preserving the existing `ui_focus`
  clamp-and-invert-Y branch at `Window.cpp:482-496`.
- `WM_MOUSEWHEEL`/`WM_MOUSEHWHEEL` → `input::mouse_scrolled`, normalising by
  `WHEEL_DELTA` to match GLFW's ±1 units.
- `WM_CHAR` → `input::text_entered`, with surrogate-pair assembly (H10).
- `WM_SETFOCUS`/`WM_KILLFOCUS` → `d.focused`.
- `WM_SIZE` → `d.framebuffer_resized`.

### Stage 3 — Cursor (~200 lines)

Shapes: `LoadCursorW(IDC_*)` mapped from `cursor_shape`, plus a `WM_SETCURSOR`
handler (H7).

Captured mode: this is the single largest hidden cost (H5). `apply_cursor_mode`
is three GLFW lines today and becomes hide + `ClipCursor` + per-frame recenter +
virtual position accumulator, ideally with `RegisterRawInputDevices` /
`WM_INPUT` for unaccelerated camera look.

### Stage 4 — Monitors, fullscreen, DPI (~350 lines)

- `enumerate_monitors` → `EnumDisplayMonitors` + `GetMonitorInfoW`; work area
  from `rcWork` (direct match for `glfwGetMonitorWorkarea`).
- `enumerate_resolutions` → `EnumDisplaySettingsW`, filtered to 32bpp, deduped
  by `(w, h, refresh)` as today.
- `apply_display_mode` → style swap `WS_OVERLAPPEDWINDOW` ↔ `WS_POPUP` +
  `SetWindowPos` for borderless; `ChangeDisplaySettingsExW` for exclusive.
- `window_handle_content_scale` → `GetDpiForWindow / 96.0f`, plus
  `WM_DPICHANGED` to honour the suggested rect.
- Geometry save/restore, honouring H2.

### Stage 5 — Cleanup (~100 lines)

Clipboard via `OpenClipboard`/`GetClipboardData(CF_UNICODETEXT)` behind the
existing main-thread `sync_clipboard` bridge (which stays — the main-thread
constraint is identical). `glfwFocusWindow` → `SetForegroundWindow`. Launcher
mode and `apply_commands` port mechanically.

Then delete `Glfw.cppm`, the dead `Types.cpp` import, the GLFW include in
`Win32.cppm`, and the CMake dependency.

---

## 5. Migration hazards

These are the parts that bite, roughly in order of how quietly they fail.

**H1 — DPI awareness is silently inherited from GLFW.** `glfwInit` calls
`SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)`. Remove GLFW without
replacing it and the process becomes DPI-virtualised: everything renders blurry
and upscaled, and `content_scale` reports 1.0 forever. Must be the first call at
startup, before any window exists. Cheap to fix, expensive to diagnose.

**H2 — Client-rect vs window-rect drift.** `glfwGetWindowPos`/`Size` report the
**client** area; `GetWindowRect` reports the **outer** frame. `saved_geometry`
is persisted to settings. For `native_frame` windows the two coincide (our
`WM_NCCALCSIZE` returns 0), but the decorated path they do not — so a naive
swap grows the window on every save/restore cycle. This is the same failure mode
as the existing native-frame geometry drift. Decision: standardise on the client
rect to match GLFW's persisted semantics, and derive the outer rect with
`AdjustWindowRectExForDpi` where needed. Keep `set_window_frame_rect` as the
single placement chokepoint.

**H3 — `monitor_key` is a persisted string built from the monitor name.**
`glfwGetMonitorName` returns an EDID-derived friendly name ("Dell U2720Q");
`GetMonitorInfoW`'s `szDevice` returns `\\.\DISPLAY1`. `monitor_key` is
`"{name} {w}x{h}"` and the monitor index is a saved setting, so changing the
name source silently invalidates saved monitor selection. Either replicate the
EDID lookup (`EnumDisplayDevices` with `EDD_GET_DEVICE_INTERFACE_NAME`, then
registry) or accept a one-time reset — but decide deliberately, don't discover
it.

**H4 — Vulkan surface must not drag `windows.h` into the Vulkan module.**
`VulkanModule` does not define `VK_USE_PLATFORM_WIN32_KHR`, so
`vk::Win32SurfaceCreateInfoKHR` does not exist. Adding the define pulls
`windows.h` into a module every Vulkan TU imports — straight into the known GCC
bug where a module including `windows.h` alongside `import std` cannot use STL
class templates. Recommended instead: hand-declare the five-field
`VkWin32SurfaceCreateInfoKHR` POD and fetch `vkCreateWin32SurfaceKHR` through
`vkGetInstanceProcAddr`. The build already uses
`VULKAN_HPP_DISPATCH_LOADER_DYNAMIC`, so this fits the existing pattern and
needs no new headers at all. Same tactic `DirectX.cppm` already uses.

**H5 — Captured-cursor mode is a feature, not a call.** Beyond hide/clip/
recenter, the clip **must** be released on `WM_KILLFOCUS` or alt-tab leaves the
user's cursor trapped with no way out. Budget real time here; it is the one
place where GLFW was doing meaningful work for us.

**H6 — The modal loop stalls rendering.** Win32 blocks inside `DefWindowProc`
during window drag/resize and menu tracking. Without a `WM_ENTERSIZEMOVE` +
`SetTimer` pump the frame loop freezes mid-drag, which reads as a hang.

**H7 — `WM_SETCURSOR`.** Windows resets the cursor to the class cursor on every
mouse move unless handled. Skipping it makes cursor shapes flicker back to
arrow — obvious, but easy to forget until the editor resize handles look broken.

**H8 — Borderless and exclusive fullscreen are currently identical.** Both paths
run through `glfwSetWindowMonitor` (`Window.cpp:404`), so the two settings
options behave the same today. Win32 lets us actually distinguish them. That is
a genuine improvement but it is *new behaviour*, not a port — treat as a
follow-up unless we consciously opt in.

**H9 — Keep the module identity fixed.** `gse.os:window_impl` must not be
renamed as part of this. File moves are fine; identity changes risk the phantom
cycle.

**H10 — `WM_CHAR` is UTF-16.** `input::text_entered` carries a `std::uint32_t`
codepoint, so astral-plane characters arriving as two surrogate messages must be
buffered and combined. Relevant given the editor's text input.

## 6. Explicitly out of scope

- Cross-platform support. This closes the door on a cheap Linux/macOS port; the
  door is currently closed anyway.
- IME (`WM_IME_*`) for CJK composition. GLFW does not give us this today either,
  so it is not a regression — but note it if editor i18n ever matters.
- Splitting borderless from exclusive fullscreen (H8).
- Any change to `Keys.cppm` / `Actions.cppm` / input state.

## 7. Estimate

~1200–1500 lines of new Win32, roughly three sittings:

1. Stages 0–1: window, pump, DPI, surface — back to rendering.
2. Stages 2–3: input, cursor. **Most of the time lives here**, in captured-cursor
   and raw input.
3. Stages 4–5: monitors, fullscreen, DPI change, clipboard, teardown.

Stages 0–2 can land while everything above the OS layer keeps compiling
untouched, since the interface is unchanged.

## 8. Verification checklist

- [ ] App is per-monitor DPI aware; `content_scale` tracks a monitor move.
- [ ] Save/restore geometry is stable across repeated launches on both the
      decorated and `native_frame` paths (H2).
- [ ] Existing `settings.ini` still selects the same monitor after migration, or
      the reset is documented (H3).
- [ ] Editor custom chrome: drag, double-click-maximise, edge resize, and the
      scrollbar/resize priority carve-outs all still work.
- [ ] Frame loop keeps rendering during a window drag (H6).
- [ ] Alt-tab out of captured-cursor mode releases the cursor (H5).
- [ ] Cursor shapes hold over editor resize handles (H7).
- [ ] Non-ASCII paste and typing into the code panel survive (H10).
- [ ] Both Vulkan and DX12 backends present.
- [ ] `grep -ri glfw Engine Editor Sandbox` returns nothing.

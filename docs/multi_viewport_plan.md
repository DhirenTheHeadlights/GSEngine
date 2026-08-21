# Multi-viewport / multi-window plan

Status as of the end of phase 0b. Read `docs/STYLEGUIDE.md` and `docs/CODE_REVIEW_GUIDE.md` first; this document assumes them.

## Goal and shape

Let an editor panel be torn out into its own OS window. The chosen shape is **(B) GUI-only secondary windows**: the main window keeps the entire render pipeline unchanged, and secondary windows get a swapchain, a frame, and a UI-only pass.

Full multi-viewport — (A), where every window owns a render graph and can host arbitrary 3D — was scoped and rejected. `Runtime/Engine.cpp` drives one `window_state` → one `begin_frame` → one `m_scheduler.render()` → one `end_frame`, and that render call runs *every* `[[= system_frame]]` system exactly once. N windows would mean teaching the scheduler which window a frame system targets, i.e. a scheduler redesign. Nothing in the editor needs it: a popped-out panel is only `sprite_command`/`text_command`, and the game viewport already arrives as a shared surface via the attached path.

## Done

### 0a′ — nested `[[= shared]]`
`Ecs/SharedView.cppm` gained `publish_kind::nested`. A field qualifies when it is a class type with its own `[[= shared]]` members (`publishes_nested<T>()`), ordered *before* the `value` check so a trivially-copyable nested struct is published rather than copied wholesale. `shared_field_specs` emits `shared_view_base<field_t>::type` / `shared_snapshot_base<field_t>::type`; `copy_shared_field` recurses. Aggregate construction was factored into `live_shared_aggregate` / `snapshot_shared_aggregate`, which `make_shared_view_live` / `make_shared_view_snapshot` now delegate to.

Recursion here is **indirect** — `shared_field_specs<Data>` instantiates `shared_field_specs<field_t>`, a different specialisation, through `define_aggregate`. It is *not* the self-recursive-consteval-returning-`std::vector` shape recorded in the July BMI bug, so that bug is neither triggered nor proven fixed by this working.

Nested publishing through a **collection** was deliberately not built: it is a per-element view-materialisation problem, not a struct-shape transform, and nothing needs it.

### 0a — `viewport_state`
`gui::data` split. `viewport_state` (in `Gui.cppm`) holds 26 fields: `menu_stack`, `menus`, `current_menu`, `current_scope`, `next_z_order`, `visible_menu_ids_last_frame`, `name_to_menu_id`, `suppressed_menus`, `active_dock_space`, `active_drag_ghost`, `current_state`, `hot`/`active`/`focus_widget_id`, `tooltip`, `input_layer_render`, `input_suppressed`, `input_layers_data`, `context_menu`, `screen_surface`, `manual_cursor`, `pending_popout_close_ids`, `pending_tab_close`, `fstate`, `rect`, `previous_viewport_size`, `previous_scale_factor`, `display_scale`, `active_monitor_key`.

`data` keeps the settings category (theme, scales, fonts, `reserve_top_bar`, `file_path`, `ui_scale_by_monitor`), shared resources (`fonts`, `blank_texture`, text pools), the widget caches (`widget_scrolls`, `widget_anim_colors` — keyed by stable widget id, so sharing is correct), the command buffers, and `[[= shared]] viewport_state primary`.

`fstate` and `display_scale` being per-viewport is what makes per-viewport DPI fall out of phase 2 for free: style scale derives from a viewport's own height and monitor.

Only two shared fields are read outside the GUI — `menus` (`PopoutSystem`) and `menu_stack` (Sandbox `GameUI`, `PauseMenuSystem`). Both mean "the main window", so both reach through `primary`.

### 0b — viewport threading
Nineteen functions in `Gui.cpp` now receive the viewport explicitly.

Pure-viewport (take `viewport_state&`, no `data` at all): `begin_menu`, `end_menu`, `calculate_display_rect`, `remove_tab_from_host`, `handle_dragging_state`, `handle_resizing_state`, `handle_resizing_divider_state`, `handle_pending_drag_state`. `handle_idle_state` takes `(const font_set&, viewport_state&, …)`.

Both (take `(data&, viewport_state&, …)`): `process_menu`, `process_screen`, `draw_menu_chrome`, `draw_tab_bar`, `process_context_menu`, `sync_monitor_scale`, `scale_factor_for`, `apply_scale`. `draw_dock_space` takes `(data&, const style&, …)` — it only wanted `fstate.sty`.

`usable_screen_rect` became pure geometry — `(float top_inset, window_s)` — computed once per frame into `viewport_state::rect`. That removed the last reason the drag/resize handlers needed `data`, and `vp.rect` is the field that becomes the window client rect in phase 1.

`update_body` was decomposed into three per-viewport entry points, each verified to contain **zero** `d.primary` references:

| function | lines | role |
|---|---|---|
| `begin_viewport_frame(d, vp, window_s, viewport_size)` | 107 | scale, menu rescale-on-resize, `fstate`, `rect`, `name_to_menu_id` |
| `update_viewport_interaction(d, vp, window_s, input_state)` | 33 | drag/resize state machine |
| `update_viewport(d, vp, input_st, viewport_size, requests_in, ui_out)` | 195 | dock space, ghost, screens, menu content, context menu, tooltip, z-order |

`update_body` is down to 120 lines. Every remaining `d.primary` in it is a *selection* of which viewport to run.

The "zero `d.primary`" claim was **verified against the tree, not asserted** — the first pass had left nine of them behind (`sync_monitor_scale`, `scale_factor_for`, `apply_scale` in `begin_viewport_frame`; all five `handle_*` calls in `update_viewport_interaction`; `process_screen`, `process_menu`, two `remove_tab_from_host`, `process_context_menu` in `update_viewport`) plus four more in `process_menu`/`draw_menu_chrome`, which take `vp` and were still reaching past it. `update_viewport_interaction` also referenced `frame_sty`, a local left behind in `begin_viewport_frame` — that one was a build error, the other thirteen would have compiled and silently made every secondary viewport drive the primary's state.

That is the failure mode to expect from the rest of this refactor: threading a parameter through compiles fine while still reading the old global. The enumeration check below is the only thing that catches it. `frame_sty` is now `const style& frame_sty = vp.fstate.sty` — the same value, since `begin_viewport_frame` writes it — which keeps the call sites readable without re-deriving the style.

Two blocks were relocated to make the per-viewport region contiguous, both on argument rather than convenience:
- screen-request drains moved **earlier**, ahead of the dock-space block — they only mutate `menu_stack`/`manual_cursor`, and the first reader is `menu_stack.empty()` further down. They stay in `update_body`, outside any future loop: screens belong to the primary viewport.
- the OS cursor block moved **later**, past the z-order pass — one cursor per process, so it is shared.

Also removed: `clear_menu_interaction`, which had zero callers repo-wide and whose last caller was the source of a drag bug (see `editor-dock-tree` memory — never re-add it).

### 0b′ — the `gse.graphics:gui` partition split
`Gui.cppm` (377) + `Gui.cpp` (2487) became eight interface/impl pairs. Every function body moved by line-range extraction and was verified byte-identical against the original; the only content change is that the drag-ghost and tooltip blocks inlined in `update_viewport` became `draw_drag_ghost` and `update_tooltip`.

| partition | owns | cpp |
|---|---|---|
| `:gui` | `data`, `viewport_state`, `frame_state`, `init`/`run`/`shutdown`/`save` | 319 |
| `:gui_frame` | `begin_viewport_frame`, `update_viewport_interaction`, `update_viewport` | 268 |
| `:gui_drag_resize` | the five `handle_*_state` | 875 |
| `:gui_chrome` | title bar, tab bar, dock-space visuals, `menu_chrome_height`, `tab_index_at`, `caption_button` | 457 |
| `:gui_menu` | `begin_menu`, `end_menu`, `process_menu`, `calculate_display_rect` | 221 |
| `:gui_screen` | `process_screen`, `draw_screen_caption` | 178 |
| `:gui_overlay` | `process_context_menu`, `update_tooltip`, `draw_drag_ghost` | 315 |
| `:gui_scale` | `apply_scale`, `scale_factor_for`, `sync_monitor_scale`, `reload_font`, `usable_screen_rect`, `intern_text` | 109 |

`init_body` and `update_body` were deleted in the same pass. They were pass-through coroutines from the May 2026 coroutine-loop refactor (`204ff2a6`) — `init`/`run` `co_await`ed them forwarding every parameter verbatim, with no shared setup, so each call allocated a second coroutine frame for nothing (`run` once per frame). No other engine system has the pattern; `popout_system::run` is annotated the same way and simply *is* its body. `init` and `run` now hold the bodies directly.

Interface graph is a star on `:gui` — no sibling interface imports another, so cross-partition calls happen only in the `:x_impl` units and there are no interface cycles. `:gui`'s exported surface dropped from 30 names to six: `data`, `viewport_state`, `init`, `run`, `shutdown`, `save`. The three helpers the editor actually calls are exported from their owning partitions instead (`apply_scale` from `:gui_scale`; `caption_button`, `menu_chrome_height`, `tab_index_at` from `:gui_chrome`). No CMakeLists change was needed — `.cppm` is globbed and `module gse.graphics:x_impl;` matches the impl regex.

**Why this matters for phase 0.** The "zero `d.primary`" property of the three per-viewport entry points was a convention that only an enumeration pass could check, and the first attempt at 0b left thirteen violations that would have compiled. It is now a *file-level* invariant: `primary` appears in `Gui.cpp` and nowhere else in the eight files. Grep is the check.

## Remaining in phase 0

### 0c — viewport-targeted content
`update_viewport` drains the whole `menu_content` channel, so with two viewports every panel is drawn in both. Worse than double-drawing: `begin_menu` **auto-creates** a menu when the name misses `vp.name_to_menu_id`, so viewport B would materialise a stray floating "Explorer" at (100,100) — the exact failure mode recorded as dock-tree invariant 1.

**A target id on `menu_content` was scoped and rejected.** There are 16 producers (`AllocPanel` in `Engine/DevTools`, `Engine/Server/Application`, `PopoutSystem`, Editor `EditorApp`/`Agent/System`/`Terminal`, Sandbox `GameUI`), and which viewport a panel lives in is known only to the dock tree. Making producers declare it contradicts dock-tree invariant 1 — `AllocPanel` must not know the editor has a dock tree — and would put a field on the message that 14 of 16 call sites could only ever fill with "primary".

**Resolve by name instead.** `vp.name_to_menu_id` is already the authority for "does this viewport host this panel", rebuilt every frame in `begin_viewport_frame`, and `suppressed_menus` already means "registered but not shown here". A viewport claims a content item iff it resolves. Producers change not at all; this is the same division of labour as today (gui owns `name_to_menu_id` resolution, the editor owns rect assignment).

That leaves one gap: brand-new content whose name no viewport knows yet must be adopted by exactly one viewport, or it appears everywhere or nowhere. So:

- `viewport_state` gains `bool adopts_unclaimed_content = false`, set true for `primary` only.
- `begin_menu` returns `false` instead of creating when the name misses and `!vp.adopts_unclaimed_content`. The create path stays exactly as it is for the adopting viewport.

This is a field on `viewport_state`, not a threaded bool parameter — `viewport_state&` is already on that call path.

Ordering note: it must be `begin_menu` that gates, not `process_menu` — `process_menu` is not the only caller, and gating higher up would re-open the auto-create hole for any future direct `begin_menu` user.

**Untestable until 0g.** With one viewport, `primary` adopts everything and behaviour is byte-identical. The acceptance test is criterion 1 (two independent dock trees) plus a negative: no stray (100,100) menu in the secondary.

### 0d — tag the UI command stream
`sprite_command` / `text_command` (`Graphics/Renderers/UiRenderer.cppm`) carry no target. `gui` pushes into one flat channel and `UiRenderer::run` drains it.

**This is invisible in one window** — sprite rects are absolute screen space, so two viewports at different rects render correctly untagged. It only becomes load-bearing in phase 2 when viewports go to different swapchains. Ship it with the debug toggle from 0g that renders *only viewport N*, or it goes untested until phase 2 breaks.

### 0e — input routing
Route by `vp.rect` containment: the viewport under the cursor is live, others get `input_suppressed`. This is the same decision phase 1 makes as "which window has focus", so get the shape right here.

### 0f — editor per-viewport
`sync_dock_menus` / `update_dock_interaction` currently take `gui::data&` and reach `s.primary`. They need `viewport_state&`. A `dock_tree` per viewport follows in phase 3.

### 0g — the harness and the acceptance test
Add `secondaries` (a non-shared collection on `data` — nothing outside the GUI reads secondary viewports) and turn the three `d.primary` call sites in `update_body` into loops. `begin_viewport_frame` already takes `viewport_size` as a parameter precisely so the primary can pass the render-graph extent while a secondary passes its own rect size.

Phase 0 is done when, with the frame split into two viewports:
1. two independent dock trees render side by side
2. the mouse routes to the viewport under the cursor
3. a panel drags from one viewport into the other
4. focus is isolated — typing in one does not disturb the other's caret, hover or active widget
5. a toggle renders only viewport N (this is what proves 0d)

## Phase 1 — window plurality
Primary `window::data` keeps settings, geometry and the native frame. Secondary windows go in a new collection with their own event queue. GLFW callbacks already receive `GLFWwindow*` and `to_native_handle` exists, so routing is mechanical.

- `glfwCreateWindow` is **main-thread only** — same constraint that forced the clipboard bridge through `window::tick`.
- Secondary windows should start OS-decorated. The native frame is installed on one HWND, and tool windows do not want custom chrome.
- `window::data` is referenced 34 times across 15 files (Dx12, Vulkan, Gpu/Context, Gpu/Device, Renderer, Gui, Input). Most of those mean "the primary window" and should keep meaning that.

## Phase 2 — per-window presentation
Swapchain + frame per window; the device stays shared. `frame::begin/end` already take a `window::data*`, and `PresentPacer` is already per-swapchain.

- **`[[= stable_shared]]` breaks.** `device`, `swapchain`, `frame`, `render_graph` are all `stable_shared unique_ptr` on `gpu::context::data`. That annotation is a write-once, address-stable contract and the infrastructure *asserts* if a stable pointer is reseated after publication. Per-window swapchain/frame cannot live under it.
- `gpu::context::shutdown` must remain the only `system_shutdown` touching GPU state; per-window swapchains are destroyed there, not by the window system.
- Surface-lost recovery has five documented destroy-order invariants that must now hold per window.
- `render_graph` holds `swap_chain* m_swapchain` and `extent()` derives from it.

## Phase 3 — editor pop-out
A `dock_tree` per viewport; tearing past the window bounds creates one; `[dock]` gains a window index and geometry. Closing the last panel in a viewport destroys the window.

## Standing hazards

- **Address stability.** `current_menu` is a raw `menu*` into `vp.menus`, and `viewport_state` in a vector-backed collection means adding a viewport invalidates any held `viewport_state&`. **Viewports may only be created or destroyed at frame boundaries**, never mid-draw. This fails as a use-after-free, not a compile error.
- **`[[= shared]]` does not publish through a collection** (see 0a′). If a secondary viewport ever needs external publication, that is a real feature, not a one-liner.
- **Verify renames by enumeration, not by eye.** This refactor moved ~300 call sites. The check that works is: list every `d.X` surviving in a file and confirm it is exactly the settings/shared set. A regex matching `d` followed by `,` or `)` misses `[&d]` capture lists — that exact hole shipped one build failure.
- **Do not pre-empt recorded toolchain bugs.** Write the natural form, build, and only fall back if it reproduces (`feedback-retest-toolchain-bugs-before-working-around`). New module partitions *are* still authored split (interface + `:x_impl`) — that one was reproduced this session, not merely recorded.

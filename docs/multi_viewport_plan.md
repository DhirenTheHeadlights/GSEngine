# Multi-viewport / multi-window plan

Status: phases 0–3 complete. Phase 2 shipped in its **corrected** form — one `frame`, one `render_graph`, N swapchains (`frame::add_present_target`, `pass_builder::target(id)`, `window_presentation { window, surface, swapchain }`) — not the "graph per window" shape 2b chose; read the two singleton sections before touching any of it. The remaining known gap is phase 0's acceptance criterion 3, cross-viewport drag. Read `docs/STYLEGUIDE.md` and `docs/CODE_REVIEW_GUIDE.md` first; this document assumes them.

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

**0b shipped one silent regression, found during 0g bring-up.** Turning `usable_screen_rect` into the cached field `viewport_state::rect` changed `init` from computing it live —

```cpp
const rectf screen_rect = usable_screen_rect(d, window_s);   // pre-0b
const rectf screen_rect = d.primary.rect;                    // 0b
```

— but `vp.rect` is written **only** in `begin_viewport_frame`, which runs in `run`, i.e. after `init`. So at init the field was a default-constructed zero rect, and init's clamp loop (`clamped_width = min(m.rect.width(), screen_rect.width())`) collapsed **every menu loaded from `gui_layout.ini` to zero size**. The menus were drawn each frame with no extent, and the next frame's rescale block saw no viewport/scale change so nothing ever repaired them. Symptom: saved dev overlays silently invisible; freshly adopted ones (created at 100..400 × 100..300) fine — which is why it hid behind the 0c work for so long.

`init` now seeds `d.primary.frame_rect` from the window and computes `d.primary.rect` through the same `usable_screen_rect` call, reproducing the pre-0b inset exactly (`fstate` is default-constructed at init, so the top inset is 0 as before). **A cached per-frame field read during init is the general shape of this bug** — `fstate` has the same hazard and is already documented under the dock-tree memory.

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

### 0c — viewport-targeted content
`update_viewport` drains the whole `menu_content` channel, so with two viewports every panel is drawn in both. Worse than double-drawing: `begin_menu` **auto-creates** a menu when the name misses `vp.name_to_menu_id`, so viewport B would materialise a stray floating "Explorer" at (100,100) — the exact failure mode recorded as dock-tree invariant 1.

**A target id on `menu_content` was scoped and rejected.** There are 16 producers (`AllocPanel` in `Engine/DevTools`, `Engine/Server/Application`, `PopoutSystem`, Editor `EditorApp`/`Agent/System`/`Terminal`, Sandbox `GameUI`), and which viewport a panel lives in is known only to the dock tree. Making producers declare it contradicts dock-tree invariant 1 — `AllocPanel` must not know the editor has a dock tree — and would put a field on the message that 14 of 16 call sites could only ever fill with "primary".

**Resolve by name instead.** `vp.name_to_menu_id` is already the authority for "does this viewport host this panel", rebuilt every frame in `begin_viewport_frame`, and `suppressed_menus` already means "registered but not shown here". A viewport claims a content item iff it resolves. Producers change not at all; this is the same division of labour as today (gui owns `name_to_menu_id` resolution, the editor owns rect assignment).

That leaves one gap: brand-new content whose name no viewport knows yet must be adopted by exactly one viewport, or it appears everywhere or nowhere. So:

- `viewport_state` gains `bool adopts_unclaimed_content = false`, set true for `primary` only.
- `begin_menu` returns `false` instead of creating when the name misses and `!vp.adopts_unclaimed_content`. The create path stays exactly as it is for the adopting viewport.

### The content invariant — every menu draws SOMEWHERE

**`claims_content(d, vp, name)`, checked in `process_menu` before `begin_menu`, is the whole rule:**

1. `vp` hosts the name (`vp.name_to_menu_id` resolves it) → `vp` claims it.
2. `vp` does not adopt → skip.
3. `vp` adopts, but some *other* viewport hosts the name → skip.
4. otherwise → the adopting viewport claims and creates it.

Because rule 4 is unconditional once no host exists, a menu can never fall through every viewport. If its state is destroyed or corrupted, the primary re-creates it at (100,100) on the next frame. **This is a self-healing invariant and it is the reason the gate is a positive claim rather than a negative skip — do not "optimise" it into an early-out that can leave a name unhandled.**

The first cut had only the `adopts_unclaimed_content` bool, and it was **actively destructive**. Menu ids are stable per tag (`generate_id(tag)` → `stable_id(tag)`), and `id_mapped_collection::add` **silently returns `nullptr` on a duplicate id**. So:

1. `migrate_menu` moved "Profiler" primary → secondary.
2. The primary's content pass runs first, failed to resolve the name, and — adopting — **re-created "Profiler" in the primary with the same stable id**.
3. Next frame `migrate_menu` popped that copy out of the primary; `add` hit the duplicate id in the secondary, returned `nullptr`, and the popped menu was **destroyed**.

Adoption and migration fought each frame, double-drawing and leaking menus. Rule 3 above is what stops it: the primary now defers to a secondary that hosts the name. Both `migrate_menu` and the reclaim in `apply_viewport_layout` also guard on `to.menus.contains(id)` before popping, so a duplicate id can never silently eat a menu again.

**`add` returning `nullptr` on duplicate is a trap worth remembering** — every `menus.add` call site must either guarantee the id is absent or check first.

This is a field on `viewport_state`, not a threaded bool parameter — `viewport_state&` is already on that call path.

The gate is in `begin_menu`, not `process_menu`: `process_menu` already bails on `begin_menu() == false` *before* it touches `d.sprite_commands`, so nothing partial is emitted, and putting the gate at the creation site means no future direct `begin_menu` caller can re-open the hole. `begin_menu` currently has exactly one caller, so the two are equivalent today — the creation site is the one that stays correct.

`primary` is marked at its declaration — `[[= shared]] viewport_state primary{ .adopts_unclaimed_content = true }` — rather than in `init`, so an early `co_return` from `init` (font/texture load failure) cannot leave the process with no adopting viewport.

Verified: `requests_in.of<menu_content>()` returns a `channel_read_guard`, a **non-consuming** view over `const std::vector<T>&`. N viewports each re-read the full list, so the 0g loop needs no buffering.

With one viewport this is byte-identical — `primary` adopts everything, so `begin_menu` never reaches the new branch.

**Implementing this changed 0g's shape.** A freshly created secondary has an empty `menus` collection and `adopts_unclaimed_content = false`, so it now renders *nothing at all* — every content item is unclaimed and skipped. Acceptance criterion 1 ("two independent dock trees render side by side") is therefore not reachable by splitting the frame alone; the harness has to give the secondary something to own.

The right primitive is **migrate a menu by name from one viewport's `menus` to another's**. Content follows automatically, because `name_to_menu_id` is rebuilt from `menus` in `begin_viewport_frame` and 0c resolves against it — which is the same motion phase 3's tear-out performs. Build that, not a special-case seeding path. It must run at a frame boundary (`current_menu` is a raw `menu*`; see Standing hazards).

### 0e — input routing
`route_cursor(d, mouse)` runs once per frame in `run`, **before** the `begin_viewport_frame` loop, and elects exactly one owning viewport:

1. a viewport whose `current_state` is not `states::idle` — it captured the mouse on a previous frame and keeps it even when the cursor leaves its rect, so a drag started near an edge does not die halfway
2. otherwise the first secondary whose `frame_rect` contains the cursor
3. otherwise the primary

Election reads *last* frame's `current_state`, which is correct: `update_viewport_interaction` has not run yet this frame.

A viewport also wins the cursor when `menu_stack.captures_input()` — a modal screen owns the whole frame. Without this the settings screen was **unclosable while split**: `process_screen` lays a screen out against the full frame, but the primary's `frame_rect` is only its half, so every control past the seam belonged to the secondary and was dead. Screens stay full-frame by design (a modal covers the window); modal capture is what makes that coherent.

Ownership feeds two gates:
- `vp.input_suppressed = window_s.cursor_captured || !vp.owns_cursor` — widget input, via `draw_context`.
- `update_viewport_interaction` returns early and forces `states::idle` when `!vp.owns_cursor`. **This second gate is the load-bearing one and is easy to miss**: `input_suppressed` only reaches widgets through `draw_context`, while the drag/resize hit-test in `handle_idle_state` never consults it. Without the early return, both viewports hit-test the same cursor position and both start dragging.

Gating on `owns_cursor` rather than `input_suppressed` keeps the `cursor_captured` path byte-identical — today that flag suppresses widgets but still runs the state machine.

With one viewport `owns_cursor` is always true, so all of this is inert.

### 0g — the harness and the acceptance test
`data` gained `std::vector<std::unique_ptr<viewport_state>> secondaries` (non-shared) and the three `d.primary` call sites in `run` became loops. The `unique_ptr` is mandatory, not stylistic — see Address stability under Standing hazards.

**Per-frame state had to come out of `begin_viewport_frame` first.** It cleared `d.sprite_commands`, `d.text_commands` and flipped `text_pool_slot` — process-wide, not per-viewport. Looping it would have wiped viewport 0's commands when viewport 1 began. Those four lines now live in `run`, once, ahead of the loop. `ui_focus_request` moved out of `update_viewport` for the same reason: one window, one focus, and N viewports pushing conflicting values is a bug.

**`viewport_state::frame_rect`** is the viewport's screen-space rect before chrome inset. `usable_screen_rect` became `(top_inset, frame_rect)` — pure geometry, no `window::data` — and `vp.rect` derives from it. `run` writes the primary's from the window every frame, so an absent or stale layout request can never strand it.

Tooltip and context-menu clamping moved from `viewport_size` to `vp.frame_rect`, and both functions lost their now-dead `viewport_size` parameter. For the primary `frame_rect` is `{0, H, W, H}`, so the bounds are numerically identical to the old `[0,W]×[0,H]` clamp; for a secondary they are simply correct instead of flinging overlays into the other half.

**Clamping alone is not enough for tooltips.** A tooltip wider than its viewport clamps to the right edge, then the left-edge clamp wins, and it overflows into the neighbour — which is exactly what a half-width viewport produces with ordinary text. The tooltip sprite and text commands therefore also carry `clip_rect = vp.frame_rect`. This is a preview of 0d: absolute-screen-space commands are fine until two viewports are adjacent, and then anything unbounded leaks across the seam.

**The harness is a separate system**, `gse::gui::viewport_harness` in `:viewport_harness` — per `feedback-diagnostics-are-separate-systems` it must not be a branch inside `gui::run`. It cannot reach into `gui::data`, so it drives gui through two new channel messages in `:menu_stack`:

- `viewport_layout_request { std::vector<rectf> rects }` — `rects[0]` is the primary, the rest are secondaries. `apply_viewport_layout` grows/shrinks `secondaries` to match, **migrating a dying viewport's menus back to the primary** before popping it.
- `menu_migrate_request { menu_name, target_viewport }` — 0 is the primary, 1..N the secondaries.

Both are drained at the very top of `run`, before any menu is begun, which satisfies the frame-boundary rule for viewport creation. `migrate_menu` is idempotent (`&from == &to` returns false, as does a name that does not resolve), so the harness can push the same request every frame and self-heal when a panel registers late. It resets `z_order` to 0 so the menu re-enters the target's z stack, and clears the source's `current_menu`, `visible_menu_ids_last_frame` and `name_to_menu_id` entries.

`register_systems<^^gui>` already walks nested namespaces (`collect_namespace_systems` is an iterative worklist over `members_of`), so the harness registers itself. It ships off: `ViewportHarness.split=true`, `.split_ratio`, `.migrated_menu=<panel name>`.

Phase 0 is done when, with the frame split into two viewports:
1. two independent dock trees render side by side
2. the mouse routes to the viewport under the cursor
3. a panel drags from one viewport into the other
4. focus is isolated — typing in one does not disturb the other's caret, hover or active widget
5. ~~a toggle renders only viewport N~~ — replaced by 0d's blanket clip, which makes a mis-tagged command visibly clip to the wrong viewport

1, 2 and 4 are reachable now. **3 is not, and is not a 0g gap** — `handle_dragging_state` operates entirely within one `viewport_state`, so a cross-viewport tear needs the drag to detect it left `vp.frame_rect` and hand off to the elected viewport, reusing `migrate_menu`. That is phase 3 work; `route_cursor`'s capture rule is the hook it will hang on. 5 belongs to 0d.

### 0d — tag and clip the UI command stream — DONE
`sprite_command` / `text_command` (`Graphics/Renderers/UiRenderer.cppm`) each gained `std::uint32_t viewport = 0`.

**The old claim here — "this is invisible in one window" — was wrong, and the tooltip bug disproved it.** Adjacency alone breaks untagged absolute-space commands; you do not need separate swapchains. Anything whose extent can exceed its viewport bleeds into the neighbour, and clamping a *position* does not fix it: clamp right, then the left clamp wins, and it overflows anyway. That is ordinary behaviour for a half-width viewport, not a corner case.

Both jobs are done in **one** place, `stamp_viewport` in `run`, which walks the command range appended by each `update_viewport` call (the same range-stamp pattern `process_menu` uses for `z_order`):

```cpp
cmd.viewport = index;
cmd.clip_rect = cmd.clip_rect ? cmd.clip_rect->intersection(bounds) : bounds;
```

Doing it at the stamp rather than at each producer is the whole point: there were at least three unclipped absolute-space sources (drag ghost, dock-space cross, context-menu backing) and per-site clips would silently miss the fourth. Per-site clips were written first and then **deleted** — two mechanisms for one job is what rots. Producers that clip to something *smaller* (the drag-ghost label to its own box, tab text to the title bar) still work: the intersection preserves them.

This is safe because every command in the buffer originates inside a stamped range. The interaction handlers push none (verified: zero `sprite_commands` references in `DragResize.cpp`), and `cursor::render_to` runs after the loops and is deliberately left unstamped — one software cursor per process, free to draw anywhere.

**Adding a field to `sprite_command` / `text_command` breaks positional structured bindings.** `UiRenderer::run` decomposed `text_command` as `[font, text, position, scale, color, clip_rect, layer, z_order]`; the new `viewport` member made that "only 8 names provided for structured binding", and the *reported* error cascaded into a bogus `push_back` overload failure ten lines later. The sibling sprite loop directly above used `cmd.field` and was unaffected — so the text loop was converted to match. Phase 2 will add more fields to these structs; prefer `cmd.field` and grep for wide `auto& [a, b, c, …]` bindings before touching them. (`UiRenderer.cpp`'s remaining 8-name binding is over `batches`, a different type.)

**Screens stopped being an exception.** `update_viewport` lost its `viewport_size` parameter and derives `vp.frame_rect.size()` internally. That parameter fed exactly one thing, `process_screen`, which was the last place a viewport used the *frame's* size instead of its own — so the primary laid modals out full-frame while its own `rect` was a half. With split off `frame_rect` is the whole frame, so this is a **no-op in the normal case**; split, a modal now lives in its viewport, which is where phase 2 needs it anyway. Note `screen::body_rect` takes a *size* and so assumes origin (0,0): fine for the primary, and secondaries never receive screens (screen requests are drained to `primary` only). Giving a secondary a screen requires teaching `body_rect` an origin first.

**The "render only viewport N" toggle was dropped, deliberately.** Its only stated job was to prove the tagging, and it cannot be built without a diagnostic branch in the production drain plus a diagnostic field on `gui::data` — the exact shape rejected in `feedback-diagnostics-are-separate-systems`. The blanket clip proves the tag better and for free: a command stamped with the wrong viewport is clipped to the wrong rect and visibly vanishes or is cut in half. Mis-tagging is now self-evident on screen, so acceptance criterion 5 is satisfied by ordinary rendering rather than by a toggle.

### 0f — editor per-viewport — DONE
Four functions were threaded, and three turned out to be **pure-viewport** — they never touched a `gui::data` setting at all:

| function | new signature |
|---|---|
| `editor_menu` (`Chrome.cppm`) | `(gui::viewport_state&, std::string_view)` |
| `apply_pending_panel_close` | `(gui::viewport_state&, editor_app::data&)` |
| `sync_dock_menus` | `(gui::viewport_state&, editor_app::data&)` |
| `update_dock_interaction` | `(gui::data&, gui::viewport_state&, editor_app::data&, const dock_input&)` |

Only `update_dock_interaction` still needs `data`, for exactly three things: `current_theme` and `reserve_top_bar` (settings) and `fonts` (a shared resource) — i.e. the same settings/shared split `data` was reduced to in 0a. That it lands so cleanly is a check on 0a's field partition, not a coincidence.

**`.primary` now appears exactly once in the entire editor** (`EditorApp.cppm:703`), as `gui::viewport_state& vp = s.primary;` inside the `settings::change_request` apply lambda — the deliberate selection of which viewport the dock tree drives, and the single line phase 3 changes. Grep is the check, as in 0b′.

The dock tree itself is still singular and still lives on the primary. A `dock_tree` per viewport is phase 3; 0f only removes the editor's hard-coded reach into `primary` so that change becomes local.

Until this lands the harness can only migrate menus gui itself owns — an editor panel migrated into a secondary is still laid out by the editor's single dock tree, which computes rects against the primary. Expect a migrated editor panel to draw in the secondary at a primary-relative rect.

## Phase 0 — where it landed

Every sub-phase 0a…0g is implemented. Of the five acceptance criteria:

1. **two independent dock trees render side by side** — partially. Two viewports render independently and a migrated menu lives entirely in the secondary, but the *dock tree* is still singular (0f removed the editor's reach into `primary`; a tree per viewport is phase 3). Verified with gui-owned dev overlays.
2. **mouse routes to the viewport under the cursor** — yes (0e), with drag and modal capture.
3. **a panel drags from one viewport into the other** — **NOT DONE, and deliberately.** `handle_dragging_state` operates within a single `viewport_state` and has no concept of handing a drag across a boundary. This is phase 3 work, not a gap in phase 0's plumbing; see below.
4. **focus is isolated** — yes, falls out of `hot`/`active`/`focus_widget_id` being per-viewport.
5. ~~toggle renders only viewport N~~ — replaced by 0d's blanket clip (see 0d).

**Criterion 3 is the honest remaining gap.** A cross-viewport drag needs: detecting the cursor left `vp.frame_rect` mid-drag, choosing the destination viewport, migrating the menu (the `migrate_menu` primitive from 0g already does this), and transferring `current_state` so the drag continues in the destination. The first three exist; only the state hand-off is missing. It is small, but it is genuinely a *phase 3* feature (tear-out) rather than viewport plumbing, so it was not smuggled into phase 0.

Unverified at the time of writing: 0d and 0f are **built but not run**. 0c/0e/0g were tested against the split harness and pass.

## Phase 1 — window plurality — DONE (windows exist; input not yet consumed)

`window::data` split exactly like `gui::data` did in 0a: a new **`window_surface`** holds the per-window state, `data` keeps the settings, saved geometry and the native frame.

`window_surface` is not a guess — it is precisely the field set the GLFW callbacks touch (`input_events`, `ui_focus`, `handle`, `cursor_captured`, `focused`, `framebuffer_resized`), plus live `position`/`size`/`content_scale`/`monitor_key`/`shown`. `data` gained `[[= shared]] window_surface primary` and `std::vector<std::unique_ptr<window_surface>> secondaries` — `unique_ptr` for the same reason as viewports: the surface owns a `task::concurrent_queue`, so it is not movable, and addresses must be stable because GLFW's user pointer points at it.

**The user pointer is the whole trick.** Callbacks already did `static_cast<window::data*>(glfwGetWindowUserPointer(w))`; they now cast to `window_surface*` and are shared verbatim between primary and secondary via a new `attach_surface_callbacks(handle, surface)`. Not one callback body was duplicated.

The estimate in the old draft — "`window::data` is referenced 34 times across 15 files" — **overstated the work by an order of magnitude**. Only **10 direct field accesses exist outside `Os/GLFW`**; everything else already goes through accessors (`window::viewport`, `raw_handle`, `is_open`, `minimized`). The accessor layer was doing its job. Inside `Window.cpp` there were 55 sites, all mechanical.

New API: `create_secondary` / `destroy_secondary` / `close_requested` / `viewport(window_surface)` / `frame_rect(window_surface)`, driven by a `secondary_window_count_request` channel message that `window::tick` drains — main-thread by construction, since `tick` is called from `Engine.cpp`'s loop. Secondaries are created OS-decorated, honour their close button, refresh geometry and content scale each tick, and are destroyed in `window::shutdown` **before** the primary.

**What phase 1 deliberately does NOT do: consume secondary input.** Each secondary queues events, but nothing drains them, because there is nowhere correct to send them yet. A secondary's cursor coordinates are relative to *its own* client area, while `gui`'s viewport routing tests `vp.frame_rect.contains(mouse)` in a single space — which worked in 0g only because both viewports were halves of one window. Reconciling those is the same problem as giving a viewport its own swapchain, i.e. phase 2. Draining secondary queues before then would silently feed the primary's viewport garbage coordinates.

Testable now via `ViewportHarness.secondary_windows` (0..3): real decorated OS windows open, move, resize, report geometry, and close. They render nothing — that is phase 2.

- `glfwCreateWindow` is **main-thread only** — same constraint that forced the clipboard bridge through `window::tick`.
- Secondary windows should start OS-decorated. The native frame is installed on one HWND, and tool windows do not want custom chrome.
- `window::data` is referenced 34 times across 15 files (Dx12, Vulkan, Gpu/Context, Gpu/Device, Renderer, Gui, Input). Most of those mean "the primary window" and should keep meaning that.

## Phase 2 — per-window presentation

### `gpu::frame` IS A SINGLETON — one frame, N swapchains

**A per-window `gpu::frame` crashes the driver.** Confirmed 2026-08-23: `0xc0000005` null write inside `nvoglv64.dll` (which hosts NVIDIA's Vulkan ICD, not just OpenGL) on the first frame after a second `frame` was begun.

`frame` is not "a frame for a swapchain" — it is *the device's* frame, and it owns device-global state:

| line | shared state |
|---|---|
| `Frame.cpp:242` | `m_device->transient().begin_frame()` — one transient executor per device, advanced twice per engine frame |
| `Frame.cpp:304` | `m_device->frame_command_buffer(queue, m_current_frame)` — command buffers owned by the **device**, indexed by the frame's own counter |
| `Frame.cpp:308-309` | `cmd_reset` + `cmd_begin` on those buffers |
| `Frame.cpp:370` | `acquire_worker_command_buffer(graphics, 0, m_current_frame)` |
| `Frame.cpp:352, 372` | `transient().recorder().run_pre_frame` / `run_post_frame` |

Two `frame` objects have independent `m_current_frame` counters, so they hand out and then **reset and re-begin the same command buffers while the other is recording into them**. That is the null write.

**The plan misread the evidence.** It cited "`frame::begin/end` already take a `window::data*`" as a sign frame was per-window-ready. That parameter only says *which window to present to*; it says nothing about instancing.

**Correct shape: one `frame`, N swapchains.** This matches the hardware — one command stream per engine frame, and `vkQueuePresentKHR` already takes an *array* of swapchains. So:

- `window_presentation` holds `{ surface, swapchain, render_graph }` — **no `frame`**.
- `frame::begin` acquires an image from every registered swapchain; `frame::end` presents them all in one call.
- Per-window `frame_status` (minimized, out-of-date) becomes per-swapchain state inside the one frame, not a separate object.

Unblocked in the meantime by removing the `begin_secondary_frames` / `end_secondary_frames` calls; surface and swapchain creation per window is unaffected and still correct.

### `render_graph` IS ALSO A SINGLETON — and this retires the "graph per window" decision

Investigated 2026-08-23 after the crash. **`render_graph::execute` calls `m_device->reset_worker_command_pools(frame_idx)` (`RenderGraph.cpp:370`)**, which is device-global: `worker_command_pools::reset_frame` (`Vulkan/CommandPools.cppm:341`) walks *every* worker pool at that frame index and does `pool.reset()`, invalidating every command buffer allocated from them.

The graph then allocates its pass command buffers from those same pools (`RenderGraph.cpp:427, 672, 1324, 1442`), records into them, and stashes the handles in `m_pending_graphics_buffers` for `frame::end` to submit. So with two graphs in one engine frame:

1. graph A resets the pools, allocates, records, stashes handles
2. graph B resets the pools — **freeing every buffer A just recorded** — then allocates the same memory and records
3. `frame::end` submits A's stale handles → use-after-reset → driver null write

This is a second, independent instance of the singleton bug class, and it sits in the *graph*, not the frame. Fixing the frame alone would not have avoided it.

**The (A)-vs-(B) decision recorded above is therefore void.** "A render graph per window" — the option chosen after the four-way comparison — is not implementable. The real answer is neither of the options that were on the table, and it is *less* machinery than both.

### Correct design: one frame, one graph, N swapchains

Three pieces of existing infrastructure already point this way:

- **`present_info` already takes spans** — `.swapchains`, `.image_indices`, `.present_modes`, `.release_fences`, `.present_ids` are all `std::span` (`Swapchain.cppm:193-199`). `swap_chain::present` just passes spans of one. Multi-swapchain present needs no new ABI.
- **`color_output_info` already discriminates targets** — `is_swapchain` / `custom_target` / `transient_target` (`RenderPass.cpp:146`). "Which swapchain" is a natural extension of a distinction the type already makes.
- **The graph records and the frame submits** — the graph accumulates `m_pending_*`, `context::end_frame` drains them through `take_*`. One submit is already the model.

So the target is **not** a graph selector. It is *which swapchain image a swapchain-targeted colour attachment resolves to*:

- `render_graph` holds N swapchains instead of one; a pass targeting window W writes W's acquired image.
- `frame::begin` acquires from every registered swapchain; `frame::end` presents them in one call.
- `window_presentation` collapses to `{ surface, swapchain }` — no frame, no graph.
- `pass_builder::target(id)` stays, but it selects the attachment's swapchain rather than a graph.

**Main ripple to plan for:** `render_graph::extent()` is called in ~25 files for viewport and projection setup, and becomes ambiguous with N swapchains. Mitigation: only UI passes ever target a secondary, so `extent()` keeps meaning *the primary's* extent, and a targeted pass asks its own target for size. Confirm that before starting — it is the one place where "one graph" could leak into unrelated renderers.

### Foundation pass — DONE

Four changes, all shipped and verified against the single-window path:

1. **`reset_worker_command_pools` moved from `render_graph::execute` into `frame::begin`**, directly after the in-flight fence wait that makes it safe. The graph no longer performs frame lifecycle on device state, and the fence ordering the reset depends on is now local instead of an unenforced cross-object assumption. **This alone de-singletons the graph.**
2. **`frame` owns its present target** — `set_present_target(window_surface*)`; `begin()` / `end(...)` no longer take a window, so the signatures stop implying per-window instancing. A constructor parameter was impossible: `context::init` only has a `shared_view<window::data>`, whose `primary` is a published *view*, not the real `window_surface&`.
3. **`render_graph(device&, frame&)`** — the swapchain comes from `frame->swapchain()`, so the graph no longer advertises being "for a swapchain".
4. **Cardinality is enforced** — private `static int s_live_count` on each class (declared in the interface, defined in the impl unit so no mutable state enters the BMI), asserted in the constructor and decremented in a destructor. The messages state the reason, so the next person gets a sentence instead of a driver-side null dereference.

Not atomics: there is one creation site each (`gpu::context::init`) and one destruction site (`shutdown`), no thread overlap to protect.

### Next phase — multi-swapchain, scoped

Mechanics established by reading the code:

| fact | location |
|---|---|
| the graph writes the swapchain via `m_swapchain->image_view(image_index)` | `RenderGraph.cpp:463` (pass attachment), `:1455` (clear), `:1479` (barrier) |
| `image_index` comes from `m_frame->image_index()` — **one index for one swapchain** | `RenderGraph.cpp:346` |
| a pass targets the swapchain when `color_output_info::is_swapchain` | `RenderPass.cpp:146` |
| sync objects are sized per swapchain: `frame_sync::create(dev, sc.image_count())` | `Frame.cpp:547` |
| `frame_sync` holds `image_available`, `render_finished(image)`, `in_flight_fence(queue, slot)` | `GpuBackend/FrameSync.cppm` |
| present already accepts spans of swapchains/images/modes/fences/ids | `Swapchain.cppm:193-199` |

Work, in dependency order:

1. **Per-swapchain acquire state.** `frame` holds a list of `{ swap_chain*, window_surface*, image_index, frame_sync }` instead of one of each. `in_flight_fence` is per queue *and frame slot* — device-scoped, stays shared. `image_available` and `render_finished` are per swapchain and move into the per-target entry.
2. **`is_swapchain` becomes "which swapchain".** Replace the bool in `color_output_info` with the target id (absent = primary). `pass_builder::target(id)` sets it. `resolve_color_target` then picks that target's `image_view(its image_index)` rather than the frame's single index.
3. **Present all targets in one call.** `frame::end` builds spans across targets and issues a single `vkQueuePresentKHR`, instead of `swap_chain::present` per swapchain.
4. **Per-target failure handling.** `recreate_resources` / `recreate_surface` currently operate on `m_swapchain`/`m_window`; they become per-target. Out-of-date on one window must not tear down the others.
5. **`extent()`** stays "the primary's" (see above); a targeted pass asks its target.

Deliberately unchanged: one `frame`, one `render_graph`, one submit. Secondary windows get UI passes only, so no renderer other than `ui` ever names a target.

### 2a — make presentation surface-driven — DONE

Prerequisite for every possible version of phase 2, and independently testable: `gpu::frame` now takes `window::window_surface*` instead of `window::data*`, all the way down.

`frame` needed exactly two things from `data` that were not per-surface: `current_present_mode_index` and `attached`. Both now live on `window_surface` as `present_mode_index` / `attached`, resolved once per tick (`window::tick` copies the settings-derived values into `primary`). A secondary can therefore describe its own presentation — FIFO, never attached — without reaching into the primary's settings. Per `feedback-no-threaded-bool-params`, these went on the struct already on the call path rather than becoming extra parameters.

Retargeted, in order: `frame::begin/end/recreate_resources/recreate_surface` → `gpu::device::recreate_surface` → both backends → `vulkan::instance::create_surface`. The whole chain bottoms out at `window::raw_handle(...)` → `create_window_surface(instance, native_handle)`, so it was mechanical. New surface overloads: `minimized`, `raw_handle`, `frame_buffer_resized` (the `data&` version now forwards to `primary` — two functions consuming the same flag is a trap).

**Both backends must change together.** `gpu_dispatch` is `define_aggregate`'d from `vulkan_device_backend`'s members, so `dx12_device_backend::recreate_surface` — which is deliberately empty, since DXGI binds the HWND directly — must keep an identical signature.

Device *creation* still takes `shared_view<window::data>` and should: it happens once, at boot, for the primary.

### 2b — the shape decision: a render graph per window — SUPERSEDED, kept for the reasoning

**What shipped is one graph, N swapchains** — the "Correct design" section above, not the choice recorded here. This section survives because its *first* paragraph is still binding (never widen `recording_context`), and because the (A)-vs-(B) reasoning is what a future per-window-3D attempt will need. Its conclusion is not.

**Shape (B) as originally written is unimplementable.** `recording_context`'s constructors are private with exactly one friend, `request_pass_awaitable` (`RecordingContext.cppm:212`). The only way to obtain a `rec` is `co_await gpu::pass<...>(pass_out)`, which emits a `render_pass_request` that a **render graph** drains. There is no "UI-only pass" that bypasses the graph, and building one would mean widening `recording_context` construction — which reintroduces manual barriers, deliberately deleted (`gpu-auto-barriers-no-manual-api`). **Do not go that way.**

**The plan's original rejection of shape (A) does not apply to UI, and that is why (A) is affordable here.** The stated objection was that `m_scheduler.render()` runs every `[[= system_frame]]` system exactly once, so N windows would need the scheduler to know which window a system targets. That bites for per-window *3D*, where every renderer system would have to run per window. It does **not** bite for UI: `ui::frame` is one system that can loop internally and emit N pass requests. What is needed is a **graph target on the pass request**, not a scheduler redesign.

~~Chosen: a minimal render graph per secondary window.~~ Retired — `render_graph` is a singleton for the reason recorded two sections up. The surviving half of the idea is the one that mattered: **a target on the pass request**. It became `pass_builder::target(id window)` selecting a *swapchain*, not a graph.

### 2c — complete scope

Ordered by dependency. Each step is separately buildable; only step 4 changes on-screen behaviour.

**0. Surface ownership — BLOCKER, resolve first.**
`vulkan::instance` owns exactly one surface as `vk::raii::SurfaceKHR m_surface`, and `vulkan::device` caches a second copy in `m_surface`, kept in sync by hand via `set_surface`. Per-window surfaces cannot use either.

The *read* side is trivial — `device::m_surface` has only two consumers, both inside `create_swap_chain` (`Device.cpp:142` capabilities query, `:242` `find_queue_families`). Threading a `gpu::surface` parameter through `swap_chain::create` → `gpu::device::create_swapchain` → both backends removes the duplicated field outright, which is a guide fix in its own right (two fields holding one fact).

The *ownership* side is the actual blocker. `vk::raii::SurfaceKHR` is a single-slot RAII owner living in `vulkan::instance`, and `recreate_surface` currently does `destroy_surface() → create_surface(win) → set_surface(instance.surface())`.

**The deciding fact: `gpu::surface` is already `handle<surface_tag>` (`GpuBackend/Core.cppm:46`)** — an opaque, backend-agnostic handle. The abstraction exists already; what is missing is create/destroy through the device vtable, and an owner.

Four shapes were considered:

| option | verdict |
|---|---|
| **A — keyed map in the Vulkan backend** (`id → vk::raii::SurfaceKHR`, swapchain holds the id) | **Rejected.** Decouples surface lifetime from swapchain lifetime, which is precisely invariant #2 ("a surface must outlive every swapchain made from it"). Two id-keyed structures that must agree; ordering becomes discipline instead of structure. |
| **B — surface as an owned `gpu::surface` on the presentation aggregate** | **Chosen.** See below. |
| **C — surface owned by the OS `window_surface`** | Rejected. The window module has no gpu dependency and would need to know about the Vulkan instance. Layering violation. |
| **D — one surface, render secondaries offscreen and blit** | Not a solution. Each window still needs a swapchain, and a swapchain still needs a surface. |

**B in detail.** Add `create_surface(native_window_handle) -> gpu::expected<gpu::surface>` and `destroy_surface(gpu::surface)` to the device vtable; both backends must declare them (`gpu_dispatch` is `define_aggregate`'d from the Vulkan backend's members), with the DX12 pair a no-op returning a null handle, exactly like the existing empty `recreate_surface`. `window_presentation` then owns `{ surface, swapchain, frame, graph }` and destroys in reverse.

Why B is the *less* short-sighted choice, not merely the tidier one:
- **Invariant #2 becomes structural.** Surface and swapchain sit in one aggregate with one defined destruction order, so "surface outlives swapchain" cannot be violated by getting a removal sequence wrong somewhere else.
- **It deletes duplicated state rather than adding more.** Both `vulkan::instance::m_surface` and `vulkan::device::m_surface` — two copies of one fact synced by hand via `set_surface` — go away, replaced by an explicit parameter.
- Cost: the surface loses `vk::raii` and becomes manual create/destroy. Acceptable because the aggregate owns the ordering anyway, and the RAII slot is what forces single-ownership today.

**Boot-time exception, not duplication.** Device creation still needs *a* surface before any swapchain exists, to pick a present-capable queue family (`find_queue_families(m_physical_device, …)`). The primary's surface is created first and passed in for that selection. That is a genuine input to device creation, not a cached copy.

#### B, part 1 — DONE (uncalled scaffolding)

`create_surface(native_window_handle) -> gpu::surface` / `destroy_surface(gpu::surface)` added to `gpu::device` and both backends. Vulkan routes to new `instance::create_owned_surface` / `destroy_owned_surface`, which create and destroy *without storing* — the existing free function `vulkan::create_window_surface` already did the creation half. DX12's pair is a no-op returning a null handle, matching its deliberately-empty `recreate_surface`.

This has **no caller yet**. It is incomplete, not wrong — unlike a `pass_builder::target()` that would accept a target and silently ignore it, these two do exactly what they say. Part 2 supplies the caller.

#### B, part 2 — DONE

`gpu::surface` is now threaded through the create-swapchain chain and **`vulkan::device::m_surface` and `set_surface` are deleted** — the duplicated fact is gone. `swap_chain` owns the surface it was created with (`surface()` / `replace_surface()`), and `create` / `recreate` / `recreate_detached` all pass it down. `device::boot_surface()` exposes the boot surface for the primary's `swap_chain::create`; on Vulkan it forwards to `instance.surface()`, on DX12 it returns null.

`recreate_surface` **kept** the backend seam and changed only its return: `-> gpu::surface`. Vulkan runs its existing `wait_idle → destroy_swapchain → destroy_surface → create_surface` sequence and returns the new surface; DX12 returns `{}`. `frame::recreate_surface` installs it via `m_swapchain->replace_surface(...)`.

**DX12 is unchanged end to end**: `boot_surface()` → null, `create_swapchain` ignores the surface parameter, `recreate_surface` → null, `replace_surface(null)`. Every DX12 path sees the same values it saw before.

Two ownership models remain, each justified: `instance` keeps RAII ownership of the **boot** surface (the device needs one before any swapchain exists, for queue-family and format selection), while per-window surfaces are created and destroyed unowned through `device::create_surface` / `destroy_surface`. Those two are still **uncalled** — the first secondary window is their caller.

#### B, part 2 — original mapping (superseded by the above)

`vulkan::device::m_surface` has exactly **two** consumers, both inside `create_swap_chain` (`Device.cpp:142` capabilities, `:242` `find_queue_families`), and the device constructor already caches queue families independently (`Device.cpp:2129-2131`). So threading a `gpu::surface` parameter through the create-swapchain chain deletes **`device::m_surface`, `device::set_surface`, and `instance::m_surface` outright**.

Six signatures gain the parameter: `swap_chain::create` / `recreate` / `recreate_detached` (with the surface stored on `swap_chain`), `gpu::device::create_swapchain`, and `create_swapchain` on both backends.

**`device::recreate_surface` must be KEPT, not retired.** An earlier draft of this plan proposed hoisting its sequence to the frame level. That is wrong, and it would have broken DX12 parity.

`destroy_swapchain` exists **only on the Vulkan side** (`vulkan::device::destroy_swapchain`, `Device.cpp:432`). It is not on the `gpu::device` vtable and DX12 has no equivalent. The whole `wait_idle → destroy_swapchain → destroy_surface → create_surface → set_surface` sequence is Vulkan-internal, hidden behind `recreate_surface` — which is exactly why DX12's override is empty and why `recreate_detached`'s "caller already destroyed the swapchain" note only holds on Vulkan. DXGI needs none of it: there is no surface object, and `swapchain.create(size, mode, old_handle)` handles recreation itself.

Hoisting the sequence would force DX12 to grow a `destroy_swapchain` it does not need and would change its recreate path. Instead, **keep the backend seam and change what it operates on**:

```
device::recreate_surface(native_window_handle handle,
                         gpu::swap_chain_handle current,
                         gpu::surface old) -> gpu::surface
```

The Vulkan implementation runs its existing sequence but creates and destroys an *unowned* surface and returns the new one. DX12 stays empty and returns `{}`. `frame::recreate_surface` then does `replace_surface(device->recreate_surface(...))` followed by `recreate_detached`.

Consequences, and why this is the parity-safe shape:
- **DX12 behaviour is bit-identical to today.** Null surface in, null surface out, `recreate_detached` runs with no prior destroy exactly as it does now.
- Invariants #2 (surface outlives every swapchain made from it) and #4 (swapchain destruction immediate, not deferred) stay *inside* the backend function that already implements them, rather than being re-derived at a new call site.
- `swap_chain` carries a `gpu::surface` that is simply null on DX12 — the same way `create_surface` already returns a null handle there.

DX12 needs no equivalent — `dx12_device_backend::recreate_surface` is deliberately empty because DXGI binds the HWND directly.

**Steps 1 and 2 below are not separable.** A `target` on the pass request with no per-window graphs to route into is an API that accepts a target and silently renders to the primary — a lying signature, and machinery with no caller. Implement them as one change.

**1. Target the pass request.**
`render_pass_descriptor` gains `std::uint32_t target = 0` (0 = primary). `pass_builder` gains `.target(n)`. `context::execute_frame` currently drains the whole `render_pass_request` channel into `d.render_graph` (`RenderPass.cpp:189`); it instead drains **once** and partitions by `target`, executing each graph with its own slice. Draining once is required — `render_pass_request` is `[[= same_frame_channel]]`.
`drain_images` / `drain_buffers` stay whole-channel to the primary: transient resources are a 3D concern and a UI pass requests none. Verify that claim before relying on it.

**2. Per-window presentation objects.**
A `window_presentation { unique_ptr<swap_chain>, unique_ptr<frame>, unique_ptr<render_graph> }`, held in `std::vector<std::unique_ptr<window_presentation>> secondaries` on `gpu::context::data`.

**It must NOT be `[[= stable_shared]]`, and it must not be `[[= shared]]` at all.** The contract is a literal assert — `assert(out.sm == nullptr || out.sm == d.m.get(), "stable shared field '{}' was reseated")` (`SharedView.cppm:308`) — so a stable pointer may be set once from null and never again. Runtime create/destroy of windows reseats by definition. `[[= shared]]` also does not publish through a collection (0a′). So secondaries are engine-private, exactly like `gui::data::secondaries`. `primary` keeps all three `stable_shared` fields untouched, so **all 25 files that read `gpu_s.render_graph` / `gpu_s.swapchain` keep working unchanged** — none of them should ever see a secondary.

`unique_ptr` elements, not values: `render_graph` holds `device*`/`swap_chain*`/`frame&` and a `transient_pool`, so its address must be stable (same reason as `viewport_state`).

**3. Creation and destruction.**
Creation is main-thread, at a frame boundary, mirroring `swap_chain::create` → `frame` → `render_graph(device, swapchain, frame)` for the primary.

Destruction must honour the recorded invariants:
- **`gpu::context::shutdown` stays the only `[[= system_shutdown]]` touching GPU state** (`scheduler-shutdown-order-bindless`). Secondary presentation objects are destroyed there, never by the window system. The window system may destroy the `GLFWwindow` only after the GPU objects are gone.
- Per-window destroy order must match the primary's, which is `render_graph.reset() → frame.reset() → swapchain.reset()` (`Context.cpp:70-73`), and **all** secondaries must be torn down before `device.reset()`.
- Anything owning a `bindless_handle` must die before the device. A secondary's graph owns a transient pool, so it counts.

**4. Route UI to windows.**
`ui::frame` loops viewports, emitting one pass per target, using 0d's `sprite_command::viewport` tag to partition batches. Its `.after<^^forward::frame, ^^tonemap::frame, …>` ordering applies only to the primary — a secondary graph has no such passes, so the dep list must be conditional or those ids simply absent from that graph. **Confirm the graph tolerates an `after` dep on a pass that does not exist in it**; if it does not, that is a real change to dep resolution, not a call-site tweak.
`d.gpu_frames[frame_index]` (UI vertex/index buffers) are indexed by the *primary* graph's `current_frame()`. Per-window graphs have independent frame indices, so these buffers must become per-target or the two windows will stomp each other's vertex data. **This is the most likely source of a subtle first bug.**

**5. Per-window surface-lost recovery.**
All five invariants in `gpu-surface-lost-recovery` now hold per window: surface rebuilt not just swapchain; `wait_idle → destroy swapchain → destroy surface → create surface → repoint → create swapchain`; null `oldSwapchain` on rebuild; immediate (not deferred) swapchain destruction; and **present must never be fatal**. 2a already made `frame` surface-driven, so `recreate_surface(const window_surface&)` works per window unchanged.
Open question: `vulkan::instance` holds a single `m_surface` and `device_config.set_surface(...)` is global. Per-window surfaces mean the instance can no longer cache one. **This is unscoped and may be the largest single item in phase 2** — resolve it before starting step 5.

**6. Input coordinate reconciliation** (deferred from phase 1). A secondary's cursor is client-relative while `vp.frame_rect` is in the primary's space. Once a viewport owns a window, `frame_rect` becomes that window's client rect and the two agree — which is why deferring was correct rather than translating twice.

### Invariants this must not break

| invariant | source | how it is preserved |
|---|---|---|
| stable shared pointers are never reseated | `SharedView.cppm:308` assert | secondaries are not shared at all; `primary` untouched |
| `[[= shared]]` does not publish through a collection | 0a′ | secondaries are engine-private |
| `gpu::context::shutdown` is the only GPU `system_shutdown` | `scheduler-shutdown-order-bindless` | secondaries destroyed there |
| bindless owners die before the device | same | all secondaries torn down before `device.reset()` |
| no manual barrier API | `gpu-auto-barriers-no-manual-api` | every window records through a graph |
| present is never fatal | `gpu-surface-lost-recovery` #5 | unchanged; already surface-driven after 2a |
| viewports created/destroyed only at frame boundaries | Standing hazards | window + presentation created in the same boundary drain |
| one cursor, one clipboard, screens are primary-only | 0b, 0d | unchanged |
Swapchain + frame per window; the device stays shared. `frame::begin/end` already take a `window::data*`, and `PresentPacer` is already per-swapchain.

- **`[[= stable_shared]]` breaks.** `device`, `swapchain`, `frame`, `render_graph` are all `stable_shared unique_ptr` on `gpu::context::data`. That annotation is a write-once, address-stable contract and the infrastructure *asserts* if a stable pointer is reseated after publication. Per-window swapchain/frame cannot live under it.
- `gpu::context::shutdown` must remain the only `system_shutdown` touching GPU state; per-window swapchains are destroyed there, not by the window system.
- Surface-lost recovery has five documented destroy-order invariants that must now hold per window.
- `render_graph` holds `swap_chain* m_swapchain` and `extent()` derives from it.

## Phase 3 — editor pop-out

### Shipped

Tear a tab past `tear_threshold` and release it outside any dock target → the panel leaves the tree and becomes its own OS window. Close the window → the menu returns to the primary and the editor re-inserts the panel.

The chain, and why each hop exists:

| step | mechanism |
|---|---|
| editor detects torn release with no drop | `update_dock_interaction` sets `data::popout` — it cannot push channels itself, it runs inside a deferred `settings::change_request` apply lambda |
| `editor_app::run` emits `window_popout_request` | carries the cursor in **client** space; the editor cannot compute screen coords because `window_surface::position` is not `[[= shared]]` |
| `window::tick` creates the window | converts client → screen (including the Y-flip), and **echoes `for_menu`** on `window_opened` — the requester cannot name the window because ids are minted inside `tick` |
| gui creates the viewport and migrates the menu | one atomic step on `window_opened`, so the menu is never homeless |
| editor records `window id → panel` | from the same echo, so `window_closed` can re-insert |

Two traps closed while building it:

- **`sync_dock_menus` would have eaten the panel.** It deletes menus for registered panels that are not live hosts. The editor removes the panel at release, but gui only migrates a frame later on `window_opened` — the menu was destroyed in between, so the panel vanished rather than popping out. `sync_dock_menus` now skips panels in `pending_popouts`, which is order-independent rather than depending on which system runs first.
- **The cursor was warped back inside the window.** `glfwSetCursorPosCallback` clamped the position *and* called `glfwSetCursorPos` to physically return the OS cursor to the client area whenever `ui_focus` was set. Dragging out was impossible at the hardware level. The clamp now yields while the left button is held: the OS keeps delivering out-of-bounds moves to the capturing window, and the engine was discarding them.

Chrome metrics moved from `window::data` to `window_surface` (they are per-window by nature), `native_frame_state` carries the surface, and secondaries are created undecorated with the native frame installed. `find_surface` treats a non-existent id as the primary, matching the "absent = primary" convention used by `viewport_for_window` and `color_output_info::window` — without that, the screen caption's metrics push (which names no window) was silently dropped and the primary's native frame stopped working.

### A popped-out window is movable and has caption controls — DONE

The diagnosis was: `sync_dock_menus` sets `bare = (leaf has exactly one panel)`, a popped-out panel was a solo leaf, so it kept `bare = true`; `menu_chrome_height` returns `style::bare_header_height` for a bare menu — the 6dp accent strip, not a 32dp title bar. gui reported that as `chrome_caption_height`, so `WM_NCHITTEST` answered `HTCAPTION` over six pixels. It is also not draggable *inside* gui — panels are `fixed = true` so `hot_item` skips them, and the editor's dock code only runs against the primary viewport — so `HTCAPTION` is the only way the window moves.

Fixed where the window-bound override already lived. `begin_viewport_frame` already forced `m.rect = vp.rect` on every top-level menu of a window-bound viewport; it now also forces `m.bare = false` in the same loop. One line, and it is the same authority that already decides "this menu fills its window", rather than a second rule somewhere else. The menu returns to the primary with `bare` stale, which is harmless: `sync_dock_menus` recomputes it every frame from the leaf's panel count.

**A second, independent misalignment had to go with it, or the fix would have looked like it did nothing.** `vp.rect` came from `usable_screen_rect(d.reserve_top_bar ? title_bar_height : 0, vp.frame_rect)` for *every* viewport. `reserve_top_bar` is pinned from `engine_config::custom_chrome` and exists for the **screen caption** — and screens only ever land on `d.primary.menu_stack` (`push_screen_request` names `d.primary` explicitly). So a secondary was reserving a strip for a caption it can never draw, putting its menu title bar at `[title_bar_height, 2 × title_bar_height)` while `chrome_caption_height` told `WM_NCHITTEST` the grab band was `[0, title_bar_height)`. The two never overlapped; the window was unmovable by construction and would have stayed that way at 32dp. The inset is now gated on `!vp.window.exists()`.

**Caption controls follow the popout-close button, not the screen caption.** `draw_screen_caption` uses `builder`/`caption_button`, but `draw_menu_chrome` has no builder — it pushes sprite commands and calls `interaction::press_from` directly. The existing per-menu close button (`is_popout_menu_tag` branch) is the local analogue, so `draw_window_caption_buttons` copies its shape: raw sprites, `press_from`, and a *deferred* result. Minimize and close set `viewport_state::pending_window_minimize` / `pending_window_close`, and `update_viewport` — which owns `ui_out` — turns them into requests. Chrome cannot push channels, exactly like `pending_popout_close_ids`.

`window_close_request` and `window_minimize_request` gained an `id window`, and `window::tick` routes them through `find_surface`: an absent id resolves to the primary and keeps the existing `d.cmd_close` / `d.cmd_minimize` path, a secondary gets `glfwSetWindowShouldClose` / `glfwIconifyWindow` directly, and an id naming **no** surface is dropped — a stale id from a window that closed this frame must not fall through to closing the app — legal because `tick` is main-thread. Close deliberately goes through `SetWindowShouldClose` rather than destroying the window inline, so it lands in the existing `close_requested` sweep that already emits `window_closed` and re-inserts the panel. **`window_toggle_maximize_request` was deliberately left alone** — there is no maximize button on a popped-out panel, and adding a window field with no caller would be machinery without one; it also keeps the window clear of `window-restore-rect-from-placement`.

Both halves of the hit-test escape are now pushed, not just the caption height: `controls_width` (right-anchored, which is exactly how `WM_NCHITTEST` reads it) so the buttons answer `HTCLIENT` instead of `HTCAPTION`, and `resize_exclude_y0/y1 = [0, caption)` so the right-hand resize border does not eat the close button's outer pixels. The corner checks run before the `right` check, so top-right resize still works; only the strip below the corner inside the caption gives up right-edge resize.

#### Three defects the first cut exposed — the window was mirrored, not merely unstyled

Shipping the caption made a much older problem visible: the popped-out window was **completely uninteractable**, and the new buttons vanished on the first click. Three independent causes, none of them in the caption code.

1. **`window_surface::ui_focus` is the coordinate-space switch, not just a capture flag.** `glfwSetCursorPosCallback` pushes `mouse_moved{ x, dims.y() - ypos }` when `ui_focus` is set and raw `{ x, ypos }` when it is not. gui is y-up. `set_ui_focus` only ever writes `d.primary.ui_focus`, so every secondary defaulted to `false` and its entire UI was **vertically mirrored** — the title bar answered at the bottom edge. `create_secondary` now sets `ui_focus = true`, which is the honest value: a popped-out panel window is pure UI and has no camera to capture the cursor. Flipping unconditionally in the callback instead would have been wrong — the `false` branch is the gameplay path and inverting it flips vertical look in the Sandbox camera.
2. **The `WM_NCMOUSEMOVE` bridge was hardcoded to the primary.** It read `d.primary.handle` for the flip height and pushed into `d.primary.input_events`, regardless of which HWND the message came from — so a secondary's caption hover was dropped, and `native_frame_state::owner` existed only to reach `d`. It now uses `state->surface` for both, and `owner` and `install_native_frame`'s `data&` parameter are gone with it.
3. **Absolute `z_order` on caption buttons loses to a menu's climbing z.** `process_menu` bumps `current_menu.z_order = vp.next_z_order++` on every press inside the menu, and `stamp_z` then paints all `z_order == 0` chrome with that value. The buttons carried hard-coded `1` and `2`, so from the second click on, the title-bar sprite drew *over* them. Fixed by leaving the button sprites at `0` like every other sprite in `draw_menu_chrome` — the stamp gives them the menu's z, and ties break by insertion order (`stable_sort`, same `blank_texture`), which puts them after the title bar. **The pre-existing `is_popout_menu_tag` close button had the identical bug** and is fixed the same way; do not reintroduce a literal `z_order` in menu chrome.

### Popped-out windows resize and maximize — DONE

**Root cause was the decoration hint, not the hit test.** GLFW's Win32 backend only adds `WS_MAXIMIZEBOX | WS_THICKFRAME` when `GLFW_DECORATED` is true; an undecorated window gets `WS_POPUP` and nothing else. Windows will not start a resize loop on a window without `WS_THICKFRAME` **no matter what `WM_NCHITTEST` returns**, so every `ht_left`/`ht_bottom_right` the native frame proc was answering for a secondary was inert, and maximize was impossible.

The primary never had this problem, and the reason is the whole technique: it is created **decorated** and `native_frame_proc` returns 0 from `WM_NCCALCSIZE` to strip the non-client *area* while the window keeps its styles. Secondaries now do the same — `decorated = true` plus the native frame — so OS resize, aero-snap, and maximize all come back for free. That also means the client rect equals the window rect, so placement had to move off `glfwSetWindowPos` (which inflates by a phantom frame, `window-native-frame-geometry-drift`) onto a new `set_surface_frame_rect`, the per-surface twin of `set_window_frame_rect`, called *after* `install_native_frame` so the requested size is the size you get.

The caption grew a maximize/restore button between minimize and close (`controls_width` is now `title_bar_height * 4.5`), and **`window_toggle_maximize_request` got the `id window` it was denied earlier** — it now has a caller, which is the whole reason it was left alone before. A secondary toggles via `glfwGetWindowAttrib(maximized)` → `glfwRestoreWindow` / `glfwMaximizeWindow`; the primary keeps its `d.cmd_toggle_maximize` path. Safe against `window-maximize-hidden-jumps-to-primary` because a popped-out window is always shown before any button exists to press.

### Re-docking a popped-out panel — DONE

Closing the window already re-inserted the panel, but always at `location::center` and with no way to aim. The panel now drags back.

**The blocker is that `HTCAPTION` never delivers a mouse press.** Windows starts its own modal move loop on `WM_NCLBUTTONDOWN`, GLFW sees nothing, and the render loop stalls inside `glfwPollEvents` for the duration — so a drag started on the caption can neither be observed nor previewed. Making the whole caption `HTCLIENT` and moving the window from gui would fix that, but it would throw away the OS move, snap and double-click-maximize that the resize work just restored.

**Chosen: a tab-shaped grip, symmetric with `controls_width`.** `window_surface` gained `chrome_grip_width`, a *left*-anchored `HTCLIENT` carve-out inside the caption band, sized to the panel's name (`window_caption_grip_rect`) and drawn as a tab. Press it and gui owns the gesture; the rest of the title bar still moves the window the OS way. This is the same shape the right-hand controls carve-out already had, so `WM_NCHITTEST` grew one clause, not a new concept.

The drag then crosses the window boundary on its own: because the press lands in the client area, Win32 captures the mouse to the secondary, so moves keep arriving with out-of-bounds coordinates once the cursor leaves — the same mechanism that made tear-*out* work.

**Coordinate conversion happens in `window::tick`, and it has to.** gui cannot do it: `window_surface::position` is not `[[= shared]]`, and `[[= shared]]` does not publish through a collection, so a secondary's position is invisible outside the window system. So gui pushes `window_panel_drag_request { window, client_cursor, released }` in its own client space, and `tick` — which holds both positions — converts source-client → screen → primary-client (with both Y-flips) and re-emits `window_panel_drag_over { window, primary_cursor, over_primary, released }`. This is the seam `window_popout_request` → `window_opened` already established for exactly this reason; it is not a new pattern.

The editor consumes that into `dock_input::external`, and `update_dock_interaction` runs the **existing** `drop_target` against the primary tree, so the dock cross and the ghost are the same ones a local drag draws. On release over a target it records `d.redock` / `d.redock_window` and pushes `window_close_request` — the already-shipped `window_closed` handler then inserts at the recorded spec instead of centre. **Re-docking is therefore still one insertion path, not two**; the drag only chooses *where*. The close request repeats each frame while `d.redock` is pending, which is idempotent (`glfwSetWindowShouldClose`) and self-healing if a request is ever dropped.

Deliberately not done: the popped-out window does not follow the cursor while its tab is dragged. The ghost in the primary is the feedback, matching how a tab dragged out of a floating window behaves elsewhere; moving the window too would need a `window_move_request` with no other caller.

#### The dock hint did not light up — `dock::space` now owns its own selection

Reported symptom: dragging a panel back into the primary drew the dock cross but never highlighted a target. The cause is the guide's **paired-derivations** defect, and it had been latent in three places at once.

`drop_target` decides which area the drop lands on, and *also* stored that as `dock_drop::location`. `draw_dock_space` then independently re-derived the same fact by hit-testing `space.areas` against a `vec2f mouse` parameter. gui's own floating-menu release path in `DragResize.cpp` re-derived it a **third** time. Three copies, all correct as long as they were handed the same point.

A cross-window drag is the case that breaks that: `update_viewport` passed `input_st.mouse_position()`, which during the drag is in the **secondary's** client space, while `space.areas` are in the primary's. No area ever matched, so nothing highlighted — and nothing failed, because each copy was individually doing exactly what it said.

Fixed by moving the fact into the type. `dock::space` gained `location hot` and `select(point)`; `draw_dock_space` **lost its `mouse` parameter entirely**, so the mismatch is now unrepresentable rather than merely fixed. `dock_drop::location` is deleted in favour of `drop.space.hot`, and the `DragResize` release path reads `->hot` instead of re-testing. Three derivations became one, and the two consumers that could disagree no longer have anything to disagree about.

#### Review findings applied to the same pass

- **`window_caption_controls_width(sty)` was the same defect one layer over**: a literal `title_bar_height * 4.5` beside a layout that placed three `1.5`-wide slots. A fourth button would have left the extra button outside the `HTCLIENT` carve-out and therefore unclickable — the exact failure the caption work started from. It is now `window_caption_buttons::extent()`, spanning min-left to max-right of the buttons actually produced.
- **The grip could start a phantom drag from the primary.** `update_viewport` runs for every viewport each frame, so a click in the primary whose coordinates happened to land inside the secondary's grip rect began a drag in the secondary. Gated on `vp.owns_cursor`, which is the existing authority for "this viewport's coordinates are meaningful".
- **Three `pending_window_*` bools encoded one mutually-exclusive action.** One cursor means at most one caption button activates per frame, so the three-bool form permitted states the system cannot produce. Collapsed to `std::optional<caption_action>` drained through one `switch`.
- **`d.redock` and `d.redock_window` were two fields that had to agree.** Merged into `dock_redock { window, where }`.
- **Dead guards removed** from the `window_panel_drag_request` drain: `!source->handle` and `!d.primary.handle` are unreachable — `create_secondary` never registers a surface without a handle and `destroy_secondary` erases the entry rather than nulling it, and `tick` cannot run before `create_window`. `!source` stays: a window can close between the push and the drain.
- **An abandoned drag leaked the overlay.** Closing the popped-out window mid-drag means no `released` message ever arrives, leaving the primary's dock cross and ghost drawn forever. `close_window_viewport` now clears both — gui owns that overlay state, so gui is where the window's disappearance has to cancel it.

#### `over_primary` matches the OS z-order

The first cut tested client-rect containment, so a drop registered wherever the cursor was *geometrically* over the primary — through any window stacked on top of it, including the popped-out window doing the dragging. The hint appeared for a target the user could not see.

`surface_topmost_at(surface, screen_point)` replaces it: on Windows, `WindowFromPoint` → `GetAncestor(GA_ROOT)` → compare against the surface's HWND, which is a pure z-order and geometry query and is unaffected by the mouse capture the drag holds. Elsewhere it falls back to rect containment, which is what the old code did everywhere. With the native frame the client rect equals the window rect, so the z-order test subsumes containment rather than sitting beside it — there is still one predicate, not two.

**Consequence, accepted deliberately:** the popped-out window does not move while its tab is dragged, so it genuinely occludes whatever is under it, and a **maximized** popped-out window covers the primary entirely and cannot be dropped back. That is the correct reading of the rule — the hint now appears exactly when the target is visible — and the escape hatches are one click each: restore the window with its own caption button and drag, or close it and take the centre insert. The alternative, excluding the source window from the test, would let the user drop onto a dock cross drawn *behind* an opaque window, which is a blind drop and strictly worse.

### The drag ghost says what release will do — DONE

`drag_ghost` gained `bool detaching`, set in the same branch that computes `d.drop`. It is **not** simply `!d.drop` — the release branch detaches only when `!d.drag->group.exists() && panel_count(d.tree) > 1`, so the ghost repeats that predicate verbatim. A group drag or the last remaining panel released in open space snaps back, and the ghost must not promise a window that will not appear.

`draw_drag_ghost` renders the distinction rather than a second overlay: detaching draws an accent outline, a `symbol::maximize()` glyph (a window outline) in a leading gutter, and the menu-body fill; docking is unchanged, because the dock cross is already carrying that half of the feedback.

## Standing hazards

- **Address stability — now enforced by the type.** `secondaries` is `std::vector<std::unique_ptr<viewport_state>>`, so a viewport's address is stable across insertion and removal and `viewport_at` can hand out a raw `viewport_state*` safely.

  This was not a design choice up front; `std::vector<viewport_state>` failed to compile. `viewport_state` is **not movable**: it holds an `input_layer`, which holds an `n_buffer`, which holds `std::atomic<std::uint64_t>`. The deleted move constructor surfaced from `stl_construct.h` during vector growth. `unique_ptr` elements were the right answer rather than making `n_buffer` movable — the atomics are load-bearing, and the indirection retires the hazard this entry used to describe (a held `viewport_state&` being invalidated by adding a viewport).

  **Frame-boundary creation is still required**, for the other half of the original reason: `current_menu` is a raw `menu*` into `vp.menus`, and destroying a viewport mid-draw dangles it. The layout/migrate drains at the top of `run` are what satisfy this. Note `data` itself has been non-movable since 0a for the same reason (it holds `viewport_state primary` by value); `[[= shared]]` never copies it because `publish_kind::nested` recurses only into the `[[= shared]]` members, never `input_layer`.
- **`[[= shared]]` does not publish through a collection** (see 0a′). If a secondary viewport ever needs external publication, that is a real feature, not a one-liner.
- **Verify renames by enumeration, not by eye.** This refactor moved ~300 call sites. The check that works is: list every `d.X` surviving in a file and confirm it is exactly the settings/shared set. A regex matching `d` followed by `,` or `)` misses `[&d]` capture lists — that exact hole shipped one build failure.
- **Do not pre-empt recorded toolchain bugs.** Write the natural form, build, and only fall back if it reproduces (`feedback-retest-toolchain-bugs-before-working-around`). New module partitions *are* still authored split (interface + `:x_impl`) — that one was reproduced this session, not merely recorded.

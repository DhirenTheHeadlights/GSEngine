# Editor panel streamline

Consolidate the editor's three hand-rolled side panels (explorer, terminal tab-list,
system-graph node detail) and the top-level frame onto a small set of reusable,
**non-docking** GUI primitives. Panels stay fixed/non-undockable — docking is explicitly
out of scope (it *is* the undock machinery we don't want).

## Why

Each panel independently re-implements: rect derivation, background + accent chrome,
scroll, and content layout — in three inconsistent ways. The frame drives `menu->rect`
by hand with a bespoke divider drag. The GUI already proves the divider + scroll patterns
work (in the docking path and `scroll_region`); we extract them in a content-level,
non-docking form and route everyone through them.

Baseline (this worktree):
- Frame layout + bespoke divider drag: `Editor/Editor/Source/App/EditorApp.cppm:2886-2970`
- Explorer chrome + rect: `EditorApp.cppm:907-943`
- Terminal tab-list manual scroll: `Editor/Editor/Source/Terminal/Terminal.cppm:696-833`
- Graph node detail panel: `Editor/Editor/Source/SystemGraph/SystemGraph.cpp:440-582` (+ scroll at 171-183)
- Reusable pieces already there: `LayoutOps.cppm` (`split_horizontal/vertical`, `size_spec`,
  `within`), `Widgets/Scroll.cppm` (`scroll_region`, RAII clip+offset+scrollbar+wheel),
  `handle_resizing_divider_state` in `Gui.cpp` (the docked-divider drag to mirror).

## Slice A — panel chrome helper (low risk, no behavior change)

New reusable helper in the GUI draw layer:

```
gse::gui::draw::panel_backdrop(ctx, {
    .rect        = std::optional<rectf>{},   // default: clip_stack.back()
    .bg          = vec4f,
    .accent      = std::optional<{ side, width, color }>{},
}) -> rectf   // returns the padded content rect (body.inset(padding))
```

- Emits bg-fill sprite + optional accent bar (replaces the duplicated sprite emits).
- Caller passes its own palette (colors stay editor-owned; the mechanism is generic).
- Convert: explorer (`EditorApp.cppm:920-933`), terminal strip bg (`Terminal.cppm:720-724`),
  graph detail bg+accent (`SystemGraph.cpp:460-473`). Graph panel passes an explicit rect
  (it floats over the graph area, not off `clip_stack`).

Ship independently. Pure de-duplication, zero behavior change.

## Slice B — content-level splitter primitive (biggest lift, highest value)

New primitive; mirror `handle_resizing_divider_state` minus the menu/dock coupling.

```
gse::gui::splitter(ctx, {
    .id, .orientation,           // vertical | horizontal
    .ratio,                      // float& — in/out, caller owns persistence
    .min_a, .min_b, .gap,
}) -> std::array<rectf, 2>       // caller within()s each half
```

- Resolves the two rects from the current rect via `split_*` using `ratio`.
- Registers a hit region on the divider, drives drag through the hot/active id helpers,
  writes the clamped ratio back into `ratio`. Calls `register_resize_block` on the divider
  so it doesn't fight window-edge resize.
- **By-reference ratio** (not an owned store): the editor already persists `explorer_ratio`
  / `terminal_ratio` to its layout file — keep it owning that; avoids a new global state map.
- Convert the frame (`EditorApp.cppm:2886-2970`): two nested calls —
  `split vertical(main-area, terminal)` then `split horizontal(explorer, code)` — feeding
  `explorer_ratio` / `terminal_ratio`. Deletes the manual hit-testing + `resizing_explorer`
  / `resizing_terminal` latches. Panels still get their rects written to `menu->rect`
  (fixed/bare/undocked) exactly as now — only the divider math is replaced.

Do the frame conversion first (best-tested path), then the primitive is available for any
future 2-pane split.

## Slice C — route hand-rolled scrollers onto `scroll_region`

- **Graph node detail** — DONE. `draw_detail_panel` now takes `builder&`; wraps content in
  `scoped_layer(overlay)` + `layout::within(panel)` + `scroll_region` (per-node scroll id
  `graph_detail::{node}`). `queue_*` raise to `current_layer` and auto-clip to `clip_stack`
  for layers ≤ popup (overlay=2 ≤ popup=3), so all per-command `.layer`/`.clip_rect` dropped.
  Manual `y`-cursor → `reserve_row`/`skip`. Deleted `graph_data.detail_scroll`, the wheel
  feed in `draw_graph`, the two selection-change resets, and the post-clamp.
- **Terminal tab-list** — folded into the tab unification below (it's a tab strip, not a
  plain scroller; its scroll is entangled with active-accent + close/＋ + auto-reveal).

## Tab unification (decided: full scope, caller-owns-close)

Three hand-rolled tab systems today, each re-copying `tab_slot {id,caption,rect,close_rect}`,
`scroll_axis` + auto-scroll-to-active, wrap/scrollbar overflow, ellipsis, active accent:
- Engine **menu tabs** (`Gui.cpp:draw_tab_bar` 1226; welded to docking — content-less string tag).
- **Terminal** vertical strip (`Terminal.cppm:696`; owns `instance`s, kill-confirm close, `+`).
- Code **doc tabs** (`EditorApp.cppm` ~1690-2040 + `Workspace.cppm`; id-keyed, drag-reorder,
  dirty `*`, diagnostics spinner, pinned uncloseable Game tab).

**New `tab_strip` widget** (`Widgets/TabStrip.cppm`), the substrate owned once:
- Input: orientation + span of `{ id, caption, dirty, busy, closeable, pinned }` + active id + config.
- Emits (immediate result): `activated`, `close_requested`, `reordered`, `add_requested`, per-tab rects.
- Owns: slot layout, ellipsis, active accent, **close-glyph ↔ busy-spinner swap**, dirty marker,
  overflow (scroll + auto-reveal + optional wrap), drag-reorder, hit-test; state in
  `tab_strip_state` keyed by strip id. `busy`→spinner + `closeable`→× become universal.
- **Close model:** widget only emits `close_requested`; callers keep confirm/undock (terminal
  kill-confirm, menu undock) caller-side.

Phases:
- **Phase 1** — DONE. Widget built; terminal + doc tabs migrated. Terminal gained the spinner.
  `tab_strip_state` lives in `:types` (embedded in `menu`, so it can't live in `:tab_strip` —
  circular). `tab_strip` takes `const draw_context&` (only calls const methods). Widget owns a
  pure `tab_strip_layout(font, style, …)` + `tab_strip_measure(font, style, …)` (no draw_context),
  so geometry is single-sourced.
- **Phase 2** — DONE. Menu tab bar folded onto `tab_strip`:
  - `draw_tab_bar` bridges a `draw_context` over `d`'s raw command buffers (mirrors the
    `begin_menu` ctor) and calls `tab_strip`; maps `activated`→`active_tab_index`,
    `close_requested`→`pending_tab_close`.
  - The docking **detach-arm** (input pass, `pending_drag.tab_index`) calls the *same*
    `tab_strip_layout` to hit-test the grabbed tab — no geometry mismatch. Detach/merge machinery
    unchanged (docking is now a consumer of the shared layout).
  - `tab_chrome_height` uses `tab_strip_measure`; deleted `tab_layout`, `tab_slot`,
    `tab_row_height`, `update_tab_bar_scroll`; menu's `tab_visible_rows`/`tab_scroll_x`/
    `tab_scroll_active_index` collapsed into `menu.tab_bar` (`tab_strip_state`). `Save.cppm`
    persists `menu.tab_bar.visible_rows` (on-disk key unchanged).
  - **Watch on run** (couldn't test here): menu-tab detach-into-floating-window + drop-to-merge,
    and multi-row wrap height (row model changed from `title_bar_height-4` to `line_height+pad`).

## Out of scope
- Dockable/undockable panels (fights the fixed frame; not wanted).
- Code text-area + explorer tree — already on the good primitives (`text_area_state`, `tree`).

## Slices are independent
A / B / C each compile and ship on their own. Suggested order: A → B → C.

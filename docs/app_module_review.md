# App Module Review (`gse.ide.app`)

Second review in the editor module series (after `gse.ide.analysis`), conducted per `docs/CODE_REVIEW_GUIDE.md` against the working tree of the `editor` branch on 2026-07-20.

Scope:

- `Editor/Editor/Import/App.cppm` (4 lines)
- `Editor/Editor/Source/App/EditorApp.cppm` (3229 lines)
- `Editor/Editor/Source/App/SearchScreen.cppm` (304 lines)

Method: static inspection only, per the guide's verification rules. **Nothing in this review was compiled or run.** The working tree contains uncommitted changes to this module (hover-panel horizontal scroll, window resize-exclusion bands) that have not been built; findings in that code are flagged.

Facts were verified against the consumed APIs rather than assumed: `workspace::data`/`document`/`hover_state`/`fs_node` (Workspace.cppm), `workspace::open_file`/`close_document` (Workspace.cppm:341, 466), `search_system::data` (SearchSystem.cppm:14), the `index_merge_request` consumer (SearchSystem.cppm:118), `query_buffer` (Search/Engine.cppm:12), `search::options` (Search/Types.cppm:67), `gse::gui::menu_stack_state::tick` (Engine MenuStack.cppm:274), `tree_selection` (Engine Tree.cppm:33), and `gse::layout_store` exports (Engine LayoutStore.cppm:83).

---

## Major

### 1. Out-of-bounds read in the quick-search enter path when async results shrink

`Editor/Editor/Source/App/EditorApp.cppm:723` (arrival), `:742` (enter)

- **Impact.** Pressing Enter in the explorer quick-search can index past the end of `state.results` — undefined behavior (crash or a jump to garbage). Trigger: type a query, press Down/Up while the previous (larger) result list is still displayed during the 120 ms debounce + query latency window, then press Enter after the smaller result set lands.
- **Mechanism.** When a completed query lands, `state.results = std::move(state.pending->results)` replaces the vector without touching `state.selected` (lines 723-729). `selected` was clamped against the *old* list by the arrow-key handlers (737-740) and the hover loop (775). The Enter path then does `state.results[static_cast<std::size_t>(idx)]` (744) with the stale index. The modal twin already handles this correctly — `SearchScreen.cppm:248` resets `m_selected = 0` on arrival — so the invariant is known but exists in only one of the two copies.
- **Immediate repair.** Reset (or clamp) `state.selected` in the arrival block at line 723, matching `SearchScreen.cppm:246-250`.
- **Prevention.** Recurring class, and the guardrail is Finding 4: one shared query driver owning the `{pending, results, selected}` triple so "results swapped ⇒ selection revalidated" is written once. A selection index into an asynchronously replaced list is exactly the "two fields that can disagree" state the guide flags.

### 2. Ghost results reappear after accepting a quick-search entry

`Editor/Editor/Source/App/EditorApp.cppm:746` (enter accept), `:793` (click accept)

- **Impact.** After accepting a result while a newer query is still in flight, the in-flight results later repopulate `state.results` even though the query box is empty. Refocusing the search box then shows a dropdown of stale rows under an empty query, and clicking one jumps (the click path at 793 has no `!query.empty()` guard).
- **Mechanism.** Both accept paths clear `query`, `last_query`, and `results` but neither cancels nor resets `state.pending`. The arrival block at 723 runs unconditionally on a later frame and reloads `results`. The gate at 731 only suppresses drawing while the widget is unfocused.
- **Immediate repair.** In both accept paths: `if (state.pending) { state.pending->cancelled.store(true, std::memory_order_release); state.pending.reset(); }` and reset `selected`/`dirty`.
- **Prevention.** Same class and same guardrail as Finding 1 — the accept/cancel lifecycle belongs to the shared driver (Finding 4). The modal avoids this only because dismissal destroys the whole screen.

### 3. Explorer multi-selection re-opens documents every frame and fights the user for the active tab

`Editor/Editor/Source/App/EditorApp.cppm:1082-1093`, with `workspace::open_file` (Workspace.cppm:341-351) and `tree_selection` (Engine Tree.cppm:33)

- **Impact.** With two or more files selected in the explorer, every frame calls `workspace::open_file` for each selected key. `open_file` runs `std::filesystem::weakly_canonical` (a filesystem syscall) and a linear scan of `documents` per call, per frame, indefinitely — and unconditionally sets `active_document_id` (Workspace.cppm:348). Concrete user-visible failure: select two files, then click any other tab — the next frame's loop snaps the active tab back to the last-iterated selected file. Tab switching is impossible while a multi-selection exists.
- **Mechanism.** `last_opened_key` is a single slot. With selection `{A, B}`, each frame `A != last` opens A (last = A), then `B != last` opens B (last = B) — the loop never reaches a fixed point. `tree_selection` is a bare `std::unordered_set`, so which file wins is iteration-order-dependent. Single selection is stable only because one key can equal the one slot.
- **Immediate repair.** React to selection *changes*, not selection *state*: keep the previous selection set (in `workspace_system::data`) and open only newly added keys. Note `last_opened_key` is also written by workspace create/rename flows (Workspace.cppm:577, 595), so retiring it touches Workspace.
- **Prevention.** Recurring class ("per-frame reaction to state instead of edges"). The strongest proportionate guardrail is an `on_select` hook on `gse::gui::tree_ops` next to the existing `on_context`, making open-on-select event-driven at the widget seam and deleting the diffing entirely.

---

## Medium

### 4. The async search query driver is written three times, and the copies have diverged

`Editor/Editor/Source/App/EditorApp.cppm:79-88` + `:667-802` (`quick_search_state` / `draw_search_bar`), `SearchScreen.cppm:51-59` + `:191-303` (`search_screen`)

- **Impact.** ~80 lines of debounce / cancel / submit / arrival / keyboard-nav / accept logic exist twice with drifted behavior; Findings 1 and 2 are the drift made concrete (one copy resets selection on arrival, the other doesn't; one destroys pending state on accept, the other leaks it). Every future search surface inherits the same fork.
- **Mechanism.** `quick_search_state` and `search_screen`'s members are the same state machine (`query`, `last_query`, change time, `dirty`, `pending`, `results`, `selected`) with no shared owner. Even the sentinel conventions differ (`selected = -1` vs `0`).
- **Immediate repair.** Extract a `search::query_driver` into `gse.ide.search` owning that state with `update(now, index, opts)` (debounce + cancel + submit + arrival + selection revalidation) and `accept()`/`reset()`. Both call sites keep their own drawing and keybinding code.
- **Prevention.** The extraction *is* the guardrail: the arrival/selection invariant and the accept lifecycle become impossible to get wrong per-call-site. This is the guide's "duplicated knowledge" case, not preference churn.

### 5. Hover-panel hit-testing ignores the new horizontal scroll offset

`Editor/Editor/Source/App/EditorApp.cppm:1272` (hit) vs `:1539`/`:1555` (draw) — **in uncommitted, unbuilt code**

- **Impact.** After horizontally scrolling a code hover card, hovering an identifier inside the card resolves the *wrong* identifier (whatever sits `scroll_x` pixels to the left), so nested hover cards show documentation for the wrong symbol.
- **Mechanism.** Drawing applies `px + pad - h.scroll_x` as the text origin; `hover_panel_code_hit` computes `x = mouse.x() - (h.panel.left() + pad)` without adding `h.scroll_x` before bucketing against `caret_offsets`. The vertical axis compensates (`h.code_rect.top() + h.scroll - mouse.y()`); the horizontal axis was missed when `scroll_x` was added.
- **Immediate repair.** `const float x = mouse.x() - (h.panel.left() + ctx.style.padding) + h.scroll_x;`
- **Prevention.** One-off in fresh code, but cheap hardening: derive the draw origin and the hit-test origin from one shared expression (a small `code_origin(h, px, pad)` helper in this file) so the two can't diverge silently.

### 6. `apply_diagnostics` guts a shared channel payload and forwards the husk

`Editor/Editor/Source/App/EditorApp.cppm:1831-1841`, consumer at `Editor/Editor/Source/Search/SearchSystem.cppm:118-122`

- **Impact.** Latent contract hazard, not a live bug. `doc.diagnostics = std::move(check->result)` and `doc.lint = std::move(check->lint)` empty two fields of a `shared_ptr<analysis::diagnostics_check>`, and the *same* shared object is then republished as `search::index_merge_request`. Correctness rests on the unstated fact that the merge consumer only reads `symbols`/`refs`. The first person to read `result` or `lint` in the merge path gets silently empty vectors.
- **Mechanism.** The channel payload is shared *mutable* state with two consumers at different times; the move is a mutation of a published object.
- **Immediate repair.** Publish what the consumer needs as values: put `path`, `symbols`, `refs` (moved) into `index_merge_request` itself instead of the whole check, then `apply_diagnostics` may freely consume the rest.
- **Prevention.** Recurring class with a strong cheap guardrail: make completion payloads `std::shared_ptr<const diagnostics_check>`. `const` makes the move (and any future mutation) a compile error, which forces the explicit value hand-off. This matches the guide's published-ownership rule (immutable owning snapshots in channels) and is worth adopting for all `*_completed` channels.

### 7. `tab_order` has no owner: reconciled per-frame in draw, again at save, and `close_document` activates an arbitrary tab

`Editor/Editor/Source/App/EditorApp.cppm:2042-2059` (per-frame), `:2909-2923` (save), `workspace::close_document` (Workspace.cppm:466-477)

- **Impact.** Two observable defects and one standing cost. (a) Closing the active tab activates `documents.begin()->first` — an *arbitrary* document in hash order, not a neighbor. (b) The draw path erases/scans/sorts/reinserts `tab_order` against `documents` every frame to repair an invariant nothing maintains — the guide's "per-frame path rebuilding stable data". (c) The same reconciliation exists a second time in `save_workspace_layout`, so the repair logic itself is duplicated knowledge.
- **Mechanism.** `documents` (map) and `tab_order` (vector) are two representations of one collection, and mutation of the pair happens nowhere: `open_file`/`close_document` update only the map, so every consumer has to defensively re-derive the vector.
- **Immediate repair.** Maintain `tab_order` inside `workspace::open_file` / `close_document` / `open_scratch` (append on open, erase on close, activate the tab-order neighbor on closing the active document). Delete both reconciliation blocks.
- **Prevention.** Single-writer invariant at the owning layer. `id_mapped_collection` was considered per the mandatory questions: it does not fit directly (`tab_order` is user-reorderable presentation order, not an index map), so the proportionate guardrail is ownership placement, not new machinery.

### 8. `static std::optional<quickfix_popup> qf` — hidden global UI state inside a draw function

`Editor/Editor/Source/App/EditorApp.cppm:2476`

- **Impact.** Maintenance trap: the quickfix popup's lifetime state is invisible from `workspace::data`, survives workspace teardown/reload, and is unreachable from any save/reset/debug path. It also breaks the module's own convention — the equivalent hover UI state (`hover_stack`) lives in `workspace::data`.
- **Mechanism.** Function-local `static` in `draw_code_panel` was the path of least resistance for cross-frame popup state.
- **Immediate repair.** Move `quickfix_popup` (already a plain struct declared at line 201) into `workspace::data` beside `hover_stack`.
- **Prevention.** One-off — the other function-local statics in this file (`rebuild_glyph`, the reflection-built action tables) are immutable caches, which are fine. The rule of thumb worth keeping: mutable cross-frame UI state lives in a system's `data`, never in a `static`.

### 9. One partition holds two systems, a screen, three panels, the hover/quickfix machinery, and the layout persistence

`Editor/Editor/Source/App/EditorApp.cppm` (3229 lines; export blocks at `:2735` and `:2762`)

- **Impact.** The file violates the style guide's file-organization rule twice (two `export namespace` blocks; sizeable independent features sharing one file) and concentrates unrelated change reasons: editing the hover card, the tab strip, the ini persistence, or the split-resize logic all touch the same partition and re-serialize the same BMI for every importer. The systems-conversion goal recorded for this editor ("editor_app = shell") has regressed — the shell has reabsorbed feature code.
- **Mechanism.** Incremental growth: each feature (hover cards, quickfix, chrome metrics, layout ini, search bar) landed in the file that already had the context it needed.
- **Immediate repair.** Split along the seams that already exist as function groups, keeping module names and behavior identical: `:chrome` (editor_screen + chrome buttons + quick-search bar), `:code_panel` (draw_code_panel + hover/quickfix/diagnostic tooltips + code hit-testing), `:layout` (layout parse/save + load/save for editor and workspace), leaving `:editor_app` as the two system definitions and their `run`/`shutdown`. Deliver as file moves per the established module-rename practice.
- **Prevention.** The style guide already states the rule; the review-level principle is that a "shell" partition should contain wiring, not features. No new machinery warranted.

---

## Low

### 10. Engine `layout_store` helpers re-implemented locally

`Editor/Editor/Source/App/EditorApp.cppm:410-428` vs `Engine/Engine/Source/Fs/LayoutStore.cppm:83-89`

`trim_layout_value` and `layout_section_name` duplicate `gse::layout_store::trimmed` and `gse::layout_store::section_name` (the writer uses them for section replacement, so their semantics are already authoritative for these files). **Correction (fix pass):** the engine pair is declared in LayoutStore.cppm's plain (module-private) namespace block, not the exported one — the original claim that they are exported was wrong, so the dedup requires first exporting them, an engine change. Worth noting for the same follow-up: full `[section] key=value` parsing now exists three times across the codebase (here at `:430`, `gse::gui::parse_layout` in Engine Save.cppm:157, and layout_store's internal scan) — hoisting a shared section parser into `layout_store` would collapse all three; that is an engine change and out of scope here. `parse_layout_float/uint` via `std::stof`/`stoul` + `catch (...)` (`:475-491`) would also fold into that helper (`std::from_chars` avoids exceptions and locale).

### 11. Per-frame cost cluster (bounded, but each has a cheaper established form)

- `Editor/Editor/Source/App/EditorApp.cppm:2683` — `ids::make("##doc_text_" + std::to_string(id))` builds and hashes a string every frame. The file itself demonstrates the cheap idiom at `:1024`: `ids::make_from_key(gse::hash_combine(id, gse::stable_id("doc_text")))`.
- `SearchScreen.cppm:136-143` — `draw_row` rebuilds the location string per row per frame: `path.filename().generic_display_string()` (an encoding conversion on Windows) plus `std::to_string` and concatenations, for up to `max_results = 200` rows while the modal is open. `search::result` rows are immutable after arrival — precompute the location string once when results land (or add it to result preparation in the engine, which already precomputes `display`/`detail`).
- `Editor/Editor/Source/App/EditorApp.cppm:992-1065` — a fresh `tree_ops` (seven `std::function`s, several with captures beyond SBO) is constructed per frame. `Editor/Editor/Source/App/EditorApp.cppm:1596`, `:1689`, `:1930` — tooltips re-run `hover_wrap_lines` (string splitting + width measurement + `vector<string>`) every frame while visible; the code hover panel re-measures every body line's width per frame (`:1474-1480`).

None of these are hot enough to be urgent in an editor UI, but they are the guide's "per-frame string construction / transient ownership" class; fix opportunistically when touching the code. Also one-shot but on the draw thread: `ws.cppref.load` (`:2482`) does its file load on the first hover frame — moving it to startup or a task removes a first-use hitch.

### 12. Hover/tooltip/panel metrics are unscaled pixels while neighboring code scales

`Editor/Editor/Source/App/EditorApp.cppm:1454` (640.f), `:1487`/`:1494` (16.f anchor offsets), `:1520` etc. (+4.f shadow), `:1559-1574` (thumb geometry, 1.5f nudge), `:1585`/`:1685`/`:1926` (460.f), `:2216` (150.f button), `SearchScreen.cppm:84-85` (900/640 card)

The module is inconsistent about DPI: some metrics multiply by `sty.scale_factor` (e.g. `:671`, `:2062`, `:2065`, `:3013-3020`), while the hover cards, tooltips, quickfix panel, viewport button, and search card use raw pixel constants. At high-DPI scale factors these panels will render undersized relative to the text they contain (font sizes scale, boxes don't — clipping is the failure, not just proportions). Sweep the listed sites onto `scale_factor`.

### 13. `m_loc_label` caches forever

`Editor/Editor/Source/App/EditorApp.cppm:539-543`

The LOC badge formats once when `cpp_loc` first becomes nonzero and never updates, so a reindex that changes the count leaves a stale badge until restart. Store the value it was computed from and reformat when it changes. (State duplication in miniature: label and source can disagree.)

### 14. Sentinel identity for the game tab

`Editor/Editor/Source/App/EditorApp.cppm:27`

`viewport_tab_id = numeric_limits<uint32>::max()` overloads `active_document_id` with a magic value, which is why the fallback logic at `:2054-2058` needs two guarded branches and every consumer must remember the exemption. The mandatory-questions answer is honest: document ids are `uint32` throughout `workspace::data`, so a typed id here alone would be churn. If tab identity grows again (a second non-document tab), replace the sentinel with a variant-typed active-tab representation at the workspace level rather than a second sentinel.

---

## Cross-module notes (mechanism lives outside this module)

- `fs_node.loaded` / `fs_node.children` are `mutable` (Workspace.cppm:58-59) so `tree_ops::children` can lazy-load through `const fs_node&` during draw (`EditorApp.cppm:993-998`) — a const-hole plus filesystem IO inside the draw path. The style guide's `mutable` rule is aimed at exactly this shape. Belongs to the Workspace review; noted here because the App draw path is what triggers it.
- `close_document`'s arbitrary-successor behavior is Workspace code but only observable through this module's tab UI (Finding 7 covers the repair).

## What holds up well

Verified positives, recorded so later reviews don't re-litigate them:

- `search_system::data.index` is `[[= gse::stable_shared]] std::unique_ptr` (SearchSystem.cppm:14); capturing the raw `index_state*` into long-lived screens is exactly the published-ownership pattern the guide blesses.
- `git::status_snapshot` generations are captured by owning value into the `menu_content` lambdas (`EditorApp.cppm:3174`) — immutable snapshot through a channel, per the guide.
- `query_buffer` ownership is sound: both UI and worker hold the `shared_ptr`; the worker writes results before `done.store(release)`, the UI moves them after `acquire`; a cancelled worker scribbles only on a buffer the UI has dropped.
- Dimensional types are used where physics-shaped math appears: `spinner_angular_velocity * now` → `angle` with `gse::fmod` against a `full_rotation` constant (`:1096-1098`), debounces and save intervals as `gse::time`.
- Channel pushes consistently use explicit template arguments with designated initializers; context menus are reflection-derived (`build_context_actions` over annotated functions, `:861-883`) rather than hand-rolled tables; chrome→workspace decoupling goes through `jump_to_request` as designed.
- Only the top screen builds per tick (MenuStack.cppm:289), so the Ctrl+F push at `:513` cannot stack duplicate search screens — verified rather than assumed.

## Guide evolution candidates

Two findings look like stable classes rather than one-offs; if they recur in later module reviews, promote them into `CODE_REVIEW_GUIDE.md`:

1. **Completion-channel payloads should be `shared_ptr<const T>`** — makes Finding 6's move-from-shared-payload unrepresentable engine-wide.
2. **A selection index into asynchronously replaced results must be revalidated at the swap site** — Finding 1's class; the guide's "two fields can disagree" question almost covers it, but the async-arrival framing is what both copies missed.

---

## Fix pass outcome (2026-07-20)

Applied the same day by three sequential agents, each diff-reviewed against a pre-fix snapshot. **Owner-verified 2026-07-21: builds and runs clean.**

- **Findings 1, 2, 4 — fixed.** New partition `Editor/Editor/Source/Search/QueryDriver.cppm` (`gse.ide.search:query_driver`, registered in the Import header) owns the debounce/cancel/submit/arrival/accept lifecycle once; `quick_search_state` shrank to `{ driver, input }` and `search_screen` to `{ driver, locations, input, dismiss }`. Arrival resets `selected`; both accept paths cancel and reset `pending`. Two documented deviations: the modal Enter path gained the same `>= 0 ? : 0` fallback the quick bar uses (the driver's selection reset on query change made it necessary), and a null index now clears results where the modal previously dereferenced unconditionally.
- **Finding 3 — fixed.** `last_opened_key` replaced by `explorer_selection_seen` (a previous-selection set beside `explorer_selection`); the explorer loop opens only newly selected keys. All four workspace writers migrated by intent: create/rename insert (suppress), commit/cancel erase (force reopen).
- **Finding 5 — fixed.** `hover_panel_code_hit` adds `+ h.scroll_x`, matching the draw origin.
- **Finding 6 — fixed.** `index_merge_request` now carries `path` + `symbols`/`refs` by value (moved from the check in `apply_diagnostics`); the consumer's `req.check` indirection and guard are gone. The `shared_ptr<const>` completion-payload guardrail stays a guide-evolution candidate.
- **Finding 7 — fixed.** `tab_order` is single-writer: `open_file`/`open_scratch` append, `close_document` erases and activates the tab-order neighbor (next, else previous, else none) computed before erasure. Both per-frame and save-time reconciliations deleted; one minimal guard kept in `draw_code_panel` (empty tab strip promotes to the viewport tab — preserves the default-to-Game startup and last-close behavior).
- **Finding 8 — fixed.** `quickfix_popup` lives in `workspace::data` beside `hover_stack`; the function-local `static` is gone.
- **Finding 9 — done** (see the split addendum below).
- **Finding 10 — partially applied.** `from_chars` replaces `stof`/`stoul` + catch. The helper dedup was blocked by the correction above (engine helpers were module-private); completed in the split addendum below.
- **Finding 11 — ids::make_from_key and the modal location cache applied.** The per-frame `tree_ops` rebuild and tooltip re-wrap items were deliberately left for opportunistic cleanup, as the finding stated.
- **Findings 12, 13 — applied** (all listed DPI sites scaled; LOC badge re-formats on value change; cppref index loads in the workspace init block, draw-site guard kept as a no-op fallback).
- **Finding 14 — deferred by design.**

Post-fix verification: no leftover references to any removed identifier (`last_opened_key`, `qf`, old screen members, `req.check`, `##doc_text_`, `stof`/`stoul`); no file outside the six intended ones changed (Engine/ and Analysis/ untouched, confirmed by mtime sweep against the snapshot).

---

## Split + engine follow-up (2026-07-21)

Finding 9 executed as a pure-motion split — agent-performed with byte-level verification (every moved range `cmp`-identical to the original, full line-coverage audit), then independently review-verified (definition accounting 48 = 6 + 12 + 19 + 11 across the four files; per-file structure and import lists checked).

- `gse.ide.app` now has partitions `:chrome` (editor_screen, chrome buttons, explorer panel, quick-search bar, context-action tables, spinner/glyphs — Chrome.cppm, 754 lines), `:code_panel` (the code panel plus all hover/quickfix/diagnostic-tooltip machinery and the format/save/diagnostics application — CodePanel.cppm), and `:layout` (ini parsing + workspace layout persistence — Layout.cppm, 233 lines), with EditorApp.cppm reduced from 3161 lines to the two system definitions plus editor-layout load/save (392 lines). `load_editor_layout`/`save_editor_layout` stayed in `:editor_app` deliberately: they take `editor_app::data&`, and moving them would cycle the partitions. Dependency direction is `:editor_app` → { `:chrome`, `:code_panel`, `:layout` } with `:code_panel` → `:chrome`.
- `viewport_tab_id` moved to `gse.ide.workspace`'s exported block, beside the struct whose field it sentinels — the placement finding 14 anticipated.
- Finding 10 completed: `layout_store::trimmed`/`section_name` declarations moved into the engine's exported block (definitions untouched), and the editor's duplicate `trim_layout_value`/`layout_section_name` deleted with all four call sites rewired. The three-parsers-to-one section-parser consolidation remains an optional engine follow-up.
- **Not yet compiled** — the split and the export await an owner build. One risk to watch in that build: the split relies on cross-partition use of module-linkage (non-exported) entities via plain partition imports; if the toolchain's BMIs reject that shape, the fallback is exporting the handful of cross-partition symbols from `:chrome`/`:code_panel`/`:layout`.
- Concurrent owner edits observed during this pass (hover-state `scroll_axis` members in Workspace.cppm and related CodePanel.cppm changes, timestamped after the split landed) are outside this pass — left untouched and unreviewed.

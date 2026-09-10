The style for this codebase is the STL style. We do, however, prefix private variables with m\_.

Do NOT put comments in your code. The code should be self documenting.

See `docs/STYLEGUIDE.md` for the full style guide.

## Function Signature Layout

- Declarations with parameters always wrap: one parameter per line and `)` on its own line.
- Definitions never wrap their parameter list.
- Zero-parameter declarations remain inline as `()`.
- Put one blank line between adjacent function declarations.
- This applies to constructors, operators, static functions, and templates.

## Unit Types

Unit types (e.g., `gap`, `displacement`, `velocity`, `force`, etc.) have the **same memory layout as their underlying float**. They can be passed directly to GPU push constants, `memcpy`, etc. without any cast or extraction — they are layout-compatible with `float`. Never `static_cast` a unit type to float. Just use it directly.

## GUI Overlays and Hit Testing

A widget is interactive only where `hovers()` / `mouse_pressed_for()` say it is, and those consult **more than the rect you drew**. Every floating element must satisfy all four of these, or it renders perfectly and does nothing:

- **Clip.** `queue_sprite` / `queue_text` apply the parent clip only for layers **at or below `popup`**; `modal` and above draw outside their panel on purpose. Hit testing follows the same rule through `draw_context::clip_for(layer)` — that one function is the authority for both halves. Never re-derive it: a bare `current_clip()` check in an input path silently makes anything drawn beyond the panel visible-but-dead. `current_clip()` is for drawing and measuring only.
- **Layer.** Put the overlay on `render_layer::modal` via `ctx.scoped_layer(...)`, and call `ctx.register_hit_region(render_layer::modal, body)`. The hit region is what stops the panel *behind* from eating the click. Regions are double-buffered (`input_layer::begin_frame` swaps them), so `topmost_at` reads **last** frame — blocking-behind starts on frame 2, which is fine because a higher layer wins outright on frame 1.
- **Press ownership.** `mouse_pressed_for` **consumes** the press globally, not per-rect. Within one frame, whatever draws first wins on overlapping rects at the same layer.
- **Open on release, never on press.** `dropdown` selects an option on `mouse_pressed_for`, so anything opened from a dropdown entry is born mid-press and a raw `ctx.mouse_pressed()` dismiss check fires on the very press that opened it. Open overlays from a `gui::button` (`press_in_rect` → activates on release).
- **Dismissal has one authority:** `gui::interaction::dismissed_by_outside_press(ctx, { .body, .keep_open, .suppressed })`. Do not hand-roll `ctx.mouse_pressed() && !rect.contains(...)` — that spelling was duplicated in three places and is what `docs/GRAPHICS_REVIEW.md` flags as re-deriving interaction policy below `draw_context`. `keep_open` holds the anchor/opener rects a press on which must *not* dismiss; `suppressed` covers "a nested overlay is open" or "a scrollbar is being dragged".

Sizing is derived, not typed: the label column of every labelled widget is `style::label_column_ratio`, so a container that hardcodes its width will truncate labels. Size the container from the widest label and that ratio.

`gui::toggle` is the boolean widget (`gui::checkbox` has no call sites). `gui::selectable` is auto-layout only — for a rect-form row use `gui::interaction::press_in_rect`.

## Logging System

A logging system writes to `%LOCALAPPDATA%\GSE\logs\<exe>.<pid>.log` (e.g. `Editor.13832.log`, `GoonSquad.4120.log`), one file per run, keeping the last few. Assertions automatically log failures.

**To debug issues:** query the log with the `gse_log_query` tool (filters by level, category and regex, returns the last N matching lines of the newest run) instead of asking the user to paste console output or reading the whole file. Fall back to reading the file only when the tool is not available in your session.

**Implementation:** `Engine/Engine/Import/Log.cppm` - read this file for the current API.

## Building and Waiting (agents)

Builds go through the running editor only. Use the `gse_build` tool (or `Tools/gse-build` in a shell); cmake, ninja and the compilers are deliberately not on your PATH. If the build is deferred because another chat is mid-work, call `gse_hibernate` (or `Tools/gse-hibernate --then "..."`) and end your turn: the editor wakes you with the result. Never poll with `sleep`; it is blocked. `gse_build_status` shows the queue.

Captured run traces under a project's `.gse/data/eval` (`train_*`, `smoke_*`, `parity_*`, `play_*`) are queried with `gse_trace_query`: call it with `run` and `summary=true` to see the line families, then filter by family, regex or step range and ask for fields, a bounded tail, or aggregates. Do not `grep | tail` these files; they run to tens of megabytes.

These tools come from `Tools/gse-mcp/server.mjs`, which the editor's agent panel loads automatically. A terminal session gets them with `claude --mcp-config Tools/gse-mcp/mcp.json` from the engine root. Design and roadmap: `plans/editor-agent-select-api.md`.

## Finding Code

`gse_symbol_query` answers from the semantic index the editor already keeps for go-to-definition, so looking a symbol up costs one call instead of a grep plus a guessed `sed -n` slice. Pass `name` (bare or qualified) and you get every declaration and definition site — file, line, kind, resolved type — definitions first, each with its own source text sliced to the end of the definition. Pass `file` instead for that file's outline: the types, functions, members, enumerators and aliases it defines, with lines. Reading engine source to find a symbol was half of all tool output before this existed.

Add `references: true` to a `name` for the other direction: every place the symbol is *used*, one source line each, sorted by file and line, with the declaration sites themselves left out. These are the compiler's own cross-references, so they follow aliases and skip the name where it appears in a comment, a string, or on an unrelated same-named symbol — distinctions `grep -rn` cannot make. `total` is the true count even when `max_matches` caps the list, so it also answers "is this still used anywhere".

It is an accelerator, not a gate. Reading and grepping source stays open, and you need it when no editor is running, when the answer comes back `indexing` because the index is still building, and whenever you are searching free text rather than a name.

## Config Module

`gse.config` (in `Engine/Engine/Import/Config.cppm`) provides every engine path as a **function**, resolved at runtime — `resource_path()`, `root_dir()`, `user_config_dir()`, etc. Re-exported by `gse.utility`. Nothing is baked into the binary: paths come from a `gse.manifest` marker file found by walking up from the executable's directory (CMake writes one to the build root at configure time with `mode = dev` and `root = <source tree>`).

Where runtime files go — **none of them land in the repo**:

| What | Where |
|---|---|
| `<executable>.ini`, `gui_layout.ini`, `editor_layout.ini` | `%APPDATA%\GSE` (`user_config_dir()`) |
| logs, crash dumps, profiles, screenshots/recordings/clips | `%LOCALAPPDATA%\GSE` (`logs_dir()`, `crash_dir()`, `profile_dir()`, `captures_dir()`) |

**User-scope settings live at `%APPDATA%\GSE\<executable_stem>.ini`** — one file per executable, so the editor and the games it launches no longer share a `[Window]` section. That is the file to hand-edit; the old `%APPDATA%\GSE\settings.ini` and `Engine/Resources/Misc/settings.ini` are no longer read. There is no automatic migration from the old locations.

Override the roots with the `GSE_USER_DIR` and `GSE_STATE_DIR` environment variables. Running an executable with no `gse.manifest` above it aborts with a message on stderr.

Set `GSE_ASSET_DEBUG_OUTPUT=1` to make asset bakers write their debug artifacts — currently the font MSDF atlas PNG, ~30 MB per font. Off by default because it costs a full PNG compression pass on every bake.


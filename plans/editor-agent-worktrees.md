# Editor agent panel + multi-worktree intelligence

Goal: run Claude Code agents natively inside GSEditor, one or more per git worktree, with the
editor as the review surface. Decided 2026-08-15 with Dhiren.

## Settled decisions

- **Headless, not TUI.** The editor drives `claude` over NDJSON in both directions and renders the
  transcript with its own widgets. It does NOT host the Claude Code TUI.
- **One editor, full intelligence per worktree.** Every registered worktree gets real indexing,
  goto-def, diagnostics and build — not a read-only foreign-tree mode.
- **Agents run `--permission-mode auto`.** No in-panel approve/deny surface; agents self-check
  commands. Review happens after the fact, on diffs. Consequence: in `-p` mode a refusal does not
  block and wait, it denies and continues — so the panel MUST surface `result.permission_denials`
  prominently, or an agent will silently skip work with no visible cause.
- **Escape hatch, not a fallback.** Streaming is the only integration. Anything genuinely needing
  the real TUI opens Windows Terminal on the same session (see Part 4).

## Why headless

`Terminal.cpp:394` runs `cmd.exe /c <cmd>`, captures a pipe, strips ANSI, and renders a read-only
tab. Hosting the real TUI needs ConPTY plus a VT emulator (alt screen, cursor addressing, scroll
regions, mouse reporting, resize) — weeks of work whose payoff is a worse Windows Terminal.

The headless protocol, verified against CLI 2.1.132:

```
claude -p --output-format stream-json --input-format stream-json --verbose
       --include-partial-messages --permission-mode auto --session-id <uuid>
```

NDJSON, one object per line, both directions. Observed event types:

| type | subtype | carries |
| --- | --- | --- |
| `system` | `init` | session_id, cwd, model, tools, mcp_servers, permissionMode |
| `system` | `hook_started` / `hook_response` | hook_name, stdout, stderr, exit_code, outcome |
| `system` | `api_retry` | attempt, error_status, retry_delay_ms |
| `assistant` | — | `message.content[]` incl. `text` and `tool_use` |
| `result` | `success` | duration_ms, num_turns, total_cost_usd, usage, permission_denials, is_error, api_error_status |

The editor renders `tool_use` for Edit/Write as a real diff in its own highlighter with click-to-open
at line. That is the entire justification for the feature — a CLI cannot do it.

## Part 1 — plural config

`ide::config::table()` is `static const resolved value = resolve();` (Import/Config.cpp:94). Every
accessor returns `const path&` into it, and worker threads hold those references across frames.
**Keep that.** The table stays resolved-once and never mutated.

`resolved` splits into editor-global `editor_paths` plus a `std::vector<worktree>`, both resolved
once into a magic static and never mutated. `worktrees()` and `browse_roots()` return
`std::span<const T>`; `browse_roots()` is the flattening of all worktrees plus the fixed Editor and
user-data roots.

**Immutable and plural — not mutable and synchronised.** An earlier pass built append-only
`std::deque` storage with a mutex and published `shared_ptr<const vector<const T*>>` generations so
a worktree could be registered at runtime. That was deleted: every `browse_roots()` caller is
init-time (`EditorApp` inside `if (!d.initialized)`, `SearchSystem::init`, Git discovery on
refresh), so the cached generations bought nothing, and the mutation they protected broke the one
invariant that matters. Do not reintroduce it without a measured hot reader.

Consequence: a worktree created *after* the editor starts gets no indexing until relaunch. Agents
are unaffected — they only need a cwd and never touch config — so a new tree is immediately
agent-usable. This matches how project switching already works.

Startup discovery reads git's own on-disk layout, no subprocess (and no dependency edge from
`gse.ide.config` to `gse.ide.build`, which would be a cycle): resolve `<root>/.git`, following the
`gitdir:` link when it is a file, then read `<git-dir>/worktrees/*/gitdir`. Each worktree is derived
from the primary by prefix substitution (`retarget`), so paths that live outside the engine tree —
`project_state`, and therefore editor layout and settings — are deliberately shared across
worktrees of the same project.

Already plural, so free or nearly so:

- `Search/SearchSystem.cpp:29` — iterates `browse_roots()`, pushes each into `index->roots` with its
  own compile_commands, dedupes the databases.
- `Search/Index.cppm` — `index_state::roots` is already a vector; `symbol_index` / `module_index`
  are already root-tagged.
- `Git/GitSystem.cppm:52` — iterates `browse_roots()` for repository discovery.
- `App/EditorApp.cppm:391` — file tree iterates `browse_roots()`.
- `App/CodePanel.cppm:1436` — analysis already picks `compile_commands_for(doc.path)` per file.

## Part 2 — the singular consumers

These assume exactly one project and are the actual work:

- **`BuildRunner.cppm`** — DONE. `build_request` gained `const config::worktree* tree` (null =
  primary), and the whole build path threads `const config::worktree&`: `project_source_roots`,
  `refresh_changed_sources`, `configure_command`, `ensure_configured`, `launch_game_attached`,
  `build_game`. `cleanup_backups` loops every worktree. `rebuild_editor` stays editor-global.
  Three things stay deliberately editor-global because they describe the *running editor*, not the
  tree being built: the vcpkg install dir and inherited toolchain cache entries in
  `configure_command`, and `compiler_bin_dir(config::build_dir())`.
- **`Terminal.cpp`** — file-link resolution now tries every worktree root instead of assuming the
  primary. `d.prompt` and the interactive command cwd stay on the primary, which is correct: they
  describe the editor's own working directory, and retargeting them needs a picker (stage 5).
- **`CodePanel.cppm:1249`** — DONE. Diagnostic display paths resolve against
  `worktree_for(diagnostic_path).project_root`.

**Scope call on concurrency.** `d.building` (the plan originally called it `build_running`) stays a
single global gate, and builds remain serialized across worktrees. Making it per-worktree means N
`build_completion`s, N game processes, and N attached surfaces — but the viewport and IPC pipeline
are single-session by construction (one `surface_pipe`, one imported ring, one `display_slot`), and
concurrent full C++ builds would thrash the machine regardless. You can now build *any* worktree;
you cannot build *two at once*. Lifting that is its own piece of work, not part of stage 2.

## Part 3 — agent sessions

New editor system `gse.ide.agent`, same `[[= system_state]]` + channel shape as `git_system`.

- **Duplex spawn.** `spawn::run_capture` is one-shot. Needs a long-lived variant that keeps the
  stdin write end open. It MUST copy the `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` discipline — a bare
  `bInheritHandles=TRUE` from a live GPU app is what corrupted cc1plus before.
- Reader thread accumulates lines, parses via `Analysis/Json.cppm`, pushes typed events into a
  channel drained in `run()`. Structurally identical to `build_runner::output_stream`.
- **The editor generates the session UUID** and passes `--session-id`, so it owns session identity
  and can `--resume <uuid>` after an editor restart.
- Session state: `{ workspace, uuid, model, state, transcript, cost, last_activity }`.
- Worktree registry seeded from `git worktree list --porcelain`. The editor owns worktree creation
  (so it can bind a project at the same time) rather than delegating to `claude -w`.

## Part 4 — the terminal escape hatch

Building a TUI host was costed and rejected (roughly 4-6x the plumbing, and it deletes the review
surface). The blocker is the font atlas: `Graphics/2D/FontCompiler.cppm:60-63` bakes MSDF glyphs at
compile time over `U+0020-U+007E` and `U+00A0-U+00FF` only. No box drawing, no block elements, no
braille spinners — all of which the Claude Code TUI is built from. Emoji are structurally impossible
without a second text path, since MSDF is monochrome and emoji need colour bitmaps.

Instead, every session row gets an "open in terminal" action:

```
wt.exe -d <worktree path> claude --resume <uuid>
```

Because the editor generated the session UUID (Part 3), this drops into the real TUI with full
history. It covers the cases streaming does not:

- **Re-auth.** Token expiry arrives as `result.is_error` + `api_error_status: 401` and cannot be
  resolved in-panel.
- **Interactive pickers.** `/resume`, `/plugin`, model selection, anything with a chooser.
- **Human-in-the-loop tools.** `AskUserQuestion` and `ExitPlanMode` want a real prompt; see the
  open questions below.

Cost is one `ShellExecute` call. This is why editor-owned session UUIDs are load-bearing rather than
incidental.

The `system/init` event also carries the full `slash_commands` and `agents` lists for the session, so
the panel can build native autocomplete for both without hardcoding anything.

## Staging

0. DONE. Duplex spawn + NDJSON reader. `launch_streamed` gained an stdin pipe (`launched::input`);
   `spawn::read_lines` was split out of `pump_output` so a caller can get lines without an
   `output_stream`; `gse.ide.agent` spawns `claude`, parses NDJSON, and emits summaries into a
   terminal tab. Trigger is the terminal builtin `agent <prompt>`.
1. DONE. Plural config — immutable, resolved once, `std::span` accessors, startup discovery of
   git worktrees. (An intermediate mutable/synchronised version was built and then deleted; see
   Part 1.)
2. DONE. BuildRunner + Terminal + CodePanel worktree parameterization.
3. Agent panel: session list, transcript rows, prompt input.
   Transcript model DONE — `transcript_row { row_kind, text, detail }` on the session is now the
   source of truth; `summarize()` returns rows, `permission_denials` are parsed into
   `row_kind::denial`, and a 401 result renders as the actionable token message rather than a raw
   error. The terminal tab is fed as a PROJECTION via `append_row` so nothing regresses; delete
   that projection (and the last `shared_ptr`) once the panel renders.
   Sessions are no longer erased on exit — the transcript must outlive the process so it stays
   readable. Handles close once; the panel needs a close action.
   Panel DONE. `agent::draw_panel` renders a session tab strip, a `gui::scroll_region` transcript,
   and a prompt input; `run()` publishes it as `gui::menu_content` under `panel_name = "Agent"`.
   Layout is a fourth `update_split` on `final_columns.second` (code|agent) with `agent_ratio` /
   `resizing_agent` persisted alongside the other ratios.
   WRAPPING DONE. `text_area` does not wrap — it is the code editor widget, and wrapping it would
   mean remapping caret, hit-testing and selection for every caller. So the transcript wraps at
   FLUSH time instead: `push_transcript_line` takes the prefix separately from the text, measures
   with the code font (the face `text_area` actually renders with), and pushes one buffer line per
   `font::wrap` segment, all mapped to the same row via `line_rows`. Continuations are indented by
   the prefix width, so `- `/`+ `/`> ` columns stay aligned. Selection, copy and click-to-jump all
   keep working because they were already row-mapped rather than line-mapped.
   The wrap width is `area.width() - pad * 2 - scrollbar_width`; subtracting the scrollbar
   unconditionally keeps the width stable instead of flapping as the bar appears. `session` gained
   `wrap_width`, and a width change drops the buffer/spans/line_rows and re-flushes every row —
   a full rebuild, but only on a panel resize. `text_area` clamps caret and anchor on entry and its
   `width_sig` is content-derived, so nothing goes stale under the rebuild.
   KNOWN LIMITS, deliberate: `font::wrap` breaks on spaces only, so an unbroken token wider than
   the panel (a long path, a JSON blob) still overflows into horizontal scroll; there is no
   per-session close action, so sessions accumulate for the process lifetime; and the terminal
   projection in `append_row` is still live. Delete the projection and the last `shared_ptr` once
   the panel has proven itself.
   3a still owed: the "open in terminal" action (`wt.exe -d <worktree> claude --resume <uuid>`,
   which needs the editor to pass `--session-id` at spawn — it does not yet) and a denial banner
   promoting `row_kind::denial` out of the transcript.
   3a. "Open in terminal" per session (`wt.exe -d <worktree> claude --resume <uuid>`), plus a
       denial banner driven by `result.permission_denials`. Both are cheap and both are what make
       stage 3 usable on its own.
4. Review surface — first cut DONE. `tool_row()` keeps a `tool_use` block's `input` instead of
   discarding it: `file_path` becomes `transcript_row::file`, `Edit`'s `old_string`/`new_string`
   become `removed`/`added`, `Write`'s `content` becomes `added`, and `Bash`/`PowerShell` keep the
   command as detail. The transcript renders those as indented `-`/`+` lines, and clicking any row
   carrying a file pushes `jump_to_request`.
   `jump_line_for` searches the file for the FIRST LINE OF `added`, not `removed` — by the time you
   click, the edit has already been applied, so the new text is what is actually in the file.
   **DONE — the transcript is now a read-only `gui::text_area`, not a `queue_text` loop.** The
   session owns `text_buffer` + `text_area_state` + `vector<text_span>` + `line_rows`, and
   `sync_transcript` flushes new rows into them at draw time (style is unavailable in `run`, the
   same reason Terminal defers via `append_lines`). `flushed_rows` makes it incremental rather than
   a per-frame rebuild. Clicks map through `text_area_position_at` -> line -> `line_rows` -> owning
   row -> `jump_to_request`, which needed a `vec2f mouse` threaded through `draw_panel`, so `run`
   gained `shared_view<input::data>`. `scroll_region`, `layout_cursor`, and both emit lambdas are
   gone — `text_area` owns scrolling, selection, copy, and hit-testing. Original note kept below
   because the reasoning is what matters: Terminal already does this (Terminal.cpp:547) and
   gets selection, copy, and click-to-jump for free; the transcript currently has none of them
   because it paints text directly. `text_area::params` takes `std::span<const text_span>`, which
   is per-range colouring — the same mechanism the code editor uses for syntax highlighting — so
   moving to it delivers row colours, selection, AND highlighted diffs from one change instead of
   three. Click mapping is `gui::draw::text_area_position_at` (see Terminal.cpp:529, which turns a
   hovered line into a `jump_to_request`); keep a line-index -> row map alongside the buffer so a
   click can find the row that owns the line.
   ANSWERED: `text_area` does NOT wrap, so `font::wrap` stays — see the wrapping note in stage 3.
   Still open after that: diff colours — the theme has no red/green, so `-` uses
   `color_text_disabled` and `+` uses `color_accent`, carried by the prefix. Proper colours mean
   adding tokens to `gui::style`.
5. Worktree dashboard: branch, dirty count, agent state, last message, cost.

Stages 0 and 1 are independent. Stage 3 needs 0; stage 5 needs 1 and 3.

## Operational notes

- Spawned agents inherit the parent shell's hooks and plugins; a probe showed eight SessionStart
  hooks firing before the first token, two of them erroring on a missing Bun. `--bare` skips hooks,
  plugins, and CLAUDE.md discovery if the noise is a problem.
- Auth failures arrive as a normal `result` with `is_error: true` and `api_error_status: 401`
  ("OAuth access token has expired"). The panel must surface this distinctly from a model error —
  it is a re-auth prompt, not a failed turn.
- **Headless needs its own credential.** Verified 2026-08-16: an interactive TUI session can be
  fully logged in while `-p` still 401s, in BOTH `--input-format text` and `stream-json`. The CLI
  emits `control_request{subtype: "oauth_token_refresh"}` on stdout — delegating the refresh to a
  client — and when nothing answers it falls through to `api_retry` and a synthetic error result.
  `claude setup-token` provisions a long-lived non-interactive token. Treat this as a prerequisite
  for the panel, and detect it early rather than reporting it as a model failure.
- **The CLI issues control requests on stdout and expects `control_response` on stdin.** The reader
  in stage 0 is one-way and ignores them. Before the panel is relied on, decide which subtypes it
  answers; `oauth_token_refresh` is the one observed so far.
- Result payload field order is not stable (`is_error` has been seen first, ahead of `type`). Parse
  by key only — never by position or by the first key present.

## Verified against a live session (2026-08-16)

- **The stdin message schema is correct.** `{"type":"user","message":{"role":"user","content":
  [{"type":"text","text":"..."}]}}` followed by a newline produces a normal turn.
- **One process serves many turns.** With stdin held open the process does NOT exit after `result`;
  writing a second user message to the same process produces a second answer. The long-lived
  session model is correct — no spawn-per-turn with `--resume` is needed.
- **Assistant content arrives as separate events per block.** A `thinking` block and a `text` block
  from one message id came as two `assistant` events, so the renderer must append per event rather
  than expect one event per message.
- **Event types the parser must swallow or it drowns.** A two-word answer produced seven
  `system/thinking_tokens` events, plus `rate_limit_event` and `system/post_turn_summary`. These
  carry no transcript value. `hook_started` is noise; `hook_response` matters only when
  `exit_code != 0`.
- **Billing draws on the Claude subscription, not metered API spend.** `claude setup-token` mints a
  subscription OAuth token (`apiKeySource: "none"`), and turns report
  `rate_limit_info.rateLimitType: "five_hour"` with `overageStatus: "rejected"` /
  `overageDisabledReason: "org_level_disabled"` — the five-hour rolling window is the subscription
  limiter, and overage (the only path that bills beyond it) is off. `total_cost_usd` is an
  API-equivalent ESTIMATE, not a charge; label it as such in any UI or it reads as a bill.
- **The scarce resource is the five-hour window, not dollars.** Editor-spawned agents consume the
  same subscription budget as interactive Claude Code use, so several agents across worktrees
  contend with the developer's own session. Surface `rate_limit_event` whenever `status != "allowed"`
  or `isUsingOverage` is true — it is the only warning before work starts failing.
- **Cost is dominated by context load, not the turn.** "Say OK" reported ~19k cache-creation tokens
  because the session loads project context on init. Per-session overhead is roughly fixed, so many
  short sessions burn the window far faster than one long one — which the process-survives-turns
  result makes easy to avoid.

## Agent credential inheritance — the durable fix (DONE)

Confirmed live 2026-08-19: the panel spawned a session that 401'd because the EDITOR process was
started before `CLAUDE_CODE_OAUTH_TOKEN` was set at User scope. `spawn::launch_streamed` passes
`nullptr` for `lpEnvironment`, so the agent inherits the editor's environment, and the editor's own
environment is frozen at launch. The in-editor rebuild-and-relaunch makes this permanent: the new
editor inherits from the old one, so the stale value propagates forever until launched fresh from a
shell that has it. Telling the user to relaunch is a workaround, not a fix.

Shipped as four steps:

1. `Engine/Engine/Source/External/Win32.cppm` — `read_user_environment(const wchar_t* name,
   wchar_t* out_value, DWORD out_capacity) -> bool` opens `HKCU\Environment`, reads one `REG_SZ` /
   `REG_EXPAND_SZ` value, and guarantees termination. `HKEY`, `LSTATUS`, `KEY_READ`, `REG_SZ` and
   friends are NOT exported — they stay inside the function body, which is why the signature is
   C arrays only. **This was a deliberate departure from the original plan step 1**: exporting the
   registry primitives would have meant exporting `HKEY_CURRENT_USER`, and that macro expands to a
   reinterpret cast, so it cannot be a `constexpr` constant the way every other constant in that
   file is. `GetEnvironmentVariableW` and `ExpandEnvironmentStringsW` are exported normally.
2. `Win32Environment.cppm` — `user_environment_value(std::wstring_view) -> std::wstring` wraps the
   raw read and expands `%VAR%` when the value contains one, and `environment_with_variable(name,
   value) -> std::vector<wchar_t>` builds a block with that variable set. Both sit on new shared
   internals — `current_environment_entries()` and `environment_block(entries)` — that
   `environment_with_path_prefix` now uses too, so there is one entry-split/sort/double-null path
   instead of two. `environment_name_is_path` became a one-liner over a general
   `environment_entry_named(entry, name)`.
3. `spawn::launch_streamed` — takes `std::span<const wchar_t> environment = {}`. Empty keeps the
   old behaviour exactly (`nullptr`, no `CREATE_UNICODE_ENVIRONMENT`), so the game-launch caller in
   `BuildRunner.cppm:881` is untouched.
4. `agent::agent_credentials()` — probes `GetEnvironmentVariableW(L"CLAUDE_CODE_OAUTH_TOKEN")`; if
   the process has it, spawn inherits as before. If not, it reads the User-scope registry and
   spawns with a block carrying the token. If neither has it, the session still starts but the
   transcript opens with a `row_kind::failure` naming `claude setup-token` — the 401 is predicted
   instead of waited for.

The editor is now immune to its own launch context, which was the actual defect. It re-reads the
registry per session rather than caching, so `claude setup-token` followed by a NEW SESSION is
enough — no editor relaunch, and rebuild-and-relaunch no longer propagates a stale environment.
The process value still wins when present, so a shell that deliberately exports a different token
keeps control.

## Open questions

1. **Do slash commands work as stream-json input message text?** `system/init` advertises the
   command list, so the panel can build a picker from it — but sending one has not been tried.
2. **What does `AskUserQuestion` do in `-p`?** There is no prompt surface. It either auto-proceeds,
   errors, or stalls. If it stalls, the panel needs a watchdog and a nudge toward the escape hatch.
3. **What does `ExitPlanMode` do in `-p` under `auto`?** Plan approval is human-in-the-loop by
   design. If plan mode is unusable headless, that is a documented limitation, not a bug to fix.

Composes with [[editor-first-restructure-plan]] (which fixed one project per process — this
supersedes that for worktrees specifically), [[editor-terminal-spawn-plan]] (spawn discipline),
[[editor-inherited-handles-corrupt-gpp.md]] (handle-list requirement).

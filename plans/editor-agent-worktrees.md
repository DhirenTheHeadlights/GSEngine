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

Change `resolved` from one project + one engine to a list of *workspaces*, each
`{ name, engine_root, project_root, build_dir, compile_commands, accent }`. `browse_roots()` becomes
the flattening of all workspaces.

Storage must be `std::deque<browse_root>`, not `vector` — appending a worktree at runtime must not
invalidate the `const path&` and `span` references already handed out. Entries are append-only;
removing a worktree marks it inactive and never erases.

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

0. Duplex spawn + NDJSON reader, events dumped raw into a terminal tab. Zero UI, proves the pipe.
1. DONE. Plural config. 1a split `resolved` into editor-global `editor_paths` plus a
   `std::deque<worktree>`; 1b added the `registry` (mutex + append-only deques + published
   `shared_ptr` index) and `register_worktree`.
2. DONE. BuildRunner + Terminal + CodePanel worktree parameterization.
3. Agent panel: session list, transcript rows, prompt input.
   3a. "Open in terminal" per session (`wt.exe -d <worktree> claude --resume <uuid>`), plus a
       denial banner driven by `result.permission_denials`. Both are cheap and both are what make
       stage 3 usable on its own.
4. Review surface: Edit/Write tool_use to inline diff, click to open at line.
5. Worktree dashboard: branch, dirty count, agent state, last message, cost.

Stages 0 and 1 are independent. Stage 3 needs 0; stage 5 needs 1 and 3.

## Operational notes

- Spawned agents inherit the parent shell's hooks and plugins; a probe showed eight SessionStart
  hooks firing before the first token, two of them erroring on a missing Bun. `--bare` skips hooks,
  plugins, and CLAUDE.md discovery if the noise is a problem.
- Auth failures arrive as a normal `result` with `is_error: true` and `api_error_status: 401`
  ("OAuth access token has expired"). The panel must surface this distinctly from a model error —
  it is a re-auth prompt, not a failed turn.

## Open questions — verify empirically once auth is restored

All three are cheap probes against a live session, and none of them change the architecture. They
change what stage 3 must render.

1. **Do slash commands work as stream-json input message text?** Expected yes, and `system/init`
   already advertises the command list — but the plugin set here is large (gsd, claude-mem,
   context-mode) and it is worth confirming before building a picker on top of it.
2. **What does `AskUserQuestion` do in `-p`?** There is no prompt surface. It either auto-proceeds,
   errors, or stalls. If it stalls, the panel needs a watchdog and a nudge toward the escape hatch.
3. **What does `ExitPlanMode` do in `-p` under `auto`?** Plan approval is human-in-the-loop by
   design. If plan mode is unusable headless, that is a documented limitation, not a bug to fix.

Composes with [[editor-first-restructure-plan]] (which fixed one project per process — this
supersedes that for worktrees specifically), [[editor-terminal-spawn-plan]] (spawn discipline),
[[editor-inherited-handles-corrupt-gpp.md]] (handle-list requirement).

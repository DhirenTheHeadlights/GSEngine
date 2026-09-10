# Editor agent select API

Scope for giving agent-panel chats a shaped, queryable interface to editor and project data,
so that requests for data go through the editor instead of raw shell dumps. Scoped 2026-09-07.

## Why

Token accounting over every local chat (324 chats, 48.8k turns, `~/.claude/tools/token-usage.mjs`)
shows the bill is context re-read per turn: 16.5B cache-read tokens against 0.5M fresh input and
20M tokens of tool output. Mean context per turn was 344k over the last week. Two levers follow:

1. **Fewer turns.** Polling was 3% of shell calls. Fixed on the harness side: `sleep` is blocked
   by the build-guard hook, hibernate wakes the chat, `run_in_background` covers processes.
2. **Smaller context per turn.** Auto-compact window lowered from 980k to 300k in user settings
   (`CLAUDE_CODE_AUTO_COMPACT_WINDOW`). Beyond that, the remaining lever is what agents pull in:
   Bash is 54% of tool bytes and Read 30%, and much of it is logs, build output and trace dumps
   read in `sed -n` slices because there is no better door.

A select API attacks the second lever at the source and, made the only door, doubles as the
sandbox boundary. Headroom-style compression of raw output was considered and rejected as the
first step: it recovers ~20% on top of whatever the agent already dumped, whereas a shaped query
never produces the dump.

## Settled constraints

- The editor already drives `claude -p --output-format stream-json --input-format stream-json`
  per worktree (`Agent/Session.cppm`) and services build requests through a file inbox under
  `%LOCALAPPDATA%\GSE\cache\agent-build` (`BuildRunner/Inbox.cpp`). Attribution rides
  `CLAUDE_CODE_SESSION_ID`, which every tool shell carries.
- Agents run `--permission-mode auto`; there is no approve/deny surface. Anything that must be
  enforced has to be enforced by a hook or by not being reachable.
- The engine tree is shared by many concurrent chats. The API must be addressed to the editor
  that owns the caller's project, exactly as `gse-build --project` is.

## Shape

**Transport: MCP over stdio, served by `Tools/gse-mcp/server.mjs`.** An MCP stdio server has
to be a child of `claude`, and the editor is claude's parent, so the editor cannot be the server
directly. The server is a dependency-free node script; claude spawns it with
`CLAUDE_CODE_SESSION_ID` in its environment (verified in CLI 2.1.261), so every call is
attributed without editor plumbing. The editor registers it by passing `--mcp-config` in
`agent_command`; terminal sessions opt in with `claude --mcp-config Tools/gse-mcp/mcp.json`
from the engine root.

Data the server can reach from disk needs no editor endpoint: build and hibernate ride the
existing file inbox exactly as the shell scripts do, and log query reads the log files. A named
pipe to the owning editor is added only when a tool needs live editor state (scene query), and
until then nothing in the editor changes except the launch flag.

Why MCP rather than more shell scripts: tool schemas are in the model's context, results are
typed and bounded, and hooks can name the tools in allow/deny rules.

**Tools, first cut.** Six, each returning a bounded payload with a `truncated` flag and a cursor.

| tool | replaces | returns |
| --- | --- | --- |
| `gse_build` | `Tools/gse-build` | request id, then the result split by ownership, exactly what the script prints today |
| `gse_build_status` | polling the exe timestamp | state of the caller's request and the last build for this project |
| `gse_log_query` | `Read` on `Editor.log` / game logs | lines filtered by level, category, regex, time window, tail N; default 200 lines |
| `gse_trace_query` | `Read` on physics/training dumps | rows from a named trace by step range and column subset; aggregates (min/max/mean) on request |
| `gse_scene_query` | grepping scene files | an entity by name or id: transform, components, bounds; or a filtered list |
| `gse_hibernate` | `Tools/gse-hibernate` | same contract: register, end turn, editor wakes the chat |

Each tool takes an optional `project`; default is the caller's cwd resolved to a `.gseproj`,
matching the scripts. Every response header carries `agent`, `project`, `bytes`, `truncated`.

**Enforcement.** Once the tools exist, a PreToolUse hook denies `Read`, `Grep` and `Bash`
access to the paths the tools cover: `logs_dir()`, `captures_dir()`, trace output under
`.gse/data`, and the build directory. The denial message names the tool that replaces the read.
This is the same pattern as the build guard and the PATH strip: the direct route is closed and
the message points at the sanctioned one. Compilers stay off PATH; `gse_build` is the only build.

**Accounting.** The editor already parses `result.usage` per turn (`Session.cpp`). Add per-tool
bytes served and per-chat context size to the panel so the effect is visible without the
offline script, and so a chat that is dumping despite the tools shows up.

## Out of scope for the first cut

- Compression of served payloads. Shape first; measure; compress only what is still large.
- Write access through the API (scene edits, file writes). Agents keep `Edit`/`Write`; the API
  is a read and control surface.
- Cross-agent memory or transcript dedup. Different problem.
- Replacing the file inbox for builds. `gse_build` fronts it; the inbox stays as the queue.

## Order of work

1. **Done 2026-09-07.** `Tools/gse-mcp/server.mjs` with `gse_build`, `gse_build_status`,
   `gse_hibernate` wrapping the existing inbox, plus `gse_log_query` over
   `%LOCALAPPDATA%\GSE\logs\<exe>.<pid>.log` (newest run by default, structured level and
   category filters, bounded by `tail` and `max_bytes`). Protocol verified with a scripted
   JSON-RPC session. The editor passes `--mcp-config` at launch.
2. Measure with the usage script before and after on the HumanoidLocomotion chats.
3. **Trace query done 2026-09-08.** `gse_trace_query` streams a run's captured log
   (`.gse/data/eval/<run>.txt` plus `.partN` continuations, oldest first) and parses every
   line into numeric fields: ANSI stripped, unit suffixes removed, tuples split into
   `.x/.y/.z`, both `key=value` and `key value` shapes. `summary=true` lists line families
   and counts; queries filter by family prefix, regex, category and a per-family step key
   (`step`, `gen`, `it`, `update`, `total_steps`, `ep`), then return a bounded tail of rows,
   every Nth row, or count/min/max/mean per field with the step at each extreme. Transcripts
   showed agents reaching these files only through `cd && grep -a | tail`, which this
   replaces. The data guard now covers the `train_/smoke_/parity_/play_` families; redirect
   targets stay writable so the queue scripts keep working.
4. **Done for logs 2026-09-07.** `~/.claude/hooks/data-guard.mjs` denies Read, Grep, Glob,
   Bash and PowerShell access to `%LOCALAPPDATA%\GSE\logs` in GSE trees and names
   `gse_log_query` plus the terminal opt-in. Deletes and heredoc bodies are exempt. Extend its
   `covered` table as trace and scene tools land. Tests: `~/.claude/hooks/tests/data-guard.sh`.
5. **Panel accounting done 2026-09-08.** Per-chat context size was already tracked and drawn
   (`record_usage`, `draw_context_bar`), so the missing half was tool output: `record_tool_output`
   sums `tool_result` bytes per chat and `remember_tool_name` attributes the largest single
   result, shown in the info panel as `tool output: 1.2 MB · biggest 380 KB from Read`.
   The counters are `archive_skip`, so they are live-session only and need no sessions-version
   bump. A chat dumping despite the tools is now visible without the offline script.

## Scene query: dropped, and why

`gse_scene_query` was scoped as tool six, addressed to "grepping scene files" and expected to be
the first tool needing a named pipe. Two findings killed it on 2026-09-08.

**The editor holds no scene.** It is an IDE: `Editor/Editor/Source` has Analysis, Search,
Navigation, Diagnostic and Viewport, and no scene, world or inspector. The ECS registry lives in
the game process, which the editor launches as an attached child and talks to over the surface
pipe, and it only exists during a play session. A scene query would therefore need an endpoint
inside the engine runtime plus routing through the editor, not a pipe to the editor.

**Nothing is asking for it.** `~/.claude/tools/tool-bytes.mjs` attributes every tool result in
the local transcripts to a target class. Over 30 days, 29,747 results, 58.7 MB returned:

| target | bytes | share |
| --- | --- | --- |
| source: engine C++ | 29.59 MB | 50% |
| other bash | 5.70 MB | 10% |
| docs/markdown | 5.18 MB | 9% |
| grep over sources | 4.12 MB | 7% |
| git history/diffs | 3.72 MB | 6% |
| COVERED: gse logs | 2.02 MB | 3% |
| COVERED: run traces | 0.49 MB | 1% |

There is no scene-file traffic to replace. Half of everything is reading engine C++ source, at
15,283 calls averaging 1.9 KB, which is agents reading files and `sed -n` slices to find symbols.

**The tool that evidence argues for instead** is a symbol query over the code the editor already
indexes: `Analysis/CompilationDatabase`, `Analysis/SymbolExtract`, `Search/Index` and clangd.
"Give me the definition of `record_usage`" instead of reading 200 lines around a guess. That is
the only remaining lever of the size the log and trace tools were. Scoping it needs a look at
whether the index is queryable from disk or needs the editor endpoint that scene query would
have introduced.

## Symbol query

Built 2026-09-09 as tool six, in place of scene query.

**The index is not queryable from disk.** `Search/Index.cpp` does cache per-translation-unit
symbols under `config::cache_dir()/symbols/<hash>.bin`, but through the engine's reflection-driven
`binary_writer`, versioned by `tu_cache_version` and `archive_format_epoch`. A node reader of that
format would be coupled to a layout that changes whenever a reflected struct does. The live
in-memory index in the running editor is the only sound source, so this is the tool that needed
the editor endpoint scene query would have introduced.

**It rides the file inbox rather than a new pipe.** `build_inbox` grows a `queries` directory and
`peek_symbol_queries`/`consume_symbol_query`; answers go back through the existing
`build_inbox::publish`, so `Tools/gse-mcp` reads them with the same `read_result` it already uses
for builds. The search system owns the index, so it does the polling: `search_system::frame` calls
`search::poll_agent_queries` every 100 ms, which is one `directory_iterator` over a usually-empty
directory. Ownership is decided exactly as builds decide it — `config::owning_worktree(cwd)`, then
`project` against `config::project_root()`.

**What it answers.** `Search/AgentQuery.cpp`. A `name` (bare or qualified) is split on the last
`::` and ranked through the index's own `selection_score`, so the tool and go-to-definition agree
on what matches; every site comes back with kind, resolved type from `xref_at`, and its source
text. A `file` returns that file's outline, filtered to the kinds worth outlining (everything but
locals and parameters). Source text comes from the content index already in memory, sliced by
`definition_extent`: from the definition line to the line at the same indent that closes it, or to
the `;` of a wrapped declaration, capped by a line budget the earlier, better-ranked sites draw
from first.

**References, added 2026-09-10.** `references: true` on a `name` returns use sites instead of
declaration sites. The measurement asked for it: two days after the first cut, Bash was 57% of all
tool calls and 47% of those were grep, so "where is this used" was the biggest remaining reason to
shell out. The feared cost did not appear. The scan is linear over all 1.22 M xrefs, matching each
`def_file`/`def_line`/`def_column` against the resolved anchors, and lands at ~300 ms inside the
existing 100 ms poll on the frame thread, so no worker was needed.

Two things this gets right that grep cannot: uses resolve through aliases, and the name in a
comment, a string or on an unrelated same-named symbol is not a use. The index emits more than one
xref per token — one resolving to the declaration, one to the definition — so results are
deduplicated by (file, line, column); without that every count came back exactly doubled, which the
first live run caught by disagreeing with grep on `split_field` (8 against 4). A symbol with no uses
is an empty result, not a lookup failure: "nothing calls this" is an answer worth having, and
`lookup_failure` has no value for it that would not be a lie.

**No guard.** Unlike logs and traces, source reads stay open: the index is empty while it builds,
absent when no editor is running, and useless for free-text search. The tool has to win by being
better, not by the alternative being closed.

**Known duplication.** `peek_requests`, `peek_hibernations` and `peek_symbol_queries` now share a
directory-walk-and-age-out skeleton three ways. It should collapse into one helper over
`(dir, abandoned, parse)`; it was left alone because another session has `BuildRunner/Inbox.cpp`
open, and a behaviour-preserving refactor of a file that is mid-edit lands in everyone's build.

## Measurement

Baseline is captured: last 7 days, 12.5k turns, mean context 344k, tool bytes Bash 54% / Read
30%. After step 2, the same script on the same projects. Success for the first cut is Read+Bash
bytes on log and trace paths going to near zero and mean context per turn falling with them.
The compaction change lands first and separately, so its effect is not confused with the API's.

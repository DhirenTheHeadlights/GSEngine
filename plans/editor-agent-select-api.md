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
3. `gse_trace_query` and `gse_scene_query`; the latter is the first tool needing a pipe.
4. **Done for logs 2026-09-07.** `~/.claude/hooks/data-guard.mjs` denies Read, Grep, Glob,
   Bash and PowerShell access to `%LOCALAPPDATA%\GSE\logs` in GSE trees and names
   `gse_log_query` plus the terminal opt-in. Deletes and heredoc bodies are exempt. Extend its
   `covered` table as trace and scene tools land. Tests: `~/.claude/hooks/tests/data-guard.sh`.
5. Panel accounting.

## Measurement

Baseline is captured: last 7 days, 12.5k turns, mean context 344k, tool bytes Bash 54% / Read
30%. After step 2, the same script on the same projects. Success for the first cut is Read+Bash
bytes on log and trace paths going to near zero and mean context per turn falling with them.
The compaction change lands first and separately, so its effect is not confused with the API's.

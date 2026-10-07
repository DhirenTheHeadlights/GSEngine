# Agent Phase Plan

A chat in the editor is currently one undifferentiated conversation in one permission mode. This plan turns it into an explicit state machine of phases, each with its own permission mode, its own system prompt, and its own context lifetime, all rendered in a single editor session.

## Goal

One chat per task, as today. Inside it:

1. The agent always scopes before it edits, and waits for approval.
2. Scoping and reviewing are read-only at the process level, not by request.
3. The review is done by an agent that has not seen the implementation reasoning.
4. Every phase knows the workflow's standing rules without being told again.

## State Machine

```
scope --(approve)--> apply --(done)--> review --+--(findings)--> revise --> review
  ^                    ^                        |
  |                    |                        +--(clean)----> settled
  +--(revise scope)----+
```

| Phase | Tool surface | Context | Writes |
|-------|--------------|---------|--------|
| `scope` | read-only | fresh session | no tool can write |
| `apply` | full | resumes `scope` | allowed |
| `review` | read-only | **fresh session** | no tool can write |
| `revise` | full | resumes `apply` | allowed |
| `settled` | full | resumes, not relaunched | allowed |

Every phase launches in `auto`. A read-only phase is made read-only by removing the capability rather than by asking: `--disallowed-tools "Bash Edit Write NotebookEdit mcp__gse__gse_build"`. What remains — `Read`, `Grep`, `Glob`, and the editor's query tools — cannot mutate anything, so the restriction holds whatever the model decides to do.

`Bash` is on that list because it is a write path, not because exploration needs restricting: in practice a scoping agent's shell calls are `ls`, `sed -n`, `grep -n` and `find`, which are `Glob`, `Read` and `Grep` wearing shell syntax. Removing it also pushes lookups onto `gse_symbol_query`, which resolves through aliases and skips comments and strings. The real loss is `git` history; the diff itself is pre-captured for `review`, and a read-only git query tool would close the rest.

**Plan mode was the obvious first choice and it does not work.** It blocks MCP tool calls categorically — "Cannot call `mcp__gse__gse_phase_done` while in plan mode" — so a read-only phase could never report itself finished, and the state machine deadlocked at `scope`, which is every new chat. `--allowed-tools` does not rescue it: that flag pre-approves prompts, while this is a mode-level prohibition. Plan mode also permits `Bash` and so never delivered the read-only guarantee it appeared to. Do not reintroduce it here.

`--permission-prompts none` is likewise unnecessary. A chat launched by the editor has no approval surface, so anything that would prompt is already denied automatically.

`Agent` is on the list for a reason that is easy to miss: **subagents inherit their own tool sets, not their parent's.** The general-purpose agents hold every tool, and even the read-only-sounding ones keep `Bash`, so a read-only phase that can spawn a subagent can shell out in one call. Denying the four mutating tools while leaving `Agent` reachable is not a restriction, it is a detour sign. If subagent fan-out is ever wanted back in a read-only phase, it has to come through `--agents` with an explicitly restricted tool set, not by re-allowing `Agent`.

A denylist is open-ended by construction: it names the mutating tools that exist today, and a new one would not be on it. `--allowed-tools` cannot be used as a strict allowlist, so this is the available mechanism — when a tool that can write is added, it belongs on that list. The test for membership is not "does this edit files" but "can this reach a tool that does".

`settled` is a terminal state rather than a mode: the process is stopped instead of relaunched, and the permissive surface applies only if the owner reopens the chat and talks to it again.

`apply` resumes the `scope` session because the plan is the thing being implemented. `review` deliberately does *not* resume anything: fresh eyes, no sunk cost in the design it is judging. `revise` resumes `apply`, which still holds every reason the code looks the way it does.

## Phase Runs

A session stops owning a single `agent_id` and starts owning a list of runs:

```cpp
enum class task_phase : std::uint8_t {
	scope [[= phase_style{ .label = "scoping", .color = &gui::style::color_folder }]],
	apply [[= phase_style{ .label = "applying", .color = &gui::style::color_added }]],
	review [[= phase_style{ .label = "reviewing", .color = &gui::style::color_accent }]],
	revise [[= phase_style{ .label = "revising", .color = &gui::style::color_warning }]],
	settled [[= phase_style{ .label = "settled", .color = &gui::style::color_text_disabled }]],
};

struct phase_run {
	task_phase phase = task_phase::scope;
	std::string agent_id;
	std::uint32_t first_row = 0;
	std::int64_t started = 0;
};
```

`session` gains `std::vector<phase_run> runs`, plus `std::optional<phase_gate> gate` for a transition awaiting approval and `std::optional<phase_gate> pending_transition` for one that is granted but not yet performed. The current phase is `runs.back().phase` rather than a field of its own, so the two cannot disagree; a session with no runs reads as `apply`, which is exactly how chats behaved before phases existed, so archives written at version 5 keep working without migration.

`agent_id` in `session_info` stays the id of the *running* process — the CLI reports it, so it is the authority — and `track_run_id` copies it onto the current run as history. `sessions_version` goes 5 to 6.

`rows` stays one flat list. Phase boundaries are markers into it, so the transcript reads as one continuous task history even though three separate claude sessions produced it.

## Transitions

A transition is requested by the model and granted by the editor. The model cannot move itself.

`gse_phase_done({ summary, findings? })` in `Tools/gse-mcp/server.mjs` rides the same file inbox as `gse_build` and `gse_hibernate`: the tool writes a report under `phases/`, `accept_phase_reports` picks it up in the editor's own tick, and the reply comes back through `build_inbox::publish` as the tool's result. No new endpoint, and the existing `gse_hibernate` flow is the working precedent for an agent telling the editor to change its state.

A granted transition is **not** performed immediately. Relaunching while the agent is mid-turn would kill it inside a tool call and leave a resumed session with a truncated `tool_use` block, so the editor records `pending_transition` and `service_phase` performs it on the first tick where the turn has ended. The tool result tells the agent to end its turn, and the phase changes as soon as it does.

This is why the transition signal is an MCP tool rather than `ExitPlanMode`: answering `ExitPlanMode` properly means implementing the stream-json control protocol as a permission host, and the gate belongs in the editor's own UI either way.

Gates:

- **scope to apply** is the only manual gate, and it is the only one that should be: it is where the owner is deciding what gets built. Everything after it runs without them.

  The Approve button appears when the agent reports, and survives until it is used — replying with notes does not consume it. An earlier version cleared the gate on any user message, treating a reply as "dismiss", which meant the owner's reply destroyed the only control that could advance the phase while the agent believed it must not report twice. The gate object carries the agent's summary; it does not carry the authority to advance.
- **apply to review** is automatic once the agent reports done. No reason to ask.
- **review to revise or settled** is automatic on the findings count.
- **revise to review** is automatic, and re-enters a *new* fresh review run, not the previous one.

Every report writes a `row_kind::phase` row carrying its summary as `detail`, gated or not, so an automatic transition still shows the owner what was reported rather than silently moving on.

The review loop needs a bound, and the first version did not have one. `review → revise` fired on any non-zero findings, `revise → review` always fired, neither edge was gated, and the exit required a reviewer to volunteer zero. But every review is a *fresh* agent holding a guide that mandates broad coverage, with no memory of what earlier passes raised or what was already declined — so it will nearly always find something, and zero was never realistically reachable. One task spent six unattended hours and seventeen review sessions in that loop before anyone looked.

Two things bound it now. `max_review_cycles` settles the task regardless after a few passes, logging a warning and marking the transcript when findings are still open — a hard bound that does not depend on anyone's judgement, and the reason the loop can safely run unattended at all. The charters carry the convergence rule: a finding declined in a revise summary is settled, zero is the expected outcome of a competent change rather than a failure of diligence, and `findings` counts defects that would change the code, not observations.

Gating `review` was the first reflex and it was the wrong instrument. It would have stopped the runaway by stopping *everything*, putting a click between the owner and every pass of a loop whose whole value is running unattended once the scope is agreed. A bound belongs in the machine, not in the owner's attention. The cap is what makes the loop safe; the gate would only have made it slow.

Executing a transition is the existing restart path: set the next phase, clear `agent_id` for fresh-context phases, call `restart_session`. For `scope` to `apply` the id is kept, so `--resume` carries the plan over.

`review` is additionally marked `side_thread`: its conversation is a throwaway branch off the task's main line, so `resume_target` skips it when finding the thread to continue. Without that, `revise` resumed the *reviewer* rather than the implementer — the review session file ends up carrying the revise opening — and a review that reported nothing produced a revise agent told to "address the review findings" with nothing in its context to address. `fresh_context` could not express this on its own: `scope` also starts fresh, yet `apply` must resume it.

Each transition also hands the reporting phase's summary to the next one *in the opening message*, not only as a file. The artifacts are listed in the system prompt, but a resumed session carrying a long prior conversation will not reliably go and read one; the findings have to arrive as the instruction itself.

The relaunched process then has to be *spoken to*. A `claude -p` session blocks on stdin and does nothing until it receives a message, so a transition that only restarts leaves the new phase sitting idle with the pipeline showing the right station and no work happening. Each phase carries a short `opening` on its policy annotation — "The scope is approved. Implement it now" — which `begin_phase` sends through `send_to_session` once the process is up. `scope` has none: the chat enters it at creation and the owner's own first message starts it, and `settled` has none because it is not relaunched at all.

## Handoff Artifacts

Fresh-context phases need the facts without the conversation. At each transition `capture_handoff` writes, under `project_state_dir()/agent/<session id>`:

- `scope.md` - the approved plan, when the phase being left is `scope`.
- `report.md` - what the ending phase reported, for every other phase.
- `diff.patch` - `git --no-optional-locks diff HEAD`, captured through `process::run_capture`.
- `touched.txt` - the files this chat wrote, derived from the transcript's tool rows, which is where that fact already lives (`session::touched` keys by build id and does not retain paths).

`write_phase_prompt` lists whichever of these exist, so the prompt never names a file that is not there. The reviewer reads the diff and the stated intent, nothing else.

`run_capture` is synchronous and runs on the editor's update tier, bounded at ten seconds. A phase transition happens a handful of times per task and `git diff` on this repo is well under a second, so this is a deliberate simplification rather than an oversight — if it ever hitches, the fix is to move it onto a worker through `task::pending`, not to cache it.

## Phase Prompts

`session_command` gains `--append-system-prompt-file <path>`. The editor regenerates the file on every launch, so it can include live state. Each file is the standing rules plus the phase's own charter.

Phase charters live in `docs/agent/<phase>.md`, keyed by the enumerator's own spelling through `enum_to_string`, so there is no filename to keep in sync and the prompts can be tuned without rebuilding the editor. The policy that cannot be derived from a name — label, colour, permission mode, whether the phase starts fresh, whether it is gated — is a `phase_policy` annotation on the enumerators, read with `annotation_from_enum`.

The standing rules land in `docs/AGENT_WORKFLOW.md`, which the generator inlines:

1. You are editing inside the editor that is running you, not driving a CLI from outside. Paths, builds, and logs are all local to this tree.
2. You may queue builds and run the executable freely. A running exe never blocks a build - the binary is renamed before the new one is linked. Do not refuse, defer, or warn about this.
3. Build errors are attributed back to you and arrive as messages. You do not need to pre-emptively verify that someone else's work still compiles.
4. Other agents are working in other trees. `build_hold::tree_busy` and the presence list are informational; a busy tree is a wait, not a failure.
5. Plan and review against `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md`. For renderer or shader work add `docs/GRAPHICS_REVIEW.md`.

The generator appends the current presence list, so each agent knows who else is live and where without asking.

Phase charters, in brief:

- `scope`: understand the change against the codebase as a whole; name the systems and ownership it touches; give options with tradeoffs where the design is genuinely open; state what the review guide will demand of this change *before* it is written. End with `gse_phase_done`.
- `apply`: implement the approved scope. Deviations from it are reported, not absorbed.
- `review`: you did not write this code. Apply the review guide in its stated order against `diff.patch`. Report findings with file and line, most severe first. Fix nothing.
- `revise`: address the findings. Push back on any that are wrong rather than complying mechanically.

## UI

`draw_phase` owns a one-line strip between the activity line and the composer, always present for the shown session: the pipeline in its policy colours, and when a gate is pending, what was reported. It is pure status and draws no widget. The full summary is not repeated there — it rides the transcript note as `detail`, where the existing group and diff rendering already handles long text.

The approval control lives in the composer row, taking the stop button's slot while a gate is pending — the two are never both meaningful, since a gated chat has ended its turn and has nothing to interrupt. Pressing it hands whatever is in the message box to the next phase as the owner's note, and clears the box. That makes the common case one gesture: read the plan, type the one thing you want changed, approve. The note is delivered after the previous phase's summary and marked as taking precedence, so a refinement overrides the plan it is attached to rather than competing with it.

Nothing here adds a window. One session, one tab, phases inside it.

## Persistence

Phase state is not settings, and does not go through `save::registry`. It rides the agent module's own `binary_writer` archive (`agent_sessions.bin`) alongside the sessions it belongs to, because it is per-chat history rather than configuration — `settings::category<"Agent">` continues to own only `default_model` and `default_effort`.

What that archive did not have was a cadence. `save_sessions` was called from `shutdown` and nowhere else, which was survivable when a session was just a transcript and a model id, and is not once a chat carries a state machine: an ungraceful exit lost every transition since startup, and the reload was silent — `runs` comes back empty, `current_phase` falls to its `apply` default, and a task that was mid-review resumes as an ordinary unphased chat with nothing to indicate anything was lost.

`accept_phase_reports` and `service_phase` now report whether they changed anything, and the system saves when either did. Those are the state machine's own checkpoints — a report recorded, a gate raised, a transition performed — so the archive is durable at exactly the points where it became expensive to lose, without a timer or a dirty flag. Chat creation and ordinary messages still ride the shutdown save, as before.

## Still Open

- **Transcript rows do not survive a restart.** `session::rows` is `archive_skip`, and `hydrate_session` restores from the current `agent_id`'s transcript only, so a reopened multi-phase chat shows the last phase rather than all of them. `runs` persists, so the phase itself is correct. Fixing it means archiving rows or hydrating across every run's transcript.
- **No phase dividers in the transcript.** `phase_run::first_row` is recorded for exactly this and nothing reads it yet.
- **The tab strip does not badge the phase**, so a chat waiting on approval is only visible once it is selected.

## Tradeoffs

- **Relaunch per transition** costs a process start and loses the CLI's warm prompt cache for fresh phases. Resumed phases keep theirs. Acceptable: transitions are rare relative to turns.
- **Fresh review context** means the reviewer will occasionally flag something the scope deliberately decided. That is the cost of fresh eyes, and `revise` lets the implementer push back rather than comply.
- **A read-only phase denies hard.** One that genuinely needs a write surfaces as a `row_kind::denial`, not a stall. This is the intended failure: the phase reports what it could not determine and the next phase finds out.
- **Hooks are the alternative enforcement**, reading phase state from a file instead of relying on launch flags. Not needed while the editor owns the launch, which it does; worth revisiting only if phases ever need to change without a relaunch.

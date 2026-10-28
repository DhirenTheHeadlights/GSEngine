# Agent chrome: every cell and every event

The chat body's chrome comes from one `chrome_plan`, built by `plan_chrome(data, session)` in
`Editor/Editor/Source/Agent/Chrome.cpp`. Reachability comes from `excluded_by(kind, phase, state)`
in the same file, and the `chrome_gallery` drawable (`gse_draw` `{"draw": "chrome_gallery",
"kind": "<chat_kind>"}`) draws every cell that predicate admits, using the real `plan_from` and
`draw_status_row`. This document, the predicate and the gallery are meant to agree. If they
disagree, the predicate is the source of truth.

The enums are `chat_kind`, `task_phase` and `agent_state` in `Agent/Model.cppm`.

- 5 kinds: chat, goal, crash, build_error, sync.
- 7 phases: scope, apply, review, revise, summarize, settled, steer.
- 12 states: resting, failed, idle, working, editing, build_queued, compiling, hibernating,
  awaiting_approval, awaiting_goal, awaiting_answer, rate_limited.

## Exclusion rules

Each unreachable combination is listed here once. Any `(kind, phase, state)` not excluded by one
of these rules is reachable.

| # | rule | excludes |
|---|------|----------|
| R1 | A sync chat never begins a phase, so `current_phase` reads `apply` for it. | sync × every phase except apply |
| R2 | `steer` is entered only by a chat with a goal. | chat, crash, build_error, sync × steer |
| R3 | `awaiting_goal` needs a goal's await (`state_of` requires `s.goal->waiting`). | chat, crash, build_error, sync × awaiting_goal |
| R4 | A read-only phase cannot write, so `editing` cannot occur. | every kind × scope, review, summarize × editing |
| R5 | A goal chat gates only on an escalation, a pause or a spent steer budget. All of those are reported from steer or summarize (`needs_owner`, `wake_goal`, `pause_goal`). | goal × scope, apply, review, revise, settled × awaiting_approval |
| R6 | Without a goal, only a gated phase (`phase_policy::gated`, i.e. scope) of an attended chat (`!kind_style::unattended`) waits for approval. | chat, build_error × every phase but scope × awaiting_approval; crash, sync × every phase × awaiting_approval |

## Reachable cells

Reachable states per `(kind, phase)` cell are listed below. The states not named for a cell are
excluded by the rule cited for it. "all" means all 12 states. `failed` is reachable wherever `resting` is: both are the `!s.running` branch of `state_of`, split only by `info.failure`.

| kind | scope | apply | review | revise | summarize | settled | steer |
|------|-------|-------|--------|--------|-----------|---------|-------|
| chat | all but editing (R4), awaiting_goal (R3) | all but awaiting_approval (R6), awaiting_goal (R3) | as scope, minus awaiting_approval (R6) | as apply | as review | as apply | none (R2) |
| goal | all but editing (R4), awaiting_approval (R5) | all but awaiting_approval (R5) | all but editing (R4), awaiting_approval (R5) | all but awaiting_approval (R5) | all but editing (R4) | all but awaiting_approval (R5) | all |
| crash | all but editing (R4), awaiting_approval (R6), awaiting_goal (R3) | all but awaiting_approval, awaiting_goal | as scope | as apply | as scope | as apply | none (R2) |
| build_error | same as chat | same as chat | same as chat | same as chat | same as chat | same as chat | none (R2) |
| sync | none (R1) | all but awaiting_approval (R6), awaiting_goal (R3) | none (R1) | none (R1) | none (R1) | none (R1) | none (R1) |

### What each reachable cell shows

The chrome is a function of the state, the phase and the kind. It does not depend on the cell as
a whole. So the cell table above, combined with the two tables below, gives every cell's chrome.

**Status row.** This is one row in the chat body, `draw_status`.

| element | shown when | content |
|---------|-----------|---------|
| phase route | always, except sync (no route) | `phase_timeline` stations scope, apply, review, revise, summarize, plus steer only for a goal chat. Never settled. The current station is coloured by `phase_policy::color` and underlined (`animated_rect`). Visited stations are secondary, unvisited ones disabled. A station visited more than once is labelled `xN`. A settled chat has no current station, so no underline. |
| note | `s.gate` (state awaiting_approval, or any state while a gate is held) | "reported done" or "reported N finding(s)" |
| note | state failed | `info.failure` |
| note | `s.pending_transition` | "approved · finishing the turn before <next phase label>" |
| activity | busy (working, editing, or a build state while the turn is open) | spinner + `s.action` (`tool_flavor` verb, or "thinking") + elapsed time, both drawn in `state_style::activity`: accent while working, the added colour while editing. While a build is queued or running it shows `status_label` instead of `s.action`. It fades in and out through `presence` on the plan's key. |

**Question block.** `draw_question`, above the input, shown while `chrome_plan::question_shown`
(state awaiting_answer).

| element | content |
|---------|---------|
| head | the question, wrapped to at most 4 lines (2 when the options do not fit) |
| hint | "Pick one / Pick any that apply, or type instead" · the question's context |
| options | one pressable row per option with a checkbox; single-choice clears the others |
| overflow line | "N more option(s) do not fit here – they are in the transcript above, and the box answers in your own words", when the options exceed 60% of the body |

Its height is eased through `animated_rect` on the session's key: it grows in when the question
arrives and shrinks out once it is answered, and the transcript gets the space back only when the
shrink ends. The question is already cleared by then, so the leaving block is drawn as its
background only. Options are not pressable until the block has finished growing. Switching to a
tab that already has a question shows the block at full height, with no replay.

**Scope plan block.** Shown when a scope gate carries a summary or `steps` from `gse_phase_done`
(`chrome_plan::plan_shown`), above the question slot. It has two pages. Page 1 is the summary,
rendered as markdown through the transcript's own `push_transcript_line`, in chat-text colour, and
scrolling when long. Page 2 is the steps/files view below. `<` / `>` and a "1/2" label sit top
right; they are inert until the block has grown, like Revise and Approve, which stay under both
pages. A new gate opens on page 1. With no `steps` only the summary page exists and the
navigation is not drawn. A page switch eases the block's height (`animated_rect`) and fades the
incoming page in (`presence`). `gse_panel_query` Agent reports it in `plans` as `gate_page`.
In the **Summary** view this block is not drawn above the input, because the lifecycle pages
below carry the same pages and buttons; it is drawn in Transcript and Everything.

**Summary view = the lifecycle summary.** The Summary view draws no transcript. It fills the
transcript area with one page set per stage on the chat's route (`phase_timeline`), paged with the
same `<` / `>` and "k/N" label (`lifecycle_of`, `lifecycle_pages`, `draw_lifecycle`):

- **scoping** contributes its summary page and, when the plan carried `steps`, its steps/files
  page. The content is the pending scope gate's while one is held, otherwise the latest scope run
  of the current task (`phase_run::summary` and `phase_run::steps`, both archived), so it stays
  after approval and across a restart. Revise and Approve appear on these pages only while the
  scope gate is held.
- **applying** contributes one page (`page_kind::changes`) when the approved scope has steps or
  the run has edits. On the left are the steps with their edit counts, plus an "unattributed"
  row when edits landed with no step begun. Once the agent has reported through `gse_step`
  (`docs/agent/apply.md` tells it to), each step is marked `[x]` done, `[>]` current or `[ ]`
  remaining, coloured by `step_style` (the selected row keeps the selection colour), and the
  head reads "k/N done · M edits". Before the first report the rows carry no mark and the head
  reads "unreported - the agent has not called gse_step · M edits". On the right are the diffs
  of the selected row, drawn by the transcript's own `push_diff` (and scrolled sideways by the same `update_diff_scroll` as the Diffs view) into a scrolling read-only text
  area and re-synced every frame, so they update live. With no row selected the right side
  shows every edit of the task. The selection is shared with the scope steps page.
  Once the run has **ended** (a later phase has begun, or the page belongs to an earlier cycle),
  nothing reads as live: every step the agent did not mark done, including a leftover current
  step, shows `[-]` "not reported" in `color_text_disabled`, edit counts still shown, and the
  head reads "apply · ended · k/N marked · M edits" (or "apply · ended · unreported · M edits").
  The marks are derived in `progress_of` and never stored. The panel query reports `run ended|live`.

  With neither steps nor edits, applying shows the
  empty page. States live on the apply run (`phase_run::progress`, archived). A revise→apply
  cycle starts from the previous apply run's states. Each edit is recorded live in `append_row`
  as a `step_edit` on the apply run (archived), and is tied to whichever step was current. A
  transcript rebuilt on load does not pass through `append_row`, so old edits are never
  re-attributed.
- **review and revise** share one page, `review:review_loop`, once the task has had a review run.
  It sits at the review station; the revise station has no page of its own. The head reads
  "review loop · cycle k of 3 · N open", where N is the last review's findings. The body is the
  summary page's scrolling markdown area: for each cycle, oldest first, a "review k · N findings ·
  <outcome>" heading followed by that review's reported summary, then, if revise began, a
  "revise k · M edits · Diffs › revise k" heading (or "in progress") followed by the revise summary.
  The outcome is reviewing, clean, sent to revise, awaiting revise, or left open at the cap. A
  loop stopped by `max_review_cycles` with findings open ends with "review limit reached - N
  finding(s) left open". Everything comes from `phase_run` (summary, findings, first_row); a
  revise's edit count is the number of `row_filter::changes` rows between its `first_row` and the
  next run's. Findings are the review's prose, not a structured list. Before any review run, the
  review and revise stations each show the empty page.
- **Diffs sections.** In the Diffs view the first edit of each apply or revise run is preceded by
  a secondary-coloured header line, "apply" or "revise k" (`run_section_of`), so revise edits read
  apart from apply's. The apply page's step diffs are unchanged.
- **summarize** has one page, `summarize:report`, once the last summarize run reported a
  summary (the text the handoff writes to `report.md`). The head is one strip: "summarize · report
  · steps k/N · review C cycles, F open" (`report_head`). The steps clause is omitted with no scope
  steps and reads "steps unreported" when `gse_step` was never called; the review clause is omitted
  with no review run; F is the last review's findings only when the loop stopped at the cap. Beside
  it, in accent, "M edits" (apply edits plus every revise's edits, `task_edits`) is pressable and
  opens Diffs. The body is the report in the summary page's coloured-markdown area. Before the
  summarize run reports, the station shows the empty page.
- **steer** (goal chats only) has one page per cycle, `steer:verdict`, once that cycle's steering
  run reported a verdict: head "steer · <date> · <verdict>" (`ledger_head`), body the run's summary
  as markdown. Before a verdict the station shows the empty page.
- **goal chats page by cycle.** A goal chat's Summary is not one task's route. `goal_cycles` splits
  `s.runs` at every scope run, so a cycle is scope → apply → review/revise → summarize → steer
  (a leading steer run with no scope before it is its own cycle). The head's right carries
  `<<` "cycle k of N" `>>` (cycle) beside `<` "i/M" `>` (stage); position 0 is "goal", the goal
  scope page below. The newest cycle follows live (`lifecycle_cycle` unset); a past cycle has no
  current station, and no gate, fix or approve button. A stage page whose run began after the
  last visit carries " · new" in its head. Older chats get every cycle; a cycle whose steering run
  recorded no verdict simply has no `steer:verdict` page, and `cycles` in the query says
  "none recorded".
- **since you were here.** `session::seen` (archived) is stamped every frame the chat is the
  active tab (`note_visits`). On arriving at a goal chat the view jumps to the first stage that began
  after the previous `seen`, and a warning-tinted banner above the page reads "since you were here
  (<UTC time>): N stages run · V steering verdicts · 1 question waiting · F failures ·
  waiting on you" (`digest_line`). The digest counts only what was stamped while the owner was
  away: after the previous visit's last `seen` (`arrived`) and no later than the moment this visit
  began (`entered`), so nothing that happens during the visit enters it. The owner's own answers
  are never counted; the question clause appears only for a question asked while away that is
  still unanswered. An empty digest draws no banner. **Catch up** jumps to the newest cycle's last page and dismisses
  it; paging to the newest cycle with `>>` dismisses it too; it shrinks out through `presence`.
  Builds are not in the digest: blame and fix records carry no time stamp.
- **ask about this stage.** Below any goal-chat stage page that has a run, "Ask about this stage"
  opens a line input with Ask and Close; disabled ("wait for the turn to end") while the chat's turn
  is open (`chrome_plan::fork_allowed`). Ask launches the existing inquiry command
  (`claude -p --tools "Read Grep Glob" --strict-mcp-config --no-session-persistence --fork-session
  --resume <agent_id>`): no edit, write or shell tools and no MCP server, so it cannot build or
  change anything, and the fork is not persisted. The prompt names the cycle and stage, its start
  time and its report. The fork resumes the session as it is now, not as it was at that stage, so
  the prompt says which stage to answer about. The answer replaces the input as markdown in a
  strip under the page, labelled "answer about <cycle · stage> · read-only fork"; Close ends the
  process and discards it. One fork per chat; closing the chat ends it.
- **goal scope** (goal chats, position 0) is `scope:goal`, the goal dashboard (`goal_dashboard_of`),
  formerly the steer station's page.
  The head is the identity strip: "goal · <standing> · N steers left · <record path>" (`goal_head`).
  Standing is a `goal_standing`: pursuing; waiting on <await_label>; waiting for your approval
  (a gate from steer, which covers both an escalation and the owner's pause, as neither carries a
  flag that tells them apart); steer budget spent (that gate with 0 steers left); and met (a met
  verdict is pending). The left column, in rows: "aim · <first line under the record's `## Aim`>"
  (read when each steering run begins, kept in `goal_state::aim`); the criteria tally
  "criteria · N met · N owner · N open"; one row per criterion in its `criterion_style` colour
  ("[x]" met, "[?]" owner, "[ ]" open, then the id and note); and the route of the current task
  ("task › steering ✓ › [scoping] › applying …", `route_line`). Rows that do not fit are counted in
  an overflow line. The right column is the ledger, newest first: one entry per steer run that
  reported a verdict, a head "YYYY-MM-DD · task|met|escalate" in the verdict's colour
  (`verdict_policy::color`), then that run's summary as markdown. The criteria come only from the
  latest steering run's `criteria` on `gse_phase_done` (`goal_state::criteria`); with none, the
  tally reads "criteria not reported". Nothing is parsed from the record except the aim line.
- **every other stage** has one empty page reading "<stage> · no screen for this stage yet".
- **Summary is the default.** `session::mode` is not archived, so every chat opens in Summary,
  existing ones included. Switching to Transcript lasts until the editor restarts. The saved
  page is clamped to the current page count every frame, so `<` never acts on a page that no
  longer exists.
- A chat with no scope run (a plain chat, or one scoped before steps were persisted) shows the
  scope summary page only if a summary exists, else an empty scoping page.
- The chosen page (`session::lifecycle_page`) is not reset by a new gate, so the view stays where
  the owner left it.

`gse_panel_query` Agent's `plans` reports, for every chat, `pages` (each `stage:kind`), `page`
(`k/N stage:kind`), `scope_steps`, `progress` (reported/unreported), and `gate_page` when a
scope gate is held. Its `progress` section lists, for every chat with step reports or apply
edits, each step's `state`, `edits` and attributed `files`, plus an `unattributed` item.

The old digest (Summary filter `row_filter::digest`) is deleted. Every row it showed is a
non-tool row, so it is in **Transcript**: user messages, failures, gaps, questions, canvases, and
phase rows with their full summary detail. The only thing it drew that Transcript did not was the
phase's short "record" title ("Scope", "Implementation", …) in place of the phase row's outcome
text; that is wording, not a fact, and the outcome text it replaced is still shown.

| element | content |
|---------|---------|
| head | "Plan · N steps · M files" |
| steps column | one pressable row per step, "1. <text>", wrapped to at most 3 lines; a click toggles the selection |
| files column | the union of every step's files in first-seen order, in the code font |
| link | hover a step (or select it, when nothing is hovered) and its files ease to the accent colour while a line from the step to each file fades in (`animated_color`, `presence`) |
| overflow line | "N more step(s) and M more file(s) do not fit here – the summary above has them all", when the block would exceed 60% of the body |
| Revise | sends the draft to the scoper, the same as pressing Enter; disabled while the draft is empty |
| approve | **`gate_label`**; the input's own approve button is not drawn while the block is shown |

Height eases through `animated_rect` like the question block. Rows and buttons are inert until
it has finished growing.

**Input action button.** `draw_input`, from `chrome_action`.

| state / condition | action |
|-------------------|--------|
| awaiting_answer | **Answer**, enabled once a draft or an option is chosen |
| gate held, no pending transition | **`gate_label`**: "Start applying", "Continue goal" or "Approve goal" |
| busy | stop, interrupts the turn |
| goal waiting (awaiting_goal), not busy | stop, pauses the goal |
| otherwise | stop, disabled |

**State text and colour.** Shown in the overview row, the info popover's "activity" row and the
gallery: `state_style::label` and `state_style::color`, plus `status_label`'s clauses and the build clauses `build_status` adds in the plan.

| state | label | clauses |
|-------|-------|---------|
| resting | ready (secondary colour) – the process ended between turns; submit relaunches it | holding N builds |
| failed | stopped (error colour) – the process ended with `info.failure` set | holding N builds |
| idle | ready | holding N builds |
| working | working | holding N builds |
| editing | editing files | holding N builds |
| build_queued | build queued | the reported `hold_style::brief`: "behind a running build", "until the editor restarts", or "behind '<blocker>'" (`names_blocker`); holding N builds |
| compiling | compiling | holding N builds |
| hibernating | hibernating | · until <unbuilt> is built (or · until the next build); holding N builds. Note: <asleep_blocker> |
| awaiting_approval | waiting for approval | holding N builds |
| awaiting_goal | waiting on the goal | · pid N to exit; · '<file>' to change; · wakes within N min; · capped at 48h when the requested wait was cut |
| awaiting_answer | waiting on your answer | holding N builds |
| rate_limited | usage limit reached | – resuming in Nm |

**Status note.** One note on the status row, chosen as the first `chrome_note` enumerator
whose `note_style::text` is non-empty, drawn in `note_style::color`. Priority is the enum order:

| note | text | colour |
|------|------|--------|
| failure | `info.failure` (state failed) | error |
| pending | approved · finishing the turn before <next> | secondary |
| findings | reported done / reported N finding(s) | secondary |
| blame | broke <blame_label> | error |
| unclaimed | build red · N error(s) nobody claims (chats in `unclaimed_tree`) | warning |
| unbuilt | NOT in the exe: <unbuilt> (stale) | warning |

**Overview footer.** Model, then `chrome_plan::footer`: the unbuilt text when stale, then
"broke <blame_label>". Coloured from the plan's tab flags: error, else warning, else secondary.

**Kind flavour.** Kind changes only the icon and its colour (`kind_style`) in the tab strip and
the overview, apart from two exceptions:
- sync replaces the status row's route with its own header (`draw_sync_header`: "Syncing <root> · <branch>" and **Cancel**), and shows the status row only while busy;
- crash is unattended, which shows up only as R6.
- build_error has its own Summary page, `apply:errors`, placed first at the apply station, ahead of
  apply's changes page. `fix_unclaimed` archives the unclaimed errors and the chats that denied them
  on the session (`session::fix`, a `build_fix`) when it opens the chat. The page draws them as a
  strip of code-face lines inside a red-tinted block: `file:line  message` per error in the error
  colour, with notes indented in secondary; a file-less error shows its message, then the
  build-output tail; the strip ends with "asked and denied: …". Below the strip is the agent's
  `diagnosis` as chat markdown, or a secondary "no diagnosis yet" line until one arrives. The head
  reads "apply · build error · N errors · diagnosed / no diagnosis yet". A build_error chat opened
  before this page existed has no `fix`, and its strip says the errors were not recorded.

**Tab flags.** `chrome_tab`:
- dirty: hibernating, a gate held, a question asked, or a build waiting;
- busy: is_busy;
- warning: stale;
- error: blamed errors or `info.failure`;
- dimmed: state failed. A resting chat is not dimmed.

## Events and their current presentation

Read from source as of this change. Where a row says "transcript row", the event appends a
`row_kind::phase` / `note` / `failure` row.

| event | presentation today |
|-------|--------------------|
| turn open | activity fades in on the status row; tab busy dot |
| turn close | activity fades out; tab busy clears |
| tool call | activity text becomes the `tool_flavor` verb. The tool row and its output appear only in the **Everything** view; Summary and Transcript leave them out, and writes are also in Diffs |
| first write of a turn | state working → editing; spinner and activity text turn from accent to the added colour |
| build queued | status and activity text become "build queued <hold brief>"; tab dirty |
| build_hold building | queue row "the editor is already running a build"; chat "build queued behind a running build" |
| build_hold editor_restart | queue row "the editor is rebuilding itself…"; chat "build queued until the editor restarts" |
| build_hold tree_busy | queue row "held until '<blocker>' finishes editing"; chat "build queued behind '<blocker>'"; blocker "· holding N builds" |
| build start | state compiling |
| build green | compiling clears; the unbuilt note and footer clear; "in the current exe" in the popover's "my edits" |
| build red, errors attributed | status note "broke <blame_label>" in the error colour; tab error; overview footer "broke <blame_label>" in the error colour |
| build red, errors unclaimed | inquiry forks; status note "build red · N error(s) nobody claims" on every chat in that tree; popover "unclaimed" row |
| phase reported (`gate`) | transcript row "<phase> reported done/N finding(s)"; note on the status row if gated |
| scope approved | the gate block leaves; the Summary view's scoping pages stay, read from the scope run, without Revise/Approve |
| gate needing the owner, scope with `steps` | the scope plan block grows in, with **Revise** and the `gate_label` button; the input's approve button is not drawn; note, state and tab as below |
| owner follows up while the scope gate is held | the split opens: a follow-up column slides in from the right (the stored `plan_followup` split, default 38% of the width, eased by `animated_rect`, snapping to the cursor while its divider is dragged) holding the conversation from the row where the gate was first raised; the plan narrows to the rest. In Transcript mode the gate block also grows to `plan_split_share` of the body. Query: `plans[].split on` |
| scope re-reports while its gate is held | the plan updates in place and the split stays open; `first_row` is kept so the column keeps the whole follow-up exchange |
| scope gate released (approve or the phase moving on) | the split closes: the column eases back to zero width and the plan widens |
| gate needing the owner | **Start applying / Continue goal / Approve goal** button (`gate_label`) in place of stop; note "reported done" or "reported N finding(s)"; state awaiting_approval; tab dirty. Model and effort stay, since they apply at the next launch. |
| auto-advance | no gate, so no button and no note; transcript row; route underline glides to the next station |
| `pending_transition` | note "approved · finishing the turn before <next phase>"; the approve button goes and stop returns (interrupts the finishing turn) |
| phase begun on fresh context | transcript phase row; route station current |
| review findings > 0 → revise | route shows revise current, review `xN` |
| review cap reached | transcript row "review limit reached – N finding(s) left open" |
| question asked | question block grows in above the input; **Answer** in place of stop, enabled once a draft or an option is chosen; state awaiting_answer; Agents tab dirty; the echoed "asking you: …" row (question + options) is shown only in the Everything view |
| question answered | block shrinks out and the transcript reclaims its space when the shrink ends; Answer returns to stop; an answered card (owner bubble) is appended: the question, every option with the picks marked `[x]` in accent, then any typed steering. The full prompt sent to the model is not drawn; it lives only in the agent's own session log. Picks and steering are archived on the row (`transcript_row::options`, `detail`) and reported by `gse_panel_query` Agent under `answers` |
| gap reported | transcript row only |
| hibernate | state hibernating "· until … is built" in `color_file`, not dimmed; note carries `asleep_blocker` when a build kept it asleep; overview row "on wake: <first line of the wake prompt>"; tab dirty |
| wake | state returns to working; note "woke: the build it slept on finished" until the woken turn closes |
| await created | state awaiting_goal with one clause per `wait_reason` (pid, file, "wakes within N min"); overview row "on wake: <then>"; stop pauses |
| await fired | steer begins; note "woke: <fired condition>" until that turn closes |
| await hitting `longest_await` | the wait is clamped to 48h, recorded as `await_condition::capped`; status adds "· capped at 48h" |
| goal drafted | gate with **Approve goal** |
| goal approved | transcript row "pursuing goal …"; kind icon becomes target; steer appears in the route |
| steer budget exhausted | transcript row "steering budget spent – waiting for approval"; **Continue goal** |
| goal met | transcript row "goal met"; kind returns to chat; Summary stays, from `met_goal`, opening on the latest cycle's summarize page; goal page standing "met" |
| goal paused | transcript row "goal paused – waiting for approval"; **Continue goal** |
| rate limit hit | state rate_limited "– resuming in Nm" |
| retry countdown | same, the countdown in the clause |
| retry resumed | state returns to working |
| process exit (clean) | state resting, "ready"; not dimmed |
| process exit (failure) | state failed, "stopped"; note carries `info.failure`; tab error and dimmed |
| launch failure | transcript failure row "failed to launch 'claude' – is it on PATH?" |
| `restart_task` from settled | route current returns to scope |
| sync start | sync chat with its header and Cancel |
| sync finish | sync report delivered; chat leaves |
| sync cancelled | transcript note "cancelled from the panel" |
| crash chat opened | crash icon tab; scope does not gate (R6) |
| build-error chat opened | hammer icon tab; the Summary view gains the `apply:errors` page with the raw-error strip and "no diagnosis yet" |
| diagnosis reported | a build_error chat's apply `gse_phase_done` carries `diagnosis`; it is stored in `build_fix::diagnosis` and drawn as markdown under the strip, and the head turns to "diagnosed". Any other chat or phase sending it is refused, naming the field. Query: `errors[].diagnosis on` |
| criteria reported | a steering run's `gse_phase_done` carries `criteria` ({id, state met/owner/open, note}); the list replaces `goal_state::criteria` and redraws the goal page's left column. Any other phase or a chat with no goal sending it is refused, naming the field; an unknown state is refused with its index. Query: `criteria[]` |
| tree busy | queue row hold label; "behind '<blocker>'" on the waiting chat; "holding N builds" on the blocker |
| context approaching the window | context bar fills (system / messages / tools slices) |
| attachment added | image strip above the input grows |
| model or effort changed mid-turn | dropdowns change; takes effect on the next launch, with no indication |
| a canvas drawn by `gse_draw` | inline canvas block in the transcript; click for fullscreen |
| step reported by `gse_step` | the apply page re-marks the step (begun → current, and any other current step returns to remaining; done → done); activity verb "ticking off a step" |
| apply run ended with steps unmarked | the apply page shows the run as ended: unmarked and current steps become `[-]` not reported (muted), the head reads "ended · k/N marked" |
| first summary presented | the first Summary page with content appears (a scope gate's summary or steps, an apply/review/summarize/steer screen, build-error records); the view-mode button grows in through `presence` and the body switches from Transcript to the Summary page, which fades in. Query: `summary ready`, `view summary`, `toggle shown` |
| file edit during apply | recorded against the current step (or unattributed) and counted on the apply page, its diff appearing live on the right |

## Gallery cells beyond the enumeration

After one row per reachable cell, `chrome_gallery` draws:
- awaiting_approval with 3 findings (the enumerated scope × awaiting_approval row is the 0-findings case);
- approved with `pending_transition`, the turn still finishing;
- a short question block with 3 options, one chosen;
- a long multi-choice question with 12 two-line options in a tight room, showing the overflow line;
- build cells: queued behind a build, queued for an editor restart, queued behind 'wasm', compiling,
  red and blamed, red and unclaimed (3), unbuilt, and working while holding 2 builds;
- wait cells: hibernated on a build, hibernated and blocked by a red tree, await on a pid, await on
  a file, await capped at 48h, and an await that fired (working, with the woke note).
- scope plan cells: a scope gate with a summary and 4 steps over 7 files on page 1 (summary) and
  on page 2 (the first step hovered so its links show), and the same gate without steps (summary
  only, no navigation). The gallery draws the buttons as labelled surfaces, because a canvas has
  no builder.
- split scope gate cells: the gate held with no follow-up (full width), and followed up (plan at
  `1 - followup_share`, the column beside it) on the steps page and on the summary page. The gallery's column draws
  three sample lines, not `draw_transcript`, because a canvas has no session or builder.
- apply cells, all on the one apply page: no report (unmarked rows, every edit on the right),
  mid-way (1 done, 1 current), all done, and step 2 selected, showing its diff.
- review loop cells: one clean review (0 findings); 3 findings sent to revise, revised with 4
  edits, then a clean review; and the cap reached, three cycles with the last's finding left open.
- summarize cells: a report with all steps done, two review cycles and the edits link; and a
  report with no steps and no review, so the strip carries only the edits link.
- build-error cells on the `apply:errors` page: two errors (one with a note) and no diagnosis yet;
  the same errors with a diagnosis below; and a file-less error showing its build-output tail with
  two denied chats.
- goal cells on the `scope:goal` page (position "goal"): steering with four criteria (met, owner, met, open) and a
  two-entry ledger; criteria never reported and an empty ledger; waiting on an await; held for
  the owner (escalated or paused); steer budget spent; and met, with a met ledger entry; met in
  a chat from before met goals were kept, with the record recovered; and the same with no
  record found.

- view cells: a fresh chat with no summary (Transcript, toggle hidden) and the same chat once its
  first summary is presented (Summary, toggle shown); each name is derived from the plan's
  `summary_ready` and `toggle_shown`.

- cycle cells: a past cycle's `steer:verdict` page and its scope page, both at "cycle 2 of 4", and
  an old chat's cycle with no verdict recorded; a since-you-were-here banner and the caught-up
  state with none; a fork asking and a fork answered.

## Relocations (criterion 16)

Step 4 moves no fact. `gate_label` stays on the button, findings and the pending target stay in
the status note, and the question overflow stays in the question block.

Step 5 moves these:
- **holding N builds / behind '<blocker>'**: these used to be `status_label` clauses. They are now `build_status` in the plan, and their places (status text, overview row, popover activity) are unchanged.
- **broke <blame_label>**: still in the overview footer and the popover's "broke" row, and now also the chat's status note.
- **NOT in the exe**: still in the popover's "my edits" row and the overview footer. The footer no longer says "in the current exe" for a blamed chat that is not stale; that text stays in the popover.
- **unclaimed errors**: previously shown nowhere. Now in the status note of each chat in the tree and in the popover's "unclaimed" row.

Step 6 moves these:
- **until <unbuilt> is built / await_label**: these used to be `status_label` clauses. They are now `wait_reason` clauses from `build_status`, in the same places (status text, overview row, popover activity). The await's "at most Nh Nm" becomes "wakes within N min"; `await_label` keeps its old text for the transcript row and the log.
- **asleep_blocker**: previously a status clause. Now the chat's status note, and a transcript note row when it is set. It is no longer in the popover's activity row or the overview status.
- **on wake: …**: still on the overview row, now taken from `chrome_plan::wake_task`.
- **why it woke**: previously only a transcript row. Now also the status note while the woken turn runs.

The scope plan pass moves these:
- **tool calls and tool output**: previously in the Transcript view. They are now only in the **Everything** view, the third state of the view-mode button (Summary → Transcript → Everything), whose filter is `row_filter::all`. Transcript is `row_filter::conversation`, which is every row except `row_kind::tool`. File writes are tool rows, so they are also still in Diffs (`row_filter::changes`). The activity line still names the running tool.
- **the plan's steps and files**: new. They are in the scope plan block while the gate is held, on the Summary view's scoping pages for the rest of the task, and in `gse_panel_query` Agent's `steps` section. The prose summary stays in the transcript's phase row.
- **everything the old Summary digest showed**: in Transcript (see "Summary view = the lifecycle summary").

The split scope gate moves no fact. The follow-up exchange stays in Transcript and is also shown in the split column.

The review loop page moves no fact. The review and revise stations showed only "no screen for
this stage yet" before it. Each review's summary and findings count stay in the transcript's phase
row and the status note (`findings_note`); the cap warning stays a transcript note. The page and
the Diffs headers add views of them, and `gse_panel_query` Agent reports them as `review_cycles`
and `diff_sections`.

The report page moves no fact. The summarize report stays in the transcript's phase row and in
`report.md`; the page adds a view of it, with the task's step, edit and review totals beside it,
and `gse_panel_query` Agent reports it as `reports` (head, diffs_link, chars).

The build-error page moves no fact. The errors, and the chats that denied them, stay in the
prompt row that opened the chat (Transcript); the page adds a view of them, archived so they
survive a restart. The diagnosis is new, and its home is the page. `gse_panel_query` Agent reports
the page as `errors` (head, errors, denied, strip_lines, diagnosis) and as `apply:errors` in `plans`.

The goal page moves no fact. `steers_left` stays in the steering prompt and the status row; the
record path stays in the "pursuing goal" transcript row; each steering verdict and summary stays in
its phase row in Transcript, and escalations and pauses keep their gate. The page adds views of
them. The criteria status and the aim line are new, and the page is their home.
The goal-navigation pass moves the dashboard from the steer station to the goal scope position
("goal", before cycle 1); every fact on it is unchanged. Each steering verdict, previously only a
ledger row there, is also its cycle's `steer:verdict` page. `gse_panel_query` Agent reports
`cycles` (index, started, runs, verdict, findings, edits, stages) and `seen` (seen, arrived,
cursor, anchor, digest, fork, fork_about, fork_answer_chars).
`gse_panel_query` Agent reports the page as `scope:goal` in `plans`, as `goals` (head, standing,
steers_left, aim, tally, route, ledger_shown, ledger_latest) and as `criteria` (id, state).
A goal that is met is still cleared from the chat (`s.goal.reset()`, behaviour, not
presentation). Just before the reset, the goal is moved into `session::met_goal`, an archived
copy kept only for display. `shown_goal` returns the live goal, else that copy. A chat that met
its goal before `met_goal` existed has neither, but it has steer runs in its history. For that
chat the record is recovered from its last "pursuing goal <path>" row and the aim is re-read
from the record. Its criteria read "not reported", and the head reads "record unknown" when no
such row exists. Summary readiness, the goal page, the cycle paging and the since-you-were-here
anchor all read `shown_goal`. A met chat therefore keeps its Summary and toggle. It opens on
the summarize page of its latest cycle, and the goal page, with standing "met" and no steer
count, is one `<<` away. Starting a new goal, or restarting the task, clears `met_goal`. The
"goal met" transcript row stays as well.

## Draggable splits

Three Summary splits have a draggable divider, built on `gui::layout::update_split` through the
panel's `draw_split`, the same helper the Diffs split beside the chat body uses:

| split (`split_kind`) | divides | default first share | minimum per side |
|---|---|---|---|
| `plan_followup` | scope plan \| follow-up chat | 0.62 | 14 font sizes |
| `plan_files` | scope page 2 steps \| files | 0.58 | 8 font sizes |
| `apply_diffs` | apply page steps \| diffs | 0.36 | 8 font sizes |
| `goal_ledger` | goal page criteria \| ledger | 0.48 | 8 font sizes |

Ratios live in `agent::data::splits`, one per kind and shared by all chats, so they persist with
the agent data and not in `settings.ini`. A ratio is written only while its divider is dragged
and the container fits both minimums. Query: the Agent report carries `split_<kind>` with each
ratio.

## Opening page

**Before any summary exists** the rules below do not apply. `plan_chrome` sets
`summary_ready` once a page from `lifecycle_at(s, latest)` has content, which means any page other
than "no screen for this stage yet". A goal chat is always ready because its goal page always
exists. A sync chat never is. Until then the body draws Transcript, whatever `s.mode` says (the
mode is not written), and `toggle_shown` is false, so the Summary/Transcript button takes no
width. Diffs is a separate control and stays. When the first summary appears, the button grows
in through `ctx.presence` and the Summary page fades in. The Agent query reports `summary`
(ready|none), the effective `view` (summary|transcript|everything) and `toggle` (shown|hidden).

Each time a chat's tab is arrived at (a tab switch, or the first draw after an editor start),
its Summary is placed by `opening_page(session, since_digest)`. The first rule that matches wins:

1. **Waiting on the owner** (a held gate, or an unanswered question): the latest cycle, the
   first page of the gated phase (`gate->from`) or else the current phase. A scope gate opens on
   the scope summary page with Revise/Approve. A question opens on the current phase's page, and
   the question block stays where it always is.
2. **Unseen anchor**: the "since you were here" anchor (goal chats only), as before.
3. **Latest phase**: the latest cycle, the page of `current_phase`. `settled` maps to
   `summarize` (the report page), and `revise` maps to `review` (the review-loop page). If no
   page matches, the last page is used. A build-error chat in apply lands on the errors page,
   which is the first apply page.

The owner wait beats the unseen anchor because an owner-gated chat makes no progress until the
owner acts. The digest banner and " · new" marks still show what is unseen. Within a visit the
owner's paging is left alone, so a phase change mid-visit does not move the page.

## Wait line

When a chat is hibernating, queued or compiling, awaiting a goal condition, asking, or holding a
gate, `chrome_plan::waiting` repeats the status row's text (`build_status`: hold reason and
blocking chat, await clauses, the 48h cap, held builds) plus `asleep_note`. There is no separate strip: `draw_status_row` draws
the wait text in the activity slot after the note (the activity line is hidden while waiting, so
the chat still has one status row), and tints the row's background with `waiting_color` at
`error_strip_fill`, easing in and out through `animated_color`. It shows on every page and in
both modes, wherever the status row is drawn.

Query: each `plans` item carries `opening` (`cycle N page k/n phase:kind`, what an arrival now
would open) and `waiting` (the status row's wait text last drawn, or `none`; only the active chat
draws, so other chats read `none`). Gallery: "opening · apply hibernating on a build" and
"opening · scope held for approval" draw a tinted status row above the plan, as does every
gate-held plan cell.

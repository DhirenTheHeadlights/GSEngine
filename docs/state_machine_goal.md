# Goal: Every implicit state machine becomes one explicit, observable state

## Aim
Find the state machines the engine and editor encode implicitly — a cluster of bools, optionals, counters and timers that together mean "where this thing is in its life" — and convert them, one per cycle, into a single shared form. Each cycle finds another one. The point is threefold:
- **UX:** what the user sees (loading screen, status text) names the real state instead of a placeholder.
- **DX:** one field and one write path per machine, so impossible flag combinations cannot be written and readiness is not re-derived at every read site.
- **Agent experience:** a run's log states each transition in one format, so "why is it not ready" is one `gse_log_query`, not a reading of nine flags.

## The unified form
A candidate is classified into one of three forms. Two already exist in the tree and are the exemplars to copy.
- **Derived status** — the state is computed from facts by one pure function and presented through an annotated enum. Exemplar: `agent::state_of` (`Editor/Editor/Source/Agent/Blame.cpp:129`) with `state_style`; `index_phase` with `index_phase_info` (`Editor/Editor/Source/Search/Index.cppm:324`).
- **Async job** — requested / running / finished flags around background work become `task::pending<T>` (`Engine/Engine/Source/Concurrency/Pending.cppm`).
- **Stored lifecycle** — a new small shared value type, working name `lifecycle<E>`, built by cycle 1. It holds one enumerator and when it was entered, and has one write path, `enter(next)`. That path asserts the edge is declared and, for singleton machines, emits one log line (`<machine>: <from> -> <to> after <time>`, enum text from the reflected formatter) and one trace mark. When states carry data, the form is a variant with per-state payload, as `turn_state` already is (`Editor/Editor/Source/Agent/Model.cppm:755`).

A machine counts as **converted** when: its flag cluster is gone; every predicate that used to combine flags reads the one state; no hand-written transition log line or raw-bool dump field for it remains; and user-facing text for it comes from an annotation on the enumerators.

## Seed inventory (from scoping, 2026-11-11)
Read end to end:
1. **Engine boot.** Nine fields in `Runtime/Engine.cppm:245-266` (`m_deferred_boot`, `m_boot_tasks_done`, `m_boot_init_baseline_captured`, `m_settings_audited`, `m_actions_logged`, `m_window_shown`, `m_frames_since_rendered`, …) plus `loading::state`'s `finished` / `rendered_once`. `m_boot_tasks_done && all_settled()` is spelled three times (`Engine.cpp:658`, `:665`, `:676`). Thirteen hand-written `boot:` log lines. `loading::state::set_phase` has one caller passing one value ("Initializing"), so the loading screen never names a phase. The loading screen advances boot from a draw call through a `const_cast` (`Gui/LoadingScreen.cppm:54`).
2. **System node init/run.** `deferred`, `init_launched`, `init_done`, `init_in_flight`, `ran_once` (`Ecs/SystemNode.cppm:120-125`). Readiness is re-derived at about ten sites in `Ecs/Scheduler.cpp` (`:718`, `:743`, `:773`, `:817`, `:1356`, `:1383`, `:1437`, `:1451`, `:1505`), and the stall dump prints the raw bools (`:1371`).
3. **Attached surface.** `m_attached_surface_attempted` / `_ready`, with "abandoned" derived at `Engine.cpp:790`; editor side `surface_pipe` `connected` / `handshake_done` / `have_magic` beside `attached_session_status` (`Editor/Editor/Source/BuildRunner/BuildRunner.cppm:137-203`).
4. **Resource slot.** An explicit `resource::state` shadowed by `load_in_flight` and the `requested_reload` / `started_reload` counters; `current_state` is stored from ten sites in `Assets/AssetRegistry.cppm:379-657`.
5. **Bench.** `bench_phase` plus `scene_requested`, `scenario_started`, `frames_in_phase`; a hand-rolled enum label at `Runtime/Bench.cpp:420`.

Seen at declaration only, not yet read (leads, classify before converting):
6. Capture recording (`Graphics/Renderers/CaptureRenderer.cppm:42-119`).
7. GPU solver seed/dispatch flags (`Physics/VBD/GpuSolver.cppm:731-740`, `:791-798`); the `_dirty` bits may be independent, not a machine.
8. Network auto-connect flags beside `client::state` (`Import/Network.cppm:102-114`).
9. Build runner `building` / `building_session`, `build_completion`, `editor_image_*` (`BuildRunner.cppm:159-227`).
10. Search index `symbols_ready` / `building` / `build_requested` / `build_scheduled` beside `index_phase` (`Index.cppm:398-414`).
11. Temporal history validity: `frames_since_history_invalid >= 2` written twice (`CloudRenderer.cpp:654`, `TaaRenderer.cpp:169`).
12. Editor job flags in Git, SourceControl, Docs, Project, Profile, Workspace — likely `task::pending` cases.
13. GUI widget open/drag flags (`Gui/Types.cppm:272`, `:579`, `:624`) — the review guide already assigns these to interaction behaviours.

Sweep recipe used: `enum class` names containing state/phase/status/stage/mode (41 hits), and bool fields named pending/loaded/ready/running/started/initialized/in_flight/requested/done/busy/open/armed/failed (238 hits in 116 files).

## Pass criteria
1. **Shared type exists with a real caller.** `lifecycle<E>` is in the tree and engine boot (seed 1) is converted to it. The loading screen label comes from the boot enumerator's annotation, and the `const_cast` in `LoadingScreen.cppm` is gone.
2. **One machine per cycle, recorded.** Each ledger entry names the machine, the form chosen, the fields removed, and the files touched. A grep for each removed field name returns nothing.
3. **Each cycle is behaviour-preserving.** The build through `gse_build` is clean. `physics_stress` reports the same world-state hash as a run taken fresh before the edit, and `cloud_showcase` exits 0. Both are in the ledger.
4. **Transitions are readable from a run.** For each converted singleton machine, `gse_log_query` on a scenario run shows its transitions in the shared format, in order, with no leftover hand-written line for the same event.
5. **Seed inventory resolved.** Every row above is in the ledger as converted, or rejected with the reason it is not a state machine.
6. **Exhausted.** Two consecutive steering runs repeat the sweep recipe and find no unresolved candidate. Each records the hit counts and what it rejected.

## Charter (no need to ask)
- **Where:** `Engine/Engine/Source/**`, `Engine/Engine/Import/**`, `Editor/Editor/Source/**`, and the Sandbox, Launcher and Installer sources.
- **What:** add the shared type in the lowest module that can reach both the log and the trace; replace a flag cluster with one state; delete the predicates, log lines and dump fields that the state makes redundant; add an annotation aggregate for user-facing state text; write that text into existing UI.
- **Changing log text** of a converted machine is in scope. Grep the whole tree (docs, `Tools/`, scripts, not only sources) for the old line before changing it.
- **Process:** one machine per cycle. Build and run scenarios freely; diagnostics through `settings` overrides only. Take the before-hash fresh each cycle. Follow `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md`: no comments, no restyling outside the change, enum text from reflection, definitions in implementation units where a choice exists.

**Off limits without asking:**
- any behaviour change beyond log text and the loading-screen label;
- persisted or archived formats (agent session archive, layouts, settings keys) and wire formats (`attached_surface_message`, network messages);
- shaders and numeric solver paths;
- adding a failed or quarantined state to system nodes — `docs/system_failure_plan.md` is an unapproved proposal that decides that;
- a global registry or editor panel of machines, and any new setting — escalate with the named caller if a cycle finds one;
- reverting previously-working code to get past a failure;
- files another chat is in; `settings.ini`; re-baselining hashes; commits.

## Steering order
1. Cycle 1: build `lifecycle<E>` and convert engine boot. Settle here how edges are declared.
2. Then the remaining read seeds in the order 5, 3, 2, 4 — smallest blast radius first, the scheduler and asset registry last.
3. Then the unread leads, each classified first.
4. Then sweep for new candidates until criterion 6 holds.

## Assumptions
- **Edge declaration is untested.** The preferred form is an annotation on the enumerators, but an enum cannot name its own later enumerators inside its definition and nothing was compiled in scoping. If annotations do not work on gcc-trunk, cycle 1 uses a `consteval` edge predicate beside the enum; if neither is clean it escalates.
- **Hash repeatability is unverified.** Cycle 1 takes two before-runs of `physics_stress`. If the hash does not repeat on an unchanged tree, steering escalates rather than weakening criterion 3.
- Headless runs boot through `boot_immediately` and skip the deferred boot path, which is why criterion 3 also needs a rendered scenario.
- Per-instance populations (system nodes, resource slots) do not log every transition; they report state through their owner's existing dump, printed by the enum formatter.
- "Engine" is read as the whole repository including the editor, because agent experience lives there.

## Ledger

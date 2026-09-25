# A system that throws must fail the frame, not hang it

Written 2026-09-24 after the HumanoidLocomotion viewer froze on **Watch**. Status: proposal, not approved, nothing
edited.

## Incident

When Watch was pressed, `pose_player::run` threw `filesystem error: Cannot convert character sequence: Illegal byte
sequence`. The log recorded `Coroutine exception: …`. After that every system downstream of `pose_player` waited
forever, all workers sat idle, and the main thread spun in `sync_wait_or_dump`. A thrown system turned into a silent
deadlock, reported only as a watchdog stall dump.

## Mechanism

Two defects combine, and either one alone is enough to lose the failure.

### 1. The task promise reads an exception that nothing ever writes (paired derivation)

- `promise_base` declares `std::exception_ptr m_exception` (`Concurrency/AsyncTask.cppm:49`).
- `void_promise::result()` (`AsyncTask.cpp:178`) and `value_promise<T>::result()` (`AsyncTask.cppm:248`, `:266`)
  rethrow it.
- `promise_base::unhandled_exception()` (`AsyncTask.cpp:94-104`) only logs. It never assigns `m_exception`.

So a coroutine that throws finishes "successfully". `final_awaiter` resumes the awaiter and `await_resume` sees no
exception. Everything built on the rethrow is dead code today: the `catch (...)` in `when_all_helper` (`:328`), the
`catch` in both `sync_wait` overloads, and the `try { co_await candidate->load(ctx); } catch` blocks in
`Assets/AssetRegistry.cppm:461-471`. For a `task<T>` it is worse than dead. `value_promise<T>::result()` falls
through to `std::move(*m_result)` on an empty optional, which is undefined behaviour. A throwing asset load
returns garbage today.

### 2. Each hook signals completion on its own, and only on success (duplicated knowledge ×3)

The scheduler signals "this system is finished" at three separate sites. Each one is a statement placed after a
`co_await`, so a throw skips it:

| Hook | Completion signal skipped by a throw | Consequence |
|---|---|---|
| `run` | `ran_once`, `notify_state_ready` ×2 (`Ecs/Scheduler.cpp:1337-1342`) | dependents block in `wait_state_ready` forever, so the update wait never returns |
| `frame` | `notify_ready_by_id` (`Scheduler.cpp:313`) | the same hang in the frame phase |
| `init` | `on_complete` in `wrap_run_task` (`:299-303`, `:1313-1319`): `init_done`, `paused_event` | `init_done` never becomes true, so the system and every dependent are silently never dispatched |

Fixing (1) alone does not fix the hang. The exception would travel to `when_all`, but `when_all` cannot finish
while the dependents are still waiting on a readiness signal that never comes.

The viewer hit this on a plain function. `pose_player::run` returns `gse::async::task<>` but is not a coroutine
(it ends in `return {};`). Its throw is synchronous inside `invoke_run_fn`, so it unwinds straight into
`run_node_update`'s coroutine body.

## Design

### Part A: the task carries its failure (`gse.concurrency`)

1. `promise_base::unhandled_exception()` becomes one statement, `m_exception = std::current_exception();`. The log
   lines go away; Part B's boundary reports the failure instead. The existing rethrow in `result()`, the `when_all`
   first-exception capture and both `sync_wait` rethrows become live with no further edits. The `task<T>`
   undefined-behaviour path goes away because `result()` now rethrows before it reads `m_result`.
2. Narrow `consume_start_handle` so that "a detached frame holding an exception" cannot be written. Detached frames
   are destroyed in `final_awaiter` with no awaiter left to receive `m_exception`. Today its only callers are the
   two `when_all` awaiters in `AsyncTask.cpp`, and those only detach `when_all_helper` frames, which catch
   everything. Take it off the exported `task` surface and keep it private to the `when_all` implementation.
   Then the invariant belongs to the one module that relies on it, and no caller outside it can break it.

No new state, no new types. One member that was always there finally gets written.

### Part B: one authority for "a system hook finished" (`gse.ecs` scheduler)

Replace the three hand-written post-`co_await` sequences with one scheduler member that awaits a hook's body. All
three hook sites route through it, so a hook body is awaited in exactly one place:

```cpp
auto run_hook(
    const system_node& node,
    system_hook hook,
    async::task<> body
) -> async::task<>;
```

The member awaits `body`. If `body` throws, it fails the process with attribution:
`assert(false, "system {} threw from {}(): {}", node.trace_id, hook, e.what())`, with a `catch (...)` twin for
non-`std::exception` types. `system_hook` is printed by the reflected enum formatter, with no switch and no
annotation. The success-path bookkeeping stays in each caller, right after `co_await run_hook(...)`. On the failure
path the process ends, so that bookkeeping never needs to run and no scope-exit machinery is added.

**Why fail fast rather than quarantine.** The other option is to log, mark the node failed, still signal readiness,
and stop dispatching it. Its cost: this frame's dependents read state that a half-finished `run()` left behind, and
a new `failed` state plus a branch in `dispatchable_nodes` exist only to turn a broken invariant into a quietly
missing feature. The review guide rejects exactly that. A crash that names the system and the hook, on the frame
it happens, is the conspicuous failure. The engine already turns it into a logged stack and a crash dump
(`Runtime/Bootstrap.cppm:49` terminate handler, `install_crash_handlers`). **This is the one owner decision in the
plan:** it means a bad click in an editor-hosted game ends that game.

### Part C: the viewer (HumanoidLocomotion, project code)

1. **Pin the throw site before changing anything.** Run the viewer under `gdb -ex "catch throw"` and click Watch
   once. Parts A and B name the system and hook but not the line: by the time `run_hook` catches, the throwing
   frames are gone. Correcting an earlier note: `pose_player`'s `gse::log::println` lines *do* reach the log file
   (for example `pose_player: clip '…' 600 frames` in run `20260923_140830`). So the last `pose_player:` line in
   the 17:01 run bounds how far `run()` got. I could not open that run through `gse_log_query`: the newest
   HumanoidLocomotion run it lists is `20260923_140830`.
2. **Remove the error class, not only the one site.** Paths travel through the viewer as `std::string` and pass
   through `generic_native_encoded_string()` and `std::filesystem::path(std::string)` 20 times across
   `LocomotionTrainer.cppm` and `ViewerUi.cppm`. Each of those is a throwing encoding conversion under libstdc++ on
   Windows. For example, `load_reference` builds a path, converts it to a string, and `load_reference_clip`
   rebuilds a path from that string. Carry `std::filesystem::path` end to end instead: `play_request`
   (`clip_request` already does), `project_path`, `load_reference_clip`, `checkpoint_load`,
   `checkpoint_load_full`, and `pose_player::data::policy_path`. Convert only at genuine contracts: CLI parsing and
   log formatting.
3. **Handle the boundary where it lives.** Filesystem calls in `pose_player::run` use the `std::error_code`
   overloads or already-checked results. A failure becomes the existing "FAILED to load" status in the viewer, not
   a throw. The review guide requires this at filesystem boundaries, and Part B makes any remaining leak loud
   instead of silent.

If step 1 shows the throw is not an encoding conversion, re-scope step 2 before starting it.

## Risks

- **Parts A and B turn every currently swallowed throw into a crash.** Before landing, query recent Editor,
  Sandbox and HumanoidLocomotion logs for `Coroutine exception` and list the systems that throw today. Each one is
  a hidden bug that surfaces as a crash in the same change. Fix them, or land them in the same series.
- **AssetRegistry behaviour changes.** Its `catch` around `co_await candidate->load(ctx)` starts firing. That is
  the intended behaviour, but a load that used to "succeed" with an undefined-behaviour result will now take the
  error branch.
- **`ppo_config` compatibility (only if C2 also retypes its path fields).** The archive matches fields by name
  *and* type, so saved `.config` files would drop the retyped fields and fall back to the viewer's values, as
  reported by `skipped_fields()`. C2 as written leaves `ppo_config` as strings and converts once when the config
  is loaded. Retyping `ppo_config` is a separate decision.

## Gate

- Build through `gse_build`. Errors are judged by the build result, not Editor.log greps.
- Behaviour: a Sandbox system that throws from `run()`, `frame()` and `init()` in turn must each produce one
  `system … threw from <hook>()` line and a crash dump within one frame. No watchdog stall dump.
- No numeric path is touched. The current CPU, Vulkan and DX12 smoke hashes must stay unchanged, taken fresh
  rather than from old notes.
- Viewer: with A and B but not C, clicking Watch on the s2 run crashes with `pose_player` named. With C, the same
  click plays the run. Then click Watch on a missing or corrupt run: it shows FAILED and does not crash.

## Style contract for the edits

`docs/STYLEGUIDE.md` and `docs/CODE_REVIEW_GUIDE.md`: no comments; declarations wrap one parameter per line, and
definitions stay single-line outside the namespace; no `static` and no anonymous namespaces; enum text comes from
reflection; paths use engine and `std::filesystem` types, not hand-built strings.

# Profiler critical path plan

## Problem

`profile.txt` answers "how much time did each tag take, summed over threads". It cannot answer "what made the frame this long". Three gaps block that question.

- Every CPU table is a parallel sum. A tag that fans out over 15 workers reads 15x its wall time, so the tables rank throughput cost, not frame cost.
- The Frame DAG is the last frame only. It is an arbitrary sample (a 32 ms frame against a 25.7 ms mean in the capture that motivated this), and it does not say which bars the frame actually waited on.
- Waits are indistinguishable from work. `sched::run_wait`, `task::fanout_wait` and the fence waits carry self time exactly like compute, and untraced time on a thread is invisible.

The motivating case: the locomotion trainer at 8192 envs is CPU-bound (fence wait ~1 us) but the dump could not say which serial chain binds, or where a ~3.5 ms untraced hole on one worker came from.

## Design

### Wait spans

`trace::span_kind` has two enumerators, `work` and `wait`. It is carried on the begin event, `span_info`, `trace::node` and `profile::report_node`. `scope_guard` gains a constructor taking a kind. The event already exists, so marking a wait adds no events and no per-span cost beyond one byte.

Marked waits: `sched::run_wait`, `sync_wait::acquire`, `task::fanout_wait` (moved into `group::wait`, so the destructor waits of `coarse_parallel` and `start_frame_tasks` are covered too), `begin_frame::wait_fence` and its per-queue children, `vbd_gpu::rb_wait`, `vbd_gpu::readback::wait`, `vbd_shadow::wait_idle`.

A wait that runs jobs inline (`run_wait` calls `try_run_one`) still attributes correctly: the analysis uses the deepest span covering an instant, and an inline job is a deeper work span.

### Frame window

`trace::frame_view` and `report_frame` gain `boundary`, the timestamp `finalize_frame` closed the frame at. The frame is `[boundary - elapsed, boundary]`. Spans are clipped to it. `report_version` goes to 5.

### Analysis over recorded frames

A new partition, `gse.diag:frame_analysis`, analyses a `report_file`. It runs once, at dump time, over the frames already held by the recording ring (`Dev.profile_frame_recording`). There is no per-frame cost.

The profiler settings (`profile_aggregator_enabled`, `profile_frame_recording`, `profile_warmup_frames`) move from the renderer into an always-registered `profile_settings` system in `gse.runtime`, in the `Dev` category next to `log_settings`. Headless runs never register the renderer, so before the move no profiler setting could be overridden there and the ring never filled. The F11 dump action stays on the renderer, because it needs window input. With recording off, the dump records the live frame first so the same code path runs over one frame.

Per frame, only closed lexical CPU spans take part (`trace::cpu_scope`). Per thread, a sweep over the nested spans produces segments, each owned by the deepest span covering it or by nothing (a gap).

Critical path, walked backwards from `boundary` on the main thread:

| segment at time t | step | next position |
|---|---|---|
| work span | `work`, the segment up to t | same thread, segment start |
| wait span, or a gap on a worker | if another thread ended a work span at s inside the segment and before t: `wake` for (s, t], keyed by the wait tag | the thread that ended latest, at s |
| same, but nothing ended inside it | `wait` for the whole segment | same thread, segment start |
| gap on the main thread | `untraced` | same thread, segment start |

Each step strictly decreases t, so the walk terminates. Main-thread gaps are untraced work: the main thread only idles inside scoped waits. Worker gaps are idleness: the worker sat in `worker_loop` waiting for a job.

The releaser is the last finisher on another thread. That is a heuristic, not a recorded edge. The `wake` step is what exposes a weak edge: a real hand-off shows microseconds of wake, a coincidental one shows a long wake. Exact edges would need a release stamp in every wait primitive (group counter, `when_all`, `done_flag`, fences). Those get added only if a capture shows the heuristic misleading.

### Output sections

1. Header verdict: the mean critical path split by step, e.g. `work 21.3 ms (83%)  wait 0.0 ms  wake 0.9 ms  untraced 3.4 ms`. `wait` with no CPU releaser is the GPU or OS share. This is the bound verdict, derived from the same walk as the table below it.
2. Critical path: per (tag, step), mean / p50 / p95 per frame and share of the frame, sorted by mean.
3. Stage wall time: per tag, the union of its intervals across threads per frame (mean / p50 / p95), its summed self time, and `par` = self / wall. A fan-out with `par` well below the worker count is imbalanced or too fine.
4. Thread utilisation: per thread, the mean busy time per frame and its share of the frame.
5. Frame DAG: drawn from the median-elapsed recorded frame, over the frame window, with `*` on the spans the critical path passed through.

Percentiles reuse `profile::percentile_of`. Every duration prints through the quantity formatter.

## Verification

- Build through `gse_build`. The trace change touches only diag, task, scheduler and wait-site scopes. The hash gates cannot move.
- Re-run the ctr6 capture with `Dev.profile_frame_recording=true`, at the same 100 us counter interval. A 10 us interval slows the trainer ~7x and invalidates the capture. The critical path should total the frame (it tiles the window), and it should name the tid 5 update chain and the 3.5 ms hole.
- Cross-check against the chrome trace for one frame by hand.

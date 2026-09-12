# Intra-pass GPU timing: design (2026-09-11; stage 1 landed 2026-09-12)

## Status

- **Stage 1 (batched query resolve, both backends) — landed.** `resolve_query_pool` on the
  contract, Vulkan readback buffer + `copyQueryPoolResults`, DX12 `EndQuery`-only stamps with
  one `ResolveQueryData` per range, graph profile-end buffer. Owner decisions taken before
  implementation: Vulkan readback is a dedicated host-visible allocation owned by the pool
  (`query_pool_resources`, handle = pointer to the struct, mirroring DX12's
  `timestamp_query_pool`); the mark budget will be dynamic, not a fixed constant; colours get
  one row each.
- **Gate 1 results (HumanoidLocomotion, 1024 envs, GPU solver):** async 100-update state hash
  `2D6CBD4A…` unchanged (two runs), sync 100-update hash `E53FB027…` unchanged, 10-update hash
  identical to the last pre-change exe. Per-pass GPU table intact: solve stage 13.07 ms vs
  13.04 ms before, frame 17.4 vs 17.1 ms (noise). No device-removed or DRED output.
- **Caveat that changes how the gate reads:** every locomotion run on the owner's box falls
  back to DX12 (`vulkan: required capability not supported: VK_EXT_swapchain_maintenance1`,
  `VK_EXT_present_timing`, `VK_KHR_present_id2`; NVIDIA 596.99). So the hashes above are DX12
  gates. The Vulkan path compiles but has **not executed** on this machine; it needs either a
  driver with those extensions or the requirement relaxed before stage 2 can claim parity.
- Stages 2 and 3 (marks, solver call sites) not started.

## Why now

The locomotion trainer's asynchronous PPO update (HumanoidLocomotion `d1b30ab`) removed the
40 ms CPU-only pause that used to sit between rollout cycles. With the pause gone, every
physics tick settles at ~17.5 ms at 1024 envs whatever the CPU does — a quarter-size update,
half the workers, or moving the update to the background lane all leave it unchanged. The
render graph's per-pass GPU timestamps sum to ~16.5 ms of a 17.6 ms frame (solve 13.0,
predict 0.78, post-stabilise 0.66, adjacency 0.50, restitution 0.37, coloring 0.36, state
copy 0.23, broad/narrow 0.29, small stages ~0.3). The trainer is GPU-bound at ~94 % GPU busy
and ~40 % CPU utilisation, and the only throughput lever left is GPU time per env-tick.

The solve stage is one pass wrapping `adaptive_iterations × (colours + 3)` dispatches per
substep, so the profiler shows it as a single 13 ms row. `docs/solver_plan.md` already scoped
the missing instrument ("extend the per-pass GPU timestamps to per-dispatch granularity … how
much of the 12.2 ms is drain vs SM work") as the prerequisite for any persistent-kernel or
colour-fold work. This document designs that instrument as a permanent, both-backend feature
of the graph rather than a throwaway, because the same primitive is what any future
sub-pass GPU question will need, and a Vulkan-only probe would widen the DX12 parity gap that
`docs/dx12_backend_status.md` tracks.

## What exists today (verified in source)

- Timing is implicit for every `co_await gpu::pass<Owner>(…)`. The graph writes one
  timestamp before the body and one after, into a per-(queue, frame-in-flight)
  `gpu_profile_slot` whose pool holds `1 + 2 × max_profiled_passes` (128) queries
  (`RenderGraph.cpp:145-152`, `:575-590`, `:677-680`). Query 0 is the CPU↔GPU reference
  stamp written from a prepended per-queue "profile begin" command buffer (`:743-763`).
- Slot index comes from an atomic `fetch_add` during parallel recording, so index ≠
  execution order; `pass_types[]`/`pass_queues[]` are stored alongside and the resolve reads
  those (`read_profile_slot`, `:154-191`). That is the one-authority shape the review guide
  asks for and the design below keeps it.
- Results are consumed when the frame ring recycles the slot (`:400-404`) and ingested via
  `profile::ingest_gpu_sample(id, duration)` plus `trace::begin_async_at/end_async_at` on
  the queue's virtual thread id. Row key is the pass's `id`.
- The pass body coroutine is resumed **live** inside the graph's record lambda
  (`pass.record_handle.resume()` at `:660`), recording straight into the same body command
  buffer that already holds the opening stamp. `graph::record_replay` is barrier replay, not
  command replay. Consequently the pass's profile slot is known while the author records.
- `recording_context` exposes no timing surface; its `pass_recorder` and `device*` are
  private (`RecordingContext.cppm:224-227`). Adding intra-pass timing means one new public
  method there — there is no back door and none should be added.
- Backend asymmetry that matters here:
  - **Vulkan** `write_timestamp` → `writeTimestamp2`; `query_pool_results` →
    `getResults(e64 | eWait)` — a **blocking** read, and waiting on a query that was reset
    but never written blocks indefinitely, which is why the graph reads exactly
    `pass_count × 2 + 1` today.
  - **DX12** `write_timestamp` → `EndQuery` **and an immediate `ResolveQueryData` of one
    query** (`Dx12/Device.cpp:419-426`, `DirectX.cppm:1788-1791`); `reset_query_pool`,
    `begin_query`, `end_query` are no-ops; `query_pool_results` maps the readback buffer.
    Per-dispatch stamps at the current DX12 cost would be one resolve per stamp — roughly a
    thousand per frame in the solve alone.
- `docs/solver_plan.md:991`: dynamic solver policy consumes deterministic inputs only —
  never timing. Timing data must stay read-only diagnostics.

## Design

Four pieces, all necessary; none introduces a second way to do something that already has one.

### 1. Marks, not spans: `recording_context::mark(id)`

```cpp
auto mark(
	id label
) const -> void;
```

Semantics: the GPU interval attributed to `label` runs from this mark to the next mark in the
same pass, or to the pass's closing stamp if it is the last. One query per segment, half the
cost of begin/end pairs, no nesting rules to police, and the closing stamp the graph already
writes doubles as the terminator. A pass with no marks behaves exactly as today.

`mark` is a no-op when the pass has no profile slot (timestamps disabled, or the pass fell
past `max_profiled_passes`) or when intra-pass marks are disabled by setting. The check is one
branch on a bool the graph set before resuming the body; at hundreds of calls per frame it is
below noise, and the state is reachable (settings), so the branch stands under the review
guide's reachability test.

Author-side labels are `gse::id`s interned once, following `m_stat_ids`
(`RenderGraph.cpp:196-205`): the solver builds `solve_marks{ std::array<id, max_colors>
color; id island; id sweep; id update_lambda; id joint_lambda; id convergence; }` at init
via `find_or_generate_id(std::format(…))`. No per-frame formatting anywhere.

### 2. Slot layout: a mark region beside the pass region, one authority for both

Extend `gpu_profile_slot`:

```cpp
struct gpu_profile_mark {
	std::uint32_t pass_slot;
	std::uint32_t query;
	id label;
};

static constexpr std::uint32_t max_profiled_marks = 4096;
static constexpr std::uint32_t mark_query_base = 1 + max_profiled_passes * 2;

std::vector<gpu_profile_mark> marks;      // capacity max_profiled_marks, indexed by cursor
std::uint32_t mark_count = 0;
```

Pool capacity becomes `mark_query_base + max_profiled_marks`. Per frame the graph's
"profile begin" buffer resets the whole pool (Vulkan; DX12 no-op as today) and writes query 0.

Allocation: the graph hands each recorded pass a small cursor object through
`recording_context_init` — `gpu_profile_slot* profile; std::uint32_t profile_slot;
std::atomic<std::uint32_t>* mark_cursor;` — set in the record lambda next to the existing
`profile`/`profile_slot` locals. `mark(label)` does `i = mark_cursor->fetch_add(1)`; if
`i < max_profiled_marks` it writes `profile->marks[i] = { profile_slot, mark_query_base + i,
label }` (each index is owned by exactly one allocator, so no lock) and records
`write_timestamp(all_commands, pool, mark_query_base + i)` into the pass's own body buffer.
Over-budget marks are dropped the way over-budget passes are today (`if (slot <
max_profiled_passes)`), and the drop is counted so the profile dump can print
`marks dropped N` instead of silently under-reporting.

Resolve (`read_profile_slot`): after the pass loop, read the mark region **as its own exact
range** `[mark_query_base, mark_query_base + mark_count)` (never the unwritten gap between
the two regions — see the Vulkan wait hazard above). Group `marks[0..mark_count)` by
`pass_slot`, sort each group by `query` (allocation is monotone per pass because a body
records on one thread), and for each mark emit `duration = t(next mark in group | pass end) −
t(mark)` via `profile::ingest_gpu_sample(label, duration)` and an async span on the queue's
virtual tid. Keys reuse the existing scheme `(frame << 16) | (queue << 14) | index` with
`index = max_profiled_passes + i`, keeping pass and mark keys disjoint in the 14-bit field
(128 + 4096 < 16384). One small `profile_key(frame, queue, index)` function replaces the
inline expression so the two producers cannot drift.

Allocation and resolution both read `marks[]`; nothing re-derives the layout.

### 3. Batched query resolve on both backends (the RHI change)

This is the part that touches `Commands.cppm` on both sides and must land as one change with
parity, per the owner's rule.

New contract command, next to the existing four query commands in `CommandContract.cppm`:

```cpp
auto resolve_query_pool(
	handle<query_pool> pool,
	std::uint32_t first_query,
	std::uint32_t query_count
) const -> void;
```

Contract: after this command executes, `query_pool_results(pool, first_query, query_count)`
returns final values without blocking. Call sites: exactly one — a per-queue "profile end"
command buffer the graph records after the drain loop finishes (the point at `RenderGraph.cpp
:1312-1320` where `results_valid` is set, when `pass_count` and `mark_count` are final) and
appends to `queue_submit_order[qi]` after the last pass body. It resolves two exact ranges:
`[0, 1 + 2 × pass_count)` and `[mark_query_base, mark_query_base + mark_count)`.

Backend implementations:

| | Vulkan | DX12 |
|---|---|---|
| pool | `vk::QueryPool` **plus a host-visible readback buffer** of `capacity × 8` bytes (new) | heap + readback buffer (unchanged) |
| `reset_query_pool` | `resetQueryPool` (unchanged) | no-op (unchanged; D3D12 has no reset) |
| `write_timestamp` | `writeTimestamp2` (unchanged) | `EndQuery` **only** — the immediate `ResolveQueryData` moves out |
| `resolve_query_pool` | `copyQueryPoolResults(pool, first, count, buffer, first × 8, 8, e64 \| eWait)` | `ResolveQueryData(heap, TIMESTAMP, first, count, readback, first × 8)` |
| `query_pool_results` | map the readback buffer, copy `[first, first+count)`, unmap — **no `eWait`** | map/copy/unmap (unchanged) |

Both backends end up with the same shape: GPU writes queries, one GPU-side resolve per range
per frame, host reads a buffer that the frame ring already guarantees is complete when the
slot is recycled. That deletes two live defects in one concept — the Vulkan host-blocking
`eWait` on the resolve path and the DX12 per-stamp resolve — rather than adding a
Vulkan-only branch. It is also the "query→buffer resolve on both backends" that
`solver_plan.md:874` names as the RHI addition needed before profile slots can become
channels, so it is on the module's own path.

`eWait` on `copyQueryPoolResults` waits on the GPU for availability of every query in range;
an unwritten query would hang the GPU, not the host. The exact-range rule above is therefore
load-bearing on Vulkan and must be stated in the contract comment-free way this codebase
uses: the graph is the only caller, and it computes both ranges from the same counters it
recorded with.

Pipeline-statistics queries stay as they are (Vulkan-only, `begin_query`/`end_query`); they
are outside this change and can adopt `resolve_query_pool` later without a new concept.

### 4. Solver call sites and the setting

`stage_solve_iterations` gains one `rec.mark(…)` before each dispatch group: per colour in
the Gauss-Seidel loop (`solve_marks.color[color]`), before the jointless sweep / island
dispatch / Jacobi apply as applicable, and before `update_lambda`, `update_joint_lambda`,
`convergence_check`. Iteration index is not part of the label: rows aggregate over
iterations and substeps exactly as the pass row already aggregates over substeps, and the
timeline spans carry the order for anyone who needs per-iteration shape. ~11–12 marks per
iteration × 40 iterations × 2 substeps ≈ 900 queries per frame at 1024 envs, inside the 4096
budget with room for a second instrumented stage.

Setting, beside `gpu_timestamps_enabled` in `Renderer.cppm`:

```cpp
[[
	= settings::describe<"Record intra-pass GPU timestamp marks for passes that place them. Adds one query per mark.">{}
]]
bool gpu_intra_pass_marks_enabled = false;
```

Default off. Marks add GPU commands to the recorded stream; that is a perturbation of exactly
the kind the solver campaign has documented (contact-trace observer effect), so the default
must be the unperturbed stream. The graph carries it as `std::atomic<bool>
m_gpu_intra_pass_marks_enabled` mirroring the two existing flags.

## Costs

- **Queries:** +4096 per queue per frame-in-flight of pool capacity (32 KB readback per slot).
  Reset of the full pool per frame on Vulkan is one command.
- **GPU:** one bottom-of-pipe timestamp per mark. Measure, do not assume: the acceptance gate
  below includes a marks-on vs marks-off A/B on the trainer.
- **Host:** resolve reads a contiguous buffer; grouping 900 marks by pass and sorting is
  microseconds. No per-frame allocation if `marks` is sized once in `ensure_profile_pools`
  and grouping uses a pre-sized scratch.
- **DX12 gets cheaper**, not dearer: today's 2 × pass_count `ResolveQueryData` calls per
  queue per frame become 2.

## Determinism and safety

- Marks never feed back: no code path reads mark results except the profiler ingest.
- With marks off the recorded command stream is byte-for-byte what it is today on both
  backends. With marks on, only timestamp commands are added; no barriers, no dispatch
  changes. GPU state hashes must be unchanged in both modes (gate below).
- Contract change to `write_timestamp` on DX12 (no immediate resolve) is invisible to every
  existing reader because the graph is the only consumer of `query_pool_results` and it will
  call `resolve_query_pool` before reading.

## Review-guide check (docs/CODE_REVIEW_GUIDE.md)

- *Machinery without a caller:* the resolve command has exactly one caller (the profile-end
  buffer); `mark` has a named hot caller (`stage_solve_iterations`); the mark cursor is
  atomic because pass bodies on one queue are recorded concurrently (`profile_next_slot`
  is atomic for the same reason).
- *Paired derivations:* slot/mark allocation and resolution both read `marks[]`; ranges
  resolved and ranges read come from the same two counters; `profile_key` is one function.
- *Branches for excluded states:* `mark`'s early-out is on a settings-driven flag and on the
  over-budget case, both reachable. No null checks on `profile` beyond the one the pass
  region already has.
- *Runtime cost:* labels are ids interned at init; no formatting on the frame path.
- *Absence of evidence:* nearest analogue read end to end — `read_profile_slot`,
  `m_stat_ids`, the pass-marker checkpoint ring (`Device.cpp:150-232`, which is the closest
  existing intra-pass primitive and was judged the wrong tool: it targets device-loss
  forensics via buffer fills, not timing).
- *Style:* declarations one parameter per line, no comments, `m_` privates, module-private
  helpers not exported, unit types untouched (durations already flow through
  `time_t<double>` here).

## Acceptance gate

1. Build on both backends with marks off: every GPU hash in the parity ledger unchanged;
   profiler per-pass table identical within noise; DX12 frame time not worse (fewer resolves).
2. Marks on, both backends: hashes unchanged (timestamps do not touch solver state);
   `vbd_solve_iterations_stage` row still present and its children sum to within a few
   percent of it (the gaps between children are the launch/drain the solver plan wants to
   see, so the *sum* being smaller than the parent is the expected, informative result).
3. Trainer A/B (HumanoidLocomotion `Tools/flag-ab.ps1`, 1024 envs, async default): marks on
   vs off, to price the instrument. Expected well under 1 %; if it is more, the setting is
   still off by default and the number goes in this document.
4. Then, and only then, read the solve breakdown on the locomotion scene and on the engine's
   stress/16k scenes to answer the solver plan's open question: colour-sweep SM time vs
   inter-dispatch gaps vs lambda/convergence tails.

## Order of work

1. `resolve_query_pool` on the contract + dispatch + `PassRecorder`, both backends, Vulkan
   readback buffer, graph profile-end buffer, exact-range resolve, `eWait` removed from
   `query_pool_results`. Gate step 1.
2. Mark region, `gpu_profile_mark`, `recording_context::mark`, cursor plumbing through
   `recording_context_init`, resolve grouping, setting. Gate step 2 with a single mark in one
   pass first.
3. Solver labels and call sites. Gate steps 3–4.

Each step is independently reviewable and leaves the tree consistent on both backends.

## Open questions for the owner

- Vulkan readback buffer per pool: host-visible coherent allocation via the existing buffer
  helpers, or a dedicated small allocation? The existing `buffer` type is the obvious choice
  if it can live inside `gse.vulkan` without importing `gse.gpu` (layering rule).
- `max_profiled_marks = 4096`: fixed like `max_profiled_passes`, or a setting? Fixed is
  simpler and matches the sibling constant; a second instrumented stage still fits.
- Row naming for colours: `vbd::solve::color[k]` gives one row per colour index (≤ 8 live
  today); alternatively one aggregate `vbd::solve::color_sweep` row plus the timeline. The
  first answers "is colour 0 disproportionately slow" directly, so it is the proposal.

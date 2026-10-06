# Vulkan worker command pool memory

Written 2026-10-05. Status: **proposal, not approved — no code has changed.**

## Why this matters

Trainers in HumanoidLocomotion have been dying with `std::bad_alloc` when several run at once
(2026-10-04 23:50 and 2026-10-05 ~09:50). The box was out of **commit**, not RAM: the page file was
at ~4 GB of 28 GB used, physical memory had ~11 GB free, and the commit limit (~59 GB) was full.
The allocation that failed was in `gse::physics::gpu_upload::run`, but that is just the
heaviest per-frame allocator, so it is where the shortage surfaces. It is not the cause.

A Vulkan trainer commits ~5.9 GB and keeps ~1.8–2.4 GB of it resident. About **2.8 GB of the
difference is 32 MB write-combined blocks that are committed and never touched.**

## Measurements

Each process was walked with `VirtualQueryEx` plus `QueryWorkingSetEx`. The script is at
`C:\Users\Dhiren\scratch\vmwalk.ps1 -TargetPid N` and is not in the repo. Blocks are private,
`PAGE_READWRITE | PAGE_WRITECOMBINE` (`0x404`), 32 MB each, 0 bytes resident.

| process | backend | envs | `Physics.gpu_max_*` | private commit | 32 MB WC blocks | engine 64 MB WC blocks |
|---|---|---|---|---|---|---|
| trainer (omniQJ) | Vulkan | 8192 | raised | 5.92 GB | 89 | 11 |
| probe | Vulkan | 1024 | raised | 5.10 GB | 83 | 10 |
| probe | Vulkan | 256 | defaults | 4.31 GB | 77 | 2 |
| probe | **DX12** | 256 | defaults | **1.65 GB** | 5 | 0 |
| editor (Oct-4, idle) | Vulkan | — | — | 2.68 GB | 5 | 5 |

Conclusions the table supports:

- The 32 MB blocks are **not** physics or rollout sizing. They barely move across a 32× env range
  and between default and raised capacities. The engine's own 64 MB pool blocks do scale with
  capacity (`Vulkan/Device.cpp:2300`, `k_default_block_size`, `Vulkan/Device.cppm:747`).
- They are **not** created by allocating pools or command buffers. The idle editor creates exactly
  the same eager pools (below) and holds 5 blocks.
- They appear during the first GPU frames: none at t=30 s, all of them at t=60 s. They do not
  grow after that. So they track **recording**, and their memory is kept at the high-water mark.
- The same workload on DX12 commits 2.66 GB less. The two backends differ in exactly the structure
  below.

## What the two backends do today

**Vulkan** (`Vulkan/CommandPools.cppm`):

- **Eager pools.** `worker_command_pools::create` (`:296`) builds one `VkCommandPool` per
  (distinct queue family × worker × frame). It is called with `task::thread_count()`
  (`Gpu/Device/DeviceVulkanBackend.cppm:884`).
  - `video_encode` shares the graphics family (`DeviceVulkanBackend.cppm:880`), so on this box
    that is 2 families × 16 workers × 2 frames = **64 pools**.
- **Eager buffers.** Each pool gets 128 eagerly allocated primary buffers (`:81`, `:277`), and an
  exhausted pool doubles (`:362-372`).
- **No release on reset.** `reset_frame` (`:339-347`) calls `vkResetCommandPool` without
  `VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT`, so every pool keeps its high-water memory forever.
- **One buffer per pass, on whichever worker runs it.** `render_graph::execute` records each pass
  into its own buffer from `task::current_worker()`'s pool, via `task::parallel_invoke_range`
  (`Gpu/Graph/RenderGraph.cpp:836-850`). A trainer frame therefore spreads its passes over all
  16 workers, and over time every worker's pool on both families records something.

**DX12** (`Dx12/CommandPools.cppm`), the in-engine analogue that does not show the problem:

- `bind` (`:77-82`) sizes `m_worker_lists` to queue type × frame, which is **6 pools**, independent
  of worker count. `acquire_worker_command_buffer` ignores its worker argument (`:91`) and
  serialises acquires through one mutex (`:92`).
- Allocator/list pairs are created **lazily**, one at a time, on exhaustion (`:96-119`). Each one
  is `Reset` on re-acquire (`:124-126`).

So the pool count is the one structural difference. On Vulkan it scales with
`hardware_concurrency`, which means a 32-thread machine would hold twice as much. On DX12 it is a
constant.

## The hypothesis, and what is not yet proven

**H:** the NVIDIA Vulkan driver commits a ~32 MB write-combined arena per command pool, or per
recording high-water within a pool, and keeps it across `vkResetCommandPool` without
`RELEASE_RESOURCES`. 64 pools × 32 MB = 2.0 GB; doubling growth in busy pools plausibly accounts
for the remaining 13–25 blocks.

The evidence above is strong but correlational: block count and pool count are close but not
equal, and nothing yet ties a specific block to a specific pool. **Phase 0 exists to prove H
before any design lands.** If H fails, this plan stops and the 32 MB blocks get a separate
investigation.

## Phase 0 — attribution (temporary, reverted)

There are two one-line probes. Each is built through `gse_build`, measured, and reverted before
the next. The measurement is the 256-env, default-capacity Vulkan probe at t=60 s and t=90 s,
counting `0x404` 32 MB blocks; the baseline is **77**.

| probe | change | if blocks drop to ≈ 0–10 | if unchanged |
|---|---|---|---|
| P1 | `reset_frame` resets with `vk::CommandPoolResetFlagBits::eReleaseResources` | memory is recording memory retained across reset: H confirmed | not recording-pool memory: stop |
| P2 | `worker_command_pools::create(..., 2)` at `DeviceVulkanBackend.cppm:884`, with `acquire_command_buffer` indexing `worker_index % 2` | memory scales with pool count: Phase 1 design A | memory follows buffers or recording volume: design B |

P1 is diagnostic only. Releasing on every frame returns the memory to the driver and reallocates
it 60+ times a second, which is exactly the per-frame churn the review guide rejects. It is not
the fix.

There is no runtime knob for the worker count. `engine_config::worker_threads`
(`Runtime/Engine.cppm:85`) has no settings or CLI path, which is why P2 is a code probe.

## Phase 1 — the fix

### Design A (expected): pools keyed by recording slot, not by worker

Make the Vulkan pool count a constant chosen by the code that records, the way DX12's already is.

1. **Record in ranges.** `render_graph::execute` records passes in `k` contiguous ranges instead of
   one task per pass. Range `r` acquires every buffer for its passes from slot `r`. `k` is a
   `static constexpr` member of the render graph's state. It is not a free global and not a
   setting: no caller needs to tune it.
   - Contiguous ranges keep the submission order the graph already relies on. Each range is
     recorded serially, so one pool is never used by two threads, which is Vulkan's
     external-synchronisation rule. This is enforced by construction rather than by a lock.
2. **Slot replaces worker index in the API.** `acquire_worker_command_buffer`'s `worker_index`
   parameter becomes the recording slot on both backends.
   - The front-end declaration (`Gpu/Device/Device.cppm:464`), the generated dispatch table
     (`Gpu/Device/Device.cpp:35-39`) and both backends' signatures (`DeviceVulkanBackend.cppm:121`,
     `DeviceDx12Backend.cppm:119`) change in one step, because the dispatch table is reflected from
     the Vulkan backend and DX12 must match it member for member.
   - DX12 keeps ignoring the value. Its single mutex already serialises acquires, and its pool
     count already is the constant this design moves Vulkan to.
   - The six single-threaded call sites (`RenderGraph.cpp:1151, 1728, 1762, 1866`,
     `Frame.cpp:455, 474`) already pass 0 and keep slot 0.
3. **Size pools from slots.** `worker_command_pools::create` takes `k` instead of
   `task::thread_count()`. The `worker_index < per_worker.size()` assert (`:353`) becomes a slot
   bound, and the dependency on `hardware_concurrency` disappears.
4. **Allocate buffers lazily.** Create buffers in the amount actually acquired, replacing the 128
   eager buffers and the doubling, matching DX12's grow-on-exhaustion. The eager 128 has no caller
   that needs it: acquires are counted per frame and the pool grows anyway.
5. **Drop `task::current_worker()` from recording.** Remove the `task::current_worker()` lookup and
   its `has_value` assert at `RenderGraph.cpp:846-847`; the range index replaces them.

Expected result: 2 families × 2 frames × `k` pools. At `k = 4` that is 16 pools, so ~0.5 GB of
arenas instead of ~2.0–2.8 GB, and the count no longer depends on the CPU.

The cost to measure is recording parallelism. Recording drops from up to 16-way to `k`-way.
Trainer frames are dispatch-heavy but record quickly; the editor's frame is the one to watch.

### Design B (if P2 shows memory follows buffers, not pools)

Keep the per-worker pools, and only do step 4 (lazy buffer allocation) plus whatever P2 points at.
Re-plan before coding; do not merge A speculatively.

## Review-guide checks this design answers

- **Nearest analogue read end to end:** `dx12::command_pools`. Design A converges Vulkan on DX12's
  shape (constant pool count, lazy growth) rather than inventing a third scheme.
- **Machinery without a caller:**
  - Design A adds no lock, cache or generation. Single ownership comes from range-to-slot
    assignment, not from synchronisation.
  - The eager 128 buffers and the per-worker fan-out are the machinery being removed.
- **Both backends in step:** the API change lands on Vulkan, DX12, the front-end and the dispatch
  table together. DX12 behaviour does not change.
- **Runtime cost:** no per-frame allocation is added. `RELEASE_RESOURCES` is explicitly rejected as
  the fix.
- **Invariant restored, not compensated:** "a pool is recorded by one thread at a time" moves from
  "one pool per thread" to "one pool per range". Ranges are serial by construction, so the
  invariant no longer depends on how many threads the scheduler happens to start.

## Gates

1. **Memory.** A 256-env default-capacity Vulkan probe at t=60/90 s has ≤ 16 32 MB `0x404` blocks,
   and total private commit is within ~0.5 GB of the DX12 probe (1.65 GB). Re-measure at 8192 envs
   with the training capacities.
2. **Behaviour.** This change only reorders command-buffer ownership, so GPU results should be
   bit-identical. Gate on the default state-hash refs on **both** backends (Vulkan `1CE4CB22`,
   DX12 `74DD4406` as of 2026-10-05), plus one frozen-probe run, because a hash on an inert
   humanoid is not proof on its own.
3. **Throughput.**
   - Trainer: interleaved A/B of steps/s against a kept pre-change exe on an idle box.
   - Editor: frame time for the render graph's record phase in the profiler, before and after.
   - A regression in either sends `k` back for measurement; it is not a reason to keep
     per-worker pools.
4. **Build.** Through `gse_build` only. Report attributed errors only.

## Risks

- **Pass order within a range.** Passes must still be submitted in graph order. Ranges are
  contiguous and each range's buffers are collected in pass order, but the submission code that
  currently consumes `pass_bodies` by pass index must be re-read when this is implemented.
- **Profile and barrier helpers.** `profile_begin` and `transition` acquire on slot 0 from the
  recording thread while ranges may be recording on other threads. Slot 0 must therefore not
  double as a range slot, or those helpers need their own slot. Decide this when implementing;
  it is the one place a second thread could touch the same pool.
- **Driver specificity.** H is about NVIDIA's driver. On AMD or Intel the saving may be smaller,
  but the change is still correct: it removes a `hardware_concurrency` dependency.

## Out of scope

- The editor's own commit (6.6 GB on the live editor): 1613 × 1 MB and ~100 × 15.8 MB `0x4`
  blocks, most of them non-resident. That editor is minimised, so these may simply be trimmed
  pages. They need a separate look.
- `gpu_upload::run` per-frame allocations (the `body_index` flat_map copy into
  `gpu_upload_report`, the `per_tick` copy). They are real churn but not the commit problem.

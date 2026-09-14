# In-process GPU hardware metrics: Nsight Perf SDK periodic sampler (2026-09-14, implemented)

## Status

Stage 1 works end to end as of 2026-09-14. The SDK is at
`C:\NVIDIA_Nsight_Perf_SDK_2025.5_Public_Windows`, `NSIGHT_PERF_SDK` is set, the overlay port
installs from it, `GSE_HAVE_NSIGHT_PERF=1` is defined in both the engine and game build trees, and
`nvperf_grfx_host.dll` is copied next to the executable by the existing `gse_copy_runtime_deps()`
with no change. Counter rows reach `profile.txt` keyed by render graph pass and by intra-pass mark.

Verified on an RTX 5090 (GB202, driver 596.99) at 8192 envs, GPU solver, 3,932,160 steps, at both a
10 us and a 100 us sampling interval. The two runs agree, and the pass they were taken to measure —
`vbd::solve::island` — held 24.68 ms of GPU time per frame at 100 us versus 25.46 ms at 10 us, so
the interval moves CPU decode cost and not the GPU behaviour under test. Results are in the Gate
section.

GPU performance counters are admin-only by driver default; on this machine
`NVPW_GPU_PeriodicSampler_BeginSession_V2` returned `NVPA_STATUS_INSUFFICIENT_PRIVILEGE` until the
counters were opened to all users in NVIDIA Control Panel → Developer. Note that
`HKLM\SYSTEM\CurrentControlSet\Services\nvlddmkm\Global\NVTweak\RmProfilingAdminOnly` still reads
absent after that change, so it is not a usable check — start a session and read the status.

Five things the plan got wrong were only discoverable by running it, and are fixed in the tree:

1. The periodic sampler needs a graphics-API driver loaded first (`NVPW_D3D12_LoadDriver`; without
   it `IsGpuSupported` returns `NVPA_STATUS_DRIVER_NOT_LOADED`).
2. The sampler can only collect the `*_realtime` counter variants, so the plan's proposed default
   metric list needed 24 passes and was replaced.
3. Evaluator metric names need a rollup and submetric suffix, so the bare counter names in NVIDIA's
   own shipped `.config.yaml` files are not accepted by `ToMetricEvalRequest`.
4. The record buffer must be opened in keep-latest, not the SDK wrapper's default keep-oldest. In
   keep-oldest a single overflow makes the driver stop recording for the rest of the session, and
   the engine takes seconds to reach its first render graph readback, so the buffer always filled in
   that startup gap and sampling was dead from then on. The signature is
   `session ended after N samples` where N is exactly the configured capacity. Recovery from a lap
   needs `SetRecordBufferReadOffset(writeOffset)` *and* `AcknowledgeRecordBuffer(unread)`, because
   decode advances only the CPU-side read offset and acknowledge only the GPU-side one.
5. Decode must run once per frame, not once per profile slot. `decode()` clears its sample vectors,
   so calling it per queue let the graphics slot drain the window and left the compute slot — which
   is where every physics mark lives — with nothing to attribute.

## Why

The locomotion trainer at 8192 envs is GPU-throughput bound: the VBD island solve is ~27 ms of
GPU time per frame (80 dependent dispatches of ~340 µs) and the PPO update chain ~18 ms
(~1050 dispatches of ~35 µs). The 600k steps/s target needs the solve roughly 3× faster. Which
lever pays (occupancy, memory latency, instruction issue) is unknown because no instrument on
this box reads SM counters for Vulkan or DX12 compute in a headless process:

- Nsight Compute profiles CUDA kernels only (confirmed 2026-09-14: the trainer ran under it
  and the report was empty).
- Nsight Graphics delimits captures on Present. The headless trainer never presents
  (`Headless device: presentation extensions not enabled`), so GPU Trace produced nothing.
  The windowed trainer would work but adds a render pass to every frame.
- The engine's own timestamps and marks (`docs/gpu_intra_pass_timing_plan.md`) give per-dispatch
  time, not why the time is spent.

The Nsight Perf SDK reads the same hardware counters in-process from any NVIDIA GPU, with no
window, no external tool, and no CUDA. It is the profiling analogue of the Aftermath crash-dump
integration and slots into the same seams.

## What the SDK offers, and which half we use

The SDK has two collection models (Getting Started Guide 2022.3, "Application Integration",
confirmed unchanged through the 2025.5 release notes):

1. **Range Profiler.** Counters per `PushRange`/`PopRange` bracket in the command stream. It is
   a multi-pass replay profiler: a pass must be replayed `NumConfigurationPasses ×
   NumNestingLevels` times, and inside a pass the driver isolates ranges from each other so
   they no longer overlap. A training frame mutates state and cannot be replayed, and isolated
   ranges change the timing being studied. Rejected for stage 1.
2. **GPU Periodic Sampler.** Samples a fixed counter set for the whole GPU at a fixed interval
   (or on `CpuTrigger`). "Requires no interaction with the graphics API", no replay, the
   configuration must fit one pass. Attribution to work comes from timestamps.

Stage 1 is the periodic sampler correlated against the engine's existing per-pass and per-mark
GPU timestamps. It never touches a command buffer, so it is bit-identical by construction and
backend-neutral: one implementation serves Vulkan and DX12.

## Change

### SDK acquisition and build (mirror `vcpkg-overlays/ports/nsight-aftermath/`)

- The SDK is a zip from developer.nvidia.com behind a developer login (the download page is a
  login wall and the asset URLs are time-limited signed links), so no vcpkg port can fetch it and
  it may not be vendored into the repo. New overlay port `vcpkg-overlays/ports/nsight-perf/` reads
  `NSIGHT_PERF_SDK` (root of the unzipped package), locates `nvperf_host.h`, the NvPerfUtility
  headers and the DLL anywhere beneath it, installs the headers plus the single runtime DLL
  `nvperf_grfx_host.dll` (Windows x64) and writes a config declaring
  `unofficial::nsight-perf::nsight-perf`. With the variable unset it installs the same empty stub
  the Aftermath port does, so the engine configures without it.
- `Engine/CMakeLists.txt`: `find_package(unofficial-nsight-perf CONFIG QUIET)` next to line 68,
  and next to lines 135-141 an `if(TARGET ...)` block that links it and defines
  `GSE_HAVE_NSIGHT_PERF=1`. No `option()`; presence of the target decides, as for Aftermath.
- The DLL is loaded at runtime by the SDK's shim header
  `NvPerf/include/windows-desktop-x64/nvperf_grfx_host_impl.h`, which may be included in exactly
  one translation unit. That unit is `Source/External/NsightPerf.cpp` (below). No import library
  is needed, which is what makes the MinGW build possible; the port must not try to link one.
- The DLL is copied next to the executable through `gse_copy_runtime_deps()`, the path the
  Agility SDK `D3D12/` folder already takes (`Engine/CMakeLists.txt` line 143 onward).
- Verify before building: the shim header compiles under GCC (it is plain C with
  `LoadLibraryA`/`GetProcAddress`; the Aftermath header did). If it does not, wrap the offending
  declarations in the same translation unit rather than patching the SDK.

### Layer A: `Source/External/NsightPerf.cppm` + `NsightPerf.cpp` (module `gse.nsight_perf`)

Mirror of `Source/External/Aftermath.cppm`: every SDK type stays in the module preamble, the
exported interface is POD mirrors and free functions, everything compiles to a no-op returning
`false`/empty when `GSE_HAVE_NSIGHT_PERF` is undefined, and `constexpr bool compiled_in` reports
linkage. Interface:

```cpp
export namespace gse::nsight_perf {
    constexpr bool compiled_in;
    struct metric_spec { std::string_view name; };
    struct session_settings {
        std::uint32_t device_index;
        std::span<const metric_spec> metrics;
        time_t<std::uint64_t> sampling_interval;
        std::size_t max_samples_per_frame;
        bool lock_clocks_to_rated_tdp;
    };
    struct sample { time_t<std::uint64_t> gpu_time; std::span<const double> values; };
    struct clock_calibration { time_t<std::uint64_t> gpu_time; time_t<std::uint64_t> cpu_time; };
    auto begin_session(const session_settings&) -> bool;
    auto end_session() -> void;
    auto status() -> session_status;
    auto calibrate() -> std::optional<clock_calibration>;
    auto decode(std::span<sample> out) -> std::size_t;
}
```

Behind it: `NVPW_InitializeHost`, device enumeration by LUID matching the engine's adapter (the
SDK's `GpuDiag` output shows `DeviceLUID`, and both `vulkan::physical_device` and the DX12
adapter expose one), metric config compiled with the SDK's metrics evaluator, the
`NumConfigurationPasses` check, and the ring buffer decode. `session_status` is an enum with
annotations for the log line: `not_compiled_in`, `no_permission`, `unsupported_gpu`,
`config_needs_replay`, `running`.

Rules from the SDK that the layer enforces, with a warning line and `false` rather than an
abort:

- `NumConfigurationPasses` must be 1. The sampler cannot replay, so a metric set that does not
  fit is refused as a whole; the log names the first metric that overflowed the pass budget so
  the list can be trimmed.
- Profiling permissions (`ERR_NVGPUCTRPERM`): the driver denies counter access unless the NVIDIA
  Control Panel setting Developer > Manage GPU Performance Counters is set to allow all users, or
  the process is elevated. The layer reports `no_permission` with that URL
  (`https://developer.nvidia.com/ERR_NVGPUCTRPERM`) and the trainer continues unprofiled.
- Clock locking is off by default. The SDK can lock to rated TDP (`NVPW_Device_SetClockSetting`),
  which makes stall ratios repeatable but moves every microsecond figure away from the profile
  tables the rest of the work is measured against. Expose it, default it off, and never leave a
  lock behind: `end_session` restores the clock setting, and so does the destructor path on
  device loss.

### Layer B: ownership and lifetime

The sampler is a property of the adapter, not of a backend, so it belongs on `gpu::device`
next to the pass-marker rings (`Source/Gpu/Device/Device.cppm` line 50 onward), not inside
`vulkan_device_backend` or `dx12_device_backend`. Both backends already expose their adapter
LUID; the device façade passes it when the session starts. No new member on either backend
struct, so the reflection-generated device contract (`Device.cpp` line 28) is untouched.

Session lifetime equals the setting's lifetime: started on the first `context::run` that sees
`gpu_perf_metrics_enabled` true, ended when it goes false or on device destruction. Starting a
session is cold (once per run), decoding is per frame.

### Layer C: setting and read site

`Source/Gpu/Context.cppm` after line 64, matching the three profiling bools:

```cpp
[[
    = settings::describe<"Sample NVIDIA GPU hardware counters (SM activity, warp occupancy, stall reasons) and attribute them to render-graph passes and marks. Needs the Nsight Perf SDK at build time and GPU performance-counter permission at run time.">{}
]]
bool gpu_perf_metrics_enabled = false;
```

Read next to `Context.cpp` lines 131-133 and forwarded to the render graph like the other
three. It requires `gpu_timestamps_enabled` (attribution needs the pass timestamps); the render
graph derives `metrics_enabled = timestamps_enabled && m_gpu_perf_metrics_enabled` exactly as
it derives `marks_enabled` (`RenderGraph.cpp` line 447). The metric list is a second setting,
`Graphics.gpu_perf_metrics`, a comma-separated string with the default below, so a run can
swap the counter set without a rebuild.

### Layer D: attribution in the render graph

`read_profile_slot` (`RenderGraph.cpp` lines 170-275) already converts every pass and mark to a
`[start, end)` interval and emits a duration row. Stage 1 adds, in the same loop and only when
metrics are enabled:

1. Once per read: `nsight_perf::decode` into a per-slot `std::vector<sample>` (capacity fixed at
   session start from `max_samples_per_frame`; no per-frame allocation after warm-up), and the
   GPU-to-CPU offset from `nsight_perf::calibrate()` rather than the current
   `cpu_ref - timestamps[0] * period` estimate, which anchors on CPU record time and can lag the
   GPU by up to a frame.
2. Per pass and per mark: the samples whose `gpu_time` fall inside the interval, averaged per
   metric, emitted with `trace::counter_at(metric_id, value, trace::gpu_stats_virtual_tid, start)`
   exactly as the pipeline-stats rows are (`RenderGraph.cpp` lines 263-268), and into a new
   `profile::ingest_gpu_metric(id, metric_index, value)` so `profile.txt` gains the columns in
   the GPU section. Row ids are derived once, `find_or_generate_id(pass_name + ":" + metric)`,
   and cached in a map keyed by pass id, the `m_stat_ids` pattern.
3. Intervals shorter than two sampling periods get no row rather than a one-sample row; the
   summary states the sample count per row so a 35 µs dispatch cannot masquerade as measured.

Samples are device-wide. On the graphics queue the trainer's passes run one after another, so
a sample inside a pass interval belongs to that pass; the DX12 and Vulkan compute queues are
unused by the trainer today. When the async-compute plan lands the attribution needs the
queue's own timestamps, which the profile slots already keep per queue.

Default metric set, chosen to fit one pass on Blackwell (the implementer verifies with the
SDK's `NumConfigurationPasses` and trims from the bottom):

```
sm__throughput.avg.pct_of_peak_sustained_elapsed
sm__warps_active.avg.pct_of_peak_sustained_active
sm__inst_executed.avg.per_cycle_active
smsp__warps_issue_stalled_long_scoreboard.avg
smsp__warps_issue_stalled_barrier.avg
smsp__warps_issue_stalled_membar.avg
smsp__warps_issue_stalled_short_scoreboard.avg
smsp__warps_issue_stalled_wait.avg
lts__t_sector_hit_rate.pct
dram__throughput.avg.pct_of_peak_sustained_elapsed
```

The first three answer the occupancy question (SMs busy, warps resident, issue rate); the
stall reasons say whether the island solve waits on memory (`long_scoreboard`), on
`GroupMemoryBarrierWithGroupSync` (`barrier`), or on dependent math (`wait`, `short_scoreboard`);
the last two say whether L2 or DRAM is the ceiling. Metric names are SDK-version specific:
enumerate them from the SDK's metrics evaluator at session start and log any name that does not
resolve rather than failing the session.

### Sampling interval

Default 10 µs. The island solve dispatches (~340 µs) get ~34 samples each; the nn dispatches
(~35 µs) get 3, enough for the per-pass averages the question needs but not for per-dispatch
rows, which is why marks shorter than two periods are dropped. The sampler ring holds the
frame's samples; at 10 µs and a 40 ms frame that is 4000 samples of ten doubles, decoded once
per frame on the CPU thread that already reads the profile slot.

### Log lines

At session start one info line: SDK version, device name, permission state, metric count,
passes (must read 1), interval, clock lock state. At session end one line with sample counts.
Under `gpu_perf` category so `gse_log_query` can filter it.

## Why this is bit-identical

Stage 1 records nothing into any command buffer and changes no submission. The only GPU-side
effect is the driver's performance-monitor sampling, which does not alter shader execution
results. The state hash therefore cannot move; the gate below checks it anyway because the
sampler shares the driver with the workload and the only cheap way to prove "no effect" is to
run the reference.

## Overhead

The SDK documents the periodic sampler as low overhead. That is true of the GPU side and badly
wrong about the CPU side at a short interval, because the cost scales with record volume rather
than with anything the plan reasoned about. A device-wide sample on a GB202 is ~17.9 KB of raw
records, so a 10 µs interval produces ~1.8 GB/s that the CPU must decode and evaluate per sample.
At 8192 envs the same 3,932,160-step run took 225 s at 10 µs and 51 s at 100 µs — 4.4x — while the
GPU time of the pass being measured moved 3 %. So the overhead is decode, it lands on the CPU, and
at 100 µs it is small enough to measure with while still giving a 25 ms pass ~250 samples a frame.
The `> 3 %` bar in the original plan is not met at 10 µs and is not worth chasing: the setting is
off by default and throughput tables must never be taken with it on.

A strict setting-off vs setting-on A/B at both env counts is still untaken; the figures above come
from two sampling intervals rather than from a no-counter baseline.

## Gate

- Vulkan and DX12 1024-env references (`AC3D5339…` / `F838905F…`, 409600 steps, 15 workers)
  with the setting off, then on, on both backends. All four must match.
- With the SDK absent (`NSIGHT_PERF_SDK` unset): the engine configures, builds, the setting is
  present but the session reports `not_compiled_in` once and does nothing.
- With permissions withheld: `no_permission` once, trainer unaffected.
- A metric list that needs two passes: `config_needs_replay` with the metric named, no session.
- Attribution sanity: at 8192 envs the `vbd_solve_iterations_stage` row must carry ~2700
  samples per frame at 10 µs and `sm__throughput` must be near the island solve's known
  ~27 ms share of a ~36 ms frame; a pass shorter than 20 µs must have no row.
- Overhead A/B as above.
- `profile.txt` GPU section shows the new columns; `trace.json` shows the counters on the GPU
  stats virtual thread.

Run on 2026-09-14 with the SDK present, at 1024 envs / 51200 steps / 8 workers (a shorter arm than
the reference gate, so the hash below is its own baseline, not `AC3D5339…`):

- Setting off and setting on both give `3FB4F1D79D3E9B6BED7D3BF75B0A4417238A1B314B868ABF936DFABCA0DC4689`
  across five builds. The enablement path perturbs nothing — but this was taken while the session
  could not start on this machine, so it shows the plumbing is inert, not yet that sampling is.
  It needs retaking now that sessions start.
- A metric list needing more than one pass: reproduced by the plan's own default list, which the
  engine refused with `config_needs_replay`, the pass count, and the offending metric named.
- Permission withheld: reproduced, though it reports `start_failed` rather than `no_permission`
  (see the known gap above). The trainer is unaffected and the run completes normally.

Attribution and output, taken 2026-09-14 at 8192 envs / 3,932,160 steps / 15 workers, GPU solver,
`gpu_intra_pass_marks_enabled`, on DX12. Two runs, one at each interval; both completed the same
step count with exit 0:

| | 10 µs | 100 µs |
| --- | --- | --- |
| `vbd::solve::island` GPU time | 25.46 ms/frame | 24.68 ms/frame |
| samples behind the row | 150941 | 6036 |
| warps active, % of peak (`queue_sync` — wrong queue, superseded) | 1.703 | 1.507 |
| `sm__inst_executed` per cycle active | 1.017 | 1.269 |
| FMA pipe, % of peak | 10.902 | 13.488 |
| `lts__t_sectors` per second | 42.1 G | 45.7 G |
| `dram__bytes` per second | 122.7 GB | 97.9 GB |
| wall clock for the run | 225 s | 51 s |

- `profile.txt` carries the `--- GPU hardware counters (mean over sampled passes) ---` section with
  pass, metric, value and sample columns, and marks appear alongside passes. Short passes drop out
  as designed: `vbd_apply_body_inputs_stage` got 4 samples at 10 µs, `vbd_collision_reset_stage` 39.
- Overhead is far higher than the plan assumed, and the reason is the record volume: a device-wide
  sample is ~17.9 KB of raw records, so 10 µs is ~1.8 GB/s and `buffered_samples = 65536` allocates
  a 1.17 GB record buffer spanning only 0.65 s. The 4.4x wall-clock difference above is CPU decode,
  not GPU: the measured GPU time for the pass moved 3 %. Use 100 µs for measurement runs; a 25 ms
  pass still gets ~250 samples per frame. Either way the setting must be off for throughput tables.
- Realtime counters return NaN for a sample in which the queue had no active cycles, because
  `per_cycle_active` and `pct_of_peak_*` divide by zero there. The first run printed `-nan` for
  `nn_chain_stage`, `nn::train_sample` and `vbd_state_copy_stage`; attribution now skips non-finite
  values per metric, and the 100 µs run has none. `vbd::solve::island` had no NaN samples in either
  run, so its numbers are unaffected by that fix.
- **Pick the queue qualifier deliberately.** Many `tpc__` and `gr__` counters come in three forms:
  unqualified, `_queue_sync_`, and `_queue_async_`. NVIDIA's shipped gb20x triage config leads with
  the `queue_sync` variants because it is written for a graphics app, and everything in this engine's
  compute path dispatches `.on(gpu::queue_type::compute)`, which maps to a separate
  `D3D12_COMMAND_LIST_TYPE_COMPUTE` queue (`External/DirectX.cppm:1217`). A `queue_sync` counter
  therefore cannot see any of it. Measured side by side on `vbd::solve::island`: unqualified 17.371,
  `queue_sync` 1.713, `queue_async` 15.657 — the unqualified form is the sum, and the sync form is
  residue from the graphics queue. The first two measurement runs above read the sync variant and
  understated warp occupancy by 9x.

Still untaken: the four reference hashes at 409600 steps on both backends, a fresh setting-off vs
setting-on parity arm now that sessions actually start, and the `trace.json` counter track on the
GPU stats virtual thread.

## What the instrument must answer

For `vbd::solve::island` at 8192 envs: SM throughput, warps active, and the top stall reason. Low
warps-active with high long-scoreboard means the solver is memory-latency bound and wants more
resident work per SM (larger islands per workgroup, fewer dependent dispatches). High barrier
stalls mean the wavefront synchronisation is the cost. High SM throughput with issue-bound
stalls means the kernel is compute bound and the 600k target needs a different solver
formulation, not tuning. For `nn::train_sample` / `nn::weight_grad` the pass-level average says
whether the 35 µs dispatches leave SMs idle (a fusion lever) or not.

### What it answered

`vbd::solve::island` is **register-file bound**, and the binding cap sits far below full occupancy.
DRAM runs at 97.9 GB/s against the card's ~1.79 TB/s (about 5 %), L2 traffic is roughly 15x DRAM
traffic so the working set is L2-resident, the FMA pipe sits at 13.5 % of peak, and resident warps
issue 1.19-1.27 instructions per active cycle against a peak near 4. None of those ceilings is
close, so DRAM bandwidth, L2 bandwidth and arithmetic throughput are ruled out as levers: load
shaping, packing and arithmetic reduction cannot pay off on their own.

What is left is occupancy, and the async-queue limiter counters say exactly what caps it. All four
were sampled in one pass over the same windows, so dividing each by the warp figure gives the warp
occupancy at which that resource would be exhausted, with no need to know the part's absolute
per-SM limits:

| resource | % of peak, measured | warp-occupancy ceiling it imposes |
| --- | --- | --- |
| warp slots (`tpc__warps_active_shader_cs_queue_async_realtime`) | 14.862 | 100 % |
| register file (`tpc__sm_rf_registers_allocated_shader_cs_queue_async_realtime`) | 58.508 | **25.4 %** |
| CTA slots (`tpc__ctas_active_queue_async_realtime`) | 29.723 | 50.0 % |
| shared memory (`tpc__l1tex_sram_lines_mem_untagged_data_shared_allocated_compute_queue_async_realtime`) | 28.527 | 52.1 % |

`vbd_solve_iterations_stage`, the enclosing pass over the same work, reproduces all three ratios to
four digits (2.0000 / 3.9372 / 1.9197), so these are stable and not a sampling artifact.

Three separate losses, in order of size:

- **The register file caps warp occupancy at 25.4 %.** It is 3.94x as full as the warp slots are.
  Folding in the 64 KiB-register SM and either possible peak-warps figure for GB202 puts the kernel
  at 126-168 registers per thread, which is enormous. This is the single highest-leverage number in
  the whole measurement and it is invisible in every other counter.
- **One warp per CTA forfeits half the machine.** `ctas_active` is exactly 2.0000x `warps_active`,
  which is what a 32-thread workgroup gives when the hardware's peak warp count is twice its peak
  CTA count: CTA slots run out at half the warp slots. `Constraints.cppm:29` sets
  `workgroup_size = 32` and the dispatch is one workgroup per island, so this is by construction.
  It only becomes the binding cap once register pressure is fixed.
- **The dispatch fills only 59 % of even the register-allowed ceiling** (14.86 of 25.4 %). That
  residue is dispatch-level: 80 serialised dispatches per frame, islands of 1-17 bodies, so each
  dispatch ends in a tail where a handful of long islands hold slots that short ones have vacated.

One idea this kills: **trading registers for shared memory is already past break-even.** Shared is
2.56x more expensive per byte than the register file (100 KiB vs 256 KiB per SM), and at 58.5 % RF
against 28.5 % shared the optimum split would buy only 25.4 % -> 29.7 %. There is 1-2 KiB per CTA
sitting unused inside the allocation granule (2068 B declared, 3072-4096 B allocated), and
`island_mask_bits = 64` is 3.8x oversized against the largest real island of 17 bodies, so a little
free spill space exists — but reducing total live state is the lever, not moving it around.

Neighbouring stages on the corrected counters, warp occupancy / RF ceiling: `nn_chain_stage`
14.07 % / 31.7 %, `nn::train_sample` 18.76 % / 50.8 %, `vbd_state_copy_stage` 21.90 % / 58.1 %,
`vbd_post_stabilize_stage` 11.64 % / 28.4 %. Their CTA-to-warp ratios run 1.99-2.00 except
`vbd_post_stabilize_stage` at 1.88, so essentially the whole engine dispatches one warp per CTA and
gives up half the warp slots; the island solve is distinguished by being the stage where the
register file, not the CTA slot count, is the binding constraint.

Note also that from 2048 envs up the GPU solve is fully overlapped and the serial CPU chain sets
throughput, so a faster island solve only converts into steps/s once that chain moves.

One limit on how far to read these: the sampler is device-wide and attributed by time window, so a
row includes anything else running on the GPU in that window. That is acceptable in the headless
trainer where the solve dominates, but these are not single-kernel numbers under a mixed workload.

## Stage 2 (not planned, noted for later)

If pass-level averages prove too coarse, the Range Profiler in frameless mode with single-pass
configs and ranges from the existing pass markers (`RenderGraph.cpp` lines 620 and 748) gives
exact per-range counters at the cost of serialised ranges and command-stream changes on both
backends; that stage needs its own plan and both hash gates before touching `Commands.cppm`.

## Implementation, and where the plan was wrong

Files touched (all under `GSEngine/`):

- `vcpkg-overlays/ports/nsight-perf/{vcpkg.json,portfile.cmake,usage}`, `vcpkg.json`,
  `Engine/CMakeLists.txt`.
- `Engine/Engine/Import/Log.cppm` (`category::gpu_perf`).
- `Engine/Engine/Source/External/NsightPerf.cppm` + `.cpp`.
- `Engine/Engine/Source/Gpu/Device/Device.cppm` + `.cpp`, `Source/Gpu/Context.cppm` + `.cpp`.
- `Engine/Engine/Source/Gpu/Graph/RenderGraph.cppm` + `.cpp`.
- `Engine/Engine/Source/Diag/ProfileAggregator.cppm` + `.cpp`.

Corrections to the plan, each found by reading the tree:

- **No adapter LUID is exposed.** "Both backends already expose their adapter LUID" is false: the
  LUID appears only inside `Source/External/DirectX.cppm` for internal adapter matching, and
  nothing on `gpu::device`, `vulkan_device_backend` or `dx12_device_backend` surfaces one. Device
  selection is therefore an index — `session_settings::device_index`, fed by a new
  `Graphics.gpu_perf_metrics_device` setting, default 0. Matching by LUID is the better answer and
  wants a separate change that gives both backends an adapter-identity accessor.
- **`gse_copy_runtime_deps()` needs no change.** The whole vcpkg `bin` directory is already copied
  next to the executable, so the port installing `nvperf_grfx_host.dll` into `bin` is sufficient.
- **The plan missed the triplet passthrough.** A portfile sees only the environment variables a
  triplet lists, so `NSIGHT_PERF_SDK` had to join `NSIGHT_AFTERMATH_SDK` in
  `VCPKG_ENV_PASSTHROUGH` in both `vcpkg-overlays/triplets/x64-mingw-static-release.cmake` and
  `x64-windows-release.cmake`; without it the port would always install the stub and report the
  variable unset even when it is set. A triplet file's contents are part of every port's ABI hash,
  so the next engine reconfigure rebuilds all vcpkg dependencies once. The `bin`-only DLL install
  is already legal here: the mingw triplet sets `VCPKG_POLICY_DLLS_WITHOUT_LIBS`, which is what a
  shim-loaded DLL with no import library needs.
- **`GSE_HAVE_NSIGHT_PERF` is not defined yet.** The CMake block links the target when it exists
  and logs that the sampler layer is unimplemented. Defining the macro with an empty SDK body
  would make `compiled_in` lie and would turn the first machine with the SDK installed into a
  build failure. Define it in the same commit that fills in the body.
- **`profile.txt` gets rows, not columns.** The GPU time table is built from
  `profile::report_entry` by the shared `write_section`; widening it for counters would corrupt the
  CPU table too. Counters are a separate section, `GPU hardware counters (mean over sampled
  passes)`, one row per pass × metric with the mean value and the contributing sample count. The
  aggregator entry point is `profile::ingest_gpu_metrics(id pass_id, std::span<const double>
  values, std::uint64_t samples)` plus `profile::set_gpu_metric_names(std::span<const
  std::string>)`, not the planned `ingest_gpu_metric(id, metric_index, value)`; one call per row
  keeps the lock count at one per pass instead of one per metric, and the name list is published
  once per session rather than per row.
- **The render graph keeps no fourth enabled flag.** Enablement is derived from
  `nsight_perf::info().status == running`, which is already gated on
  `gpu_perf_metrics_enabled && gpu_timestamps_enabled` where the setting is read in
  `context::run`. `session_info::generation` (bumped by every `begin_session`) invalidates both
  the cached metric ids and the published metric names from one fact.
- **`calibrate()` became `gpu_timestamp()`.** The sampler can report the GPU clock; pairing it
  with a CPU stamp is the render graph's job, and it already holds the fallback offset
  (`cpu_ref - timestamps[0] * period`) for when there is no session. One function, no struct.
- **`decode()` returns a view, not an out-span.** `sample_window { std::span<const
  time_t<std::uint64_t>> times; std::span<const double> values; }` over buffers the module owns, so
  `max_samples_per_frame` is not part of the interface and nothing allocates per frame. `values`
  is row-major, `metrics.size()` doubles per sample.
- **`decode()` is called once per frame, and the window is shared by every queue.** An earlier
  revision called it once per profile slot on the theory that idle queues return early and the
  graphics queue would be read first. That is wrong whenever more than one queue is busy: `decode()`
  clears its sample buffers on entry, so the first slot to run drains the window and every later
  slot sees an empty one. In the headless trainer that meant the compute queue — which carries every
  physics mark, including the pass this instrument exists to measure — got no attribution at all.
  `execute` now opens one `perf_frame` and passes it to all three `read_profile_slot` calls.

### What the SDK body actually needed

`Source/External/NsightPerf.cpp` is a plain module implementation unit with the SDK headers in its
global module fragment. It uses the header-only `NvPerfUtility` C++ layer rather than the raw
`NVPW_*` C API wherever one exists, which collapsed most of the call sequence the plan sketched
into `nv::perf::sampler::GpuPeriodicSampler`, `MetricsEvaluator`, `MetricsConfigBuilder` and
`RingBufferCounterData`.

Corrections to the sketch above:

- MinGW needs `#include <x86intrin.h>` ahead of the SDK headers. `windows.h` reaches `winnt.h`,
  which includes `<x86intrin.h>` inside an `extern "C"` block; the intrinsics then collide with the
  C++-linkage copies reachable through `import std` / `import gse.math` ("conflicting language
  linkage for imported declaration `_mm_set_epi64x`", ~60 errors). Including it first at global
  scope sets the include guard and keeps the linkage C++.
- `NVPW_D3D12_LoadDriver` must be called after `NVPW_InitializeTarget` and before
  `IsGpuSupported`, which otherwise returns `NVPA_STATUS_DRIVER_NOT_LOADED`. It takes no arguments
  and no D3D12 headers (the SDK forward-declares `ID3D12Device`), and the engine already links
  d3d12 on Windows. The counters are device-wide, so the D3D12 connection serves the Vulkan backend
  too; `NVPW_VK_LoadDriver` would need a `VkInstance` plumbed through `session_settings`.
- The periodic sampler only reads counters that have a `*_realtime` variant. The plan's default
  list (`sm__throughput.avg.pct_of_peak_sustained_elapsed` and friends) resolved fine but needed
  24 passes. The shipped `NvPerfUtility/data/MetricConfigurations/gb20x/Top-Level_Triage.config.yaml`
  is the ground truth for which counters this chip can sample and how NVIDIA groups them one pass
  at a time.
- Those YAML entries are raw counter names and are *not* valid evaluator metric names;
  `ToMetricEvalRequest` rejects them with `NVPA_STATUS_INVALID_ARGUMENT`. A name needs a rollup and
  submetric (`<counter>.avg.per_cycle_active`). `suggest_metric_names` in the implementation now
  enumerates the device and prints the valid spellings whenever a configured name is rejected.
- `metrics_fitting_one_pass` reports the longest prefix of the metric list that fits a single pass,
  so a `config_needs_replay` failure names the metric that broke the budget rather than only the
  pass count.
- `sampler::SampleTimestamp` + `CounterDataGetSampleTime` supply the per-sample GPU timestamp that
  the plan flagged as the blocking unknown.
- `MetricsConfigBuilder::AddMetrics` takes optional `RawCounterSchedulingHints`. The shipped
  configurations use them to pack a group into one pass; the implementation does not, which is why
  the default list is kept small.
- `GpuPeriodicSampler::BeginSession` defaults its append mode to
  `RECORD_BUFFER_APPEND_MODE_KEEP_OLDEST`, in which the driver stops recording permanently on the
  first overflow — and `GetRecordBufferStatus`'s `overflow` flag is documented as only ever being
  true in that mode. The engine reaches its first `read_profile_slot` seconds after `begin_session`,
  which is long enough to fill any buffer, so keep-oldest could never survive startup here. The
  implementation queries `GpuPeriodicSamplerIsKeepLatestModeSupported` and opens the session in
  keep-latest when it can, and `skip_to_newest_records` recovers from a lap or from a decode that
  stops on `UNEXPECTED_RECORD` / `OUT_OF_ORDER_RECORD` / `EXCESSIVE_BACKPRESSURE`. Recovery needs
  both halves: `SetRecordBufferReadOffset` moves only the CPU-side offset and
  `AcknowledgeRecordBuffer` only the GPU-side one.
- `buffered_samples` sizes both the counter data ring and, via
  `GpuPeriodicSamplerCalculateRecordBufferSize`, the record buffer. Because a sample is ~17.9 KB on
  this chip, the current 65536 is a 1.17 GB allocation. That is the knob to reconsider before the
  sampling interval if memory matters more than history depth.

Known gap: `GpuPeriodicSampler::BeginSession` returns `bool` and swallows the `NVPA_Status`, so a
permission failure surfaces as `start_failed` with the SDK's own forwarded line carrying the
`ERR_NVGPUCTRPERM` URL, rather than as `no_permission` with its remedy. Mapping it properly means
calling `NVPW_GPU_PeriodicSampler_BeginSession_V2` directly and duplicating the wrapper's trigger
validation and bookkeeping.

## Review guide checklist for the implementer

- Ids are `gse::id` derived once and cached; no per-frame string construction.
- SDK types never cross the module interface; POD mirrors only, as in `gse.aftermath`.
- Enumerators carry their log labels as annotations; no parallel string tables.
- Time values are `time_t<…>`; the calibration pair is the one place raw ticks enter and it is
  local to `calibrate()`.
- One owner: the sampler lives on `gpu::device`, the render graph reads it, the context system
  flips the setting. No second copy of the enabled flag.
- Every failure path returns a status and logs once; nothing throws or aborts the trainer.
- Clock lock restored on every exit path, verified by reading the clock status after
  `end_session` in the gate.
- No comments in code. Builds only through `Tools/gse-build`; state plainly if a claim was not
  compiled.

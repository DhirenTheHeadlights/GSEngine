# Goal: Intel DX12 `vbd_solve_color` crash — fixed, not diagnosed

## Aim
Make `scene_reset` run to completion on the Intel DX12 machine. Keep the GPU VBD solver path. Keep going across as many tasks as it takes; do not stop at a diagnosis and hand back a new task.

## Pass criteria
1. `gse_run scenario=scene_reset` on the Intel DX12 box runs all 240 frames and exits cleanly, 3 runs in a row, with the same world-state hash every time.
2. That hash matches the Vulkan `scene_reset` hash on the same commit. If it doesn't match, the cause is understood and written in the ledger (for example, it's known to be the expected floating-point divergence between backends).
3. No physics behaviour is removed to get there. Joints, muscles, contacts, motors and island sweeps all still run on the GPU.
4. The release build is clean.

## Charter (no need to ask)
- Edit `Engine/Resources/Shaders/Bodies/VBDPhysics/*.slang`, especially `vbd_solve_color.slang` and the headers it includes.
- Edit the GPU solver dispatch code in `Engine/Engine/Source/Physics/VBD/GpuSolver.*`.
- Bisect with temporary experiments: compile out a branch, `[loop]`/`[unroll]` attributes, splitting shaders. Revert every experiment before closing a task, and record each one in the ledger.
- Build and run scenarios as needed. Turn on diagnostics with `settings` overrides only. Never edit settings.ini.
- Remove `Physics.gpu_direct_dispatch` and its guards if they still work around nothing.

**Off limits without asking:** the CPU solver, scenario definitions and expected hashes, any other renderer or backend code, and filing external driver bugs.

## Leading suspects (from reading the shader)
- **`int64_t` local arrays with dynamic indexing** (`fx_grad`/`fx_ang_grad` through `fx_add_vec3`). Intel's int64 + indexable-temp support in DXIL is a known weak spot. **Try this first:** replace them with `int64_t3` or scalar accumulators, or with an int2 hi/lo pair.
- **`solve_body` is inlined at three call sites** (both island sweeps plus the main path), each pulling in all of the joint, muscle and wrap code. That would explain the 17.3 s compile and a driver JIT blow-up on the CPU. **Fix:** keep one call site by branching on the sweep direction, or split the island and per-color pipelines.
- **Heavy groupshared use** (roughly 15 arrays of size `island_slots`): check it against Intel's limit.

## Steering order
1. Bisect by compiling out (contacts, joints, muscles, island path, fx accumulators) until the crash goes away.
2. Restructure the part that trips the driver without changing the maths.
3. Verify against the pass criteria.

## Ledger
- 2026-10-30 (task 92, all experiments reverted). Baseline: 30 of 34 `scene_reset` runs crash in `igd12um64xe2.dll+0x122b2ab` at solver dispatch, so judge every experiment on at least 5 runs. Full-shader passing hash `0x898657a046b0a396`, deterministic across passes.
  - `int64_t[3]` → `int64_t3` accumulators: still crashes at the same address. **Refuted.**
  - Holding the first dispatch 60 s after pipeline creation: still crashes. **Refuted** (not the background compile).
  - Muscle path compiled out: 1 pass, then a crash. **Inconclusive, leaning refuted.**
  - Joints and muscles out, or muscles out alone: sometimes a GPU fence hang. **Unexplained** (may also explain the ~60 s `shutdown::systems` stall).
  - Whole `solve_body` body out: 1 pass, hash `0x2759e19b273c4e70`, compile 15 s → 1.3 s. **Single run, suggestive of size/register pressure.**
  - Untested hypothesis: register spill plus scratch sharing across command lists recorded in parallel. A single-thread recording test needs backend or task-pool code, which is outside the charter.
- 2026-10-30 (task 93, change kept in tree, not reverted). Island path split into its own `vbd_solve_islands` pipeline; one `solve_body` call site per pipeline; `[loop]` on contact/joint/muscle/wrap loops.
  - Groupshared: ~7 KB at `island_slots = 96`, far under 32 KB. **Refuted by arithmetic.**
  - Driver compile: `vbd_solve_color` 15 s → 2.0–2.6 s; `vbd_solve_islands` 5.1–5.8 s.
  - 2 DX12 `scene_reset` runs: neither hit the `igd12um64xe2.dll+0x122b2ab` access violation at dispatch. Both instead hung the GPU (`DEVICE_HUNG` 0x887a0006) around frame 65–70 after a 3–9 s `engine::update` watchdog stall; the bench still reported 240 frames with hash `0x7e6b4347e6e5e573` on a removed device, so that hash is meaningless. **0 of 2 passes.**
  - DRED (run 2): the first incomplete compute op is the `vbd_predict` dispatch, and every later list, the `vbd_solve_iterations` list included, completed 0 ops. That clears neither predict nor the solve, because DRED marks an already-hung queue's next op.
  - **The second run's hang left the owner's machine needing a reboot.** The owner then said to continue.
  - Bisect (reverted): pipeline split kept, original multi-call island body put back. Islands compile went back up to 13–16 s. 1 pass (hash `0x898657a046b0a396`), then 1 hang. **The unified loop is not the cause of the hang.**
  - Kept change, sleep on: one more run passed with hash `0x898657a046b0a396`, identical to the pre-change passing hash. **The restructure preserves the maths.**
  - Sleep off (`Physics.velocity_sleep_threshold=0 m/s`): original-loop build 2 of 2 passed (hash `0x291fe17184cf46df`); kept change 1 hang. **Sleep is refuted as the trigger.**
  - Tally across task 93: 8 DX12 runs, 4 passes, 4 GPU hangs, **0** of the original `igd12um64xe2.dll+0x122b2ab` access violations (baseline 30 of 34). The CPU-side driver crash looks fixed. What remains is an intermittent GPU hang (`DEVICE_HUNG`), which happens in the strike/collapse window (frames ~65–70). It was already seen in task 92 as the "fence hang". When the GPU hangs, the bench still finishes and reports `0x7e6b4347e6e5e573`, so that hash means the device was removed.
  - Next: find which pass hangs. Use `Graphics.device_settings.pass_checkpoints=true` (DRED alone blamed the `vbd_predict` dispatch, which is bounded code). Each hang may need a reboot on this machine.

## Assumption
The Intel machine is the one `gse_run` uses. If it isn't, steering escalates.
- 2026-10-30 (steering after task 93). Judged task 93 against the criteria: criterion 1 fails (4 of 8 runs hung with `DEVICE_HUNG`), and criteria 2 and 4 have not been checked. **Not met.** Next task: find which pass hangs using `pass_checkpoints`, then fix the hang.
- 2026-10-30 (task 94, all experiments reverted; only the dead `color_offset` writes removed). Hang **not fixed**.
  - Baseline with `pass_checkpoints=true`: 3 passes (`0x898657a046b0a396`), then a hang at frame 189 (run `20261030_131745`). The only pass in flight was `vbd_predict_stage` (substep 0); the passes before it in the chain, build_adjacency and build_coloring, were marked complete. Hangs happen at varying frames (≈65–70, 146, 189) while the hash stays deterministic, which points at a timing or race cause, not at bad data.
  - Caveat: on DX12 the checkpoint is `WriteBufferImmediate` in mode DEFAULT, which is not ordered after earlier dispatches, so "completed" can be premature. The fix is MARKER_IN/OUT in `Dx12/Native/Device.cpp`, which is outside the charter, so it was not made. **Owner's call.**
  - E1, per-wave groupshared in `collision_build_coloring` sized for wave 8 (`/8` instead of `/32`): failed on run 1 (watchdog stall, process died). **Refuted** as the hang cause (the wave size on this driver is still unconfirmed).
  - E2, predict's sleep-wake neighbour loops compiled out: 1 pass (hash unchanged), then a hang on run 2. **Refuted.**
  - E4, predict made a no-op: 1 "pass" with hash `0x2759e19b273c4e70`. Nothing moves, so the strike window never happens. **Confounded; proves nothing.**
  - `Physics.gpu_direct_dispatch=true` (setting override, no ExecuteIndirect): hang on run 1. **ExecuteIndirect refuted.** `gpu_direct_dispatch` still works around nothing, so it can be removed.
  - New symptom: in 3 of these failures the process died partway through the watchdog stack dump, with no device-lost record. Either something outside the process killed it, or the driver killed it at TDR.
  - Hang rate this task: 5 of 9 runs. Remaining hypotheses: (a) the task-92 one, scratch/register spill shared across command lists recorded in parallel, which needs task-pool/backend changes outside the charter; (b) a lifetime race on a CPU-written upload channel that predict reads (`solver_config`, `motor_map`, `motor`), since Intel reports page faults as `DEVICE_HUNG`. Next useful experiments: run with single-threaded recording, and turn on DX12 GPU-based validation or page-fault DRED (`Graphics.validation_layers_enabled=true`). Both are likely to need backend edits, so escalate.
  - `Graphics.validation_layers_enabled=true` (debug layer, GPU-based validation off): hang on run 1 (`20261030_133247`), with no validation errors, only "barrier-only command list" warnings. **The editor died at the same moment**, so a hang can take the editor down as well. Next would be GPU-based validation, which has no setting and needs a backend edit.
- 2026-10-30 (steering after task 94). Criterion 1 fails: 5 of 9 runs hung with `DEVICE_HUNG`. Criteria 2 and 4 have not been checked. **Not met.** Both remaining hypotheses, and the checkpoint ordering fix, need edits outside the charter: single-threaded recording, GPU-based validation or page-fault DRED, and MARKER_IN/OUT in `Dx12/Native/Device.cpp`. **Escalated** to ask the owner for that authorisation.
- 2026-10-30 (steering after escalation). The owner approved the first two backend diagnostics: (1) GPU-based validation or page-fault DRED, and (2) MARKER_IN/OUT checkpoints, both in `Dx12/Native/Device.cpp`. They did **not** approve single-threaded recording. Nothing was run. Next task: make both diagnostic edits, then rerun the hang with them on to find which pass hangs and whether it is a page fault.
- 2026-10-30 (task 95; diagnostic kept, no experiments). Hang **not fixed**.
  - Page-fault DRED (`Graphics.device_settings.dred`) and GPU-based validation (`Graphics.device_settings.gpu_based_validation` together with `validation_layers_enabled`) already existed as settings, so only overrides were needed. The task-94 note that GBV had no setting was wrong.
  - Kept: DX12 pass checkpoints now use `WriteBufferImmediate` in MARKER_OUT mode (`External/Native/DirectX.cppm`). A marker lands only after all earlier work in its list has finished, so "begin reached, body did not finish" now means that pass really was executing.
  - Kept: `Physics.gpu_direct_dispatch` removed, along with `indirect_slot` and the shader early-outs it guarded. Hash unchanged (`0x898657a046b0a396`).
  - 5 runs with `pass_checkpoints` and `dred` on: 3 passes (`0x898657a046b0a396`), 2 hangs (`DEVICE_HUNG`).
    - Run `20261030_144605`, frame 14: the in-flight pass is `vbd_build_coloring_stage` (substep 0; round/commit loop plus single-workgroup `collision_build_coloring`).
    - Run `20261030_144731`, frame 85: the in-flight pass is `vbd_predict_stage` (substep 0), after 3–6 s `engine::update` watchdog stalls. **The editor died with this hang.**
  - **No DRED page fault in either hang.** Hypothesis (b), an upload-channel lifetime race, is **not supported**.
  - Ordered checkpoints name two different, bounded passes. That rules out one runaway shader as the cause: the GPU stalls in whatever pass is executing at the time. Remaining hypotheses: a driver/hardware fault triggered by register or scratch pressure; or a cross-queue or fence wait that never resolves. A wait is unlikely, since DRED shows the compute list itself incomplete.
  - Suspect noted for the next bisect: `collision_build_coloring` sizes its per-wave groupshared for wave 32 (`adjacency_workgroup_size / 32`). If Intel runs it at wave 8 or 16, `gs_wave_color_count` and `gs_wave_jointless_*` are written out of bounds. The corrupted `gs_color_count` could then drive the `kb < full_count` loop very long. Task 94's E1 (`/8`) had only one run, so this is unrefuted. Querying `WaveGetLaneCount` on this driver would settle it.
  - GPU-based validation run: not done. The editor had just died with hang 2.
- 2026-10-30 (steering after task 95). Criterion 1 fails: 2 of 5 runs hung with `DEVICE_HUNG`. Criteria 2 and 4 have not been checked. **Not met.** Next task: settle the wave-size suspect in `collision_build_coloring`. First record `WaveGetLaneCount` on this driver. Then size the per-wave groupshared arrays for the smallest possible wave, or guard them, without changing the maths. Judge it on at least 5 runs with checkpoints on, and run GPU-based validation once.
- 2026-10-30 (task 96, experiment reverted, no change kept). Wave-size suspect **refuted**.
  - Experiment: `collision_build_coloring` wrote `WaveGetLaneCount()` into `collision_state[state_coloring_conflict_index]`, which reads back as `coloring_conflicts`. Run `20261030_150937` (pass, hash `0x898657a046b0a396`): the peak was 32, with total 1184 over 37 colouring ticks, so **32 lanes on every dispatch**. `max_waves_per_group = adjacency_workgroup_size / 32` fits exactly, and nothing is written out of bounds. Reverted.
  - Gotcha: VBD shaders are compiled from `Engine/Resources` source when the process boots. A `gse_run` launched before a source edit can still pick up the edit, and a launch over an unbuilt edit did not start at all. Build first, and revert only after the run exits. One run (`20261030_150238`) accidentally measured the reverted shader (conflicts 0); it passed with hash `0x898657a046b0a396`.
  - Nothing was resized, so no 5-run judgement or GBV run was needed for this task. Tally this task: 2 runs, 2 passes, 0 hangs.
  - Remaining hypothesis for the intermittent `DEVICE_HUNG`: a driver/hardware fault under register or scratch pressure, possibly tied to parallel command-list recording. Single-threaded recording is still unapproved. GPU-based validation is still not run.
- 2026-10-30 (steering after task 96). Wave-size suspect refuted (32 lanes on every dispatch); 2 of 2 runs passed this task, which is too few to say the hang rate changed. Criterion 1 still fails on the task-95 record (2 of 5 hung); criteria 2 and 4 are unchecked. **Not met.** Next task: GPU-based validation, which needs only setting overrides and so falls inside the charter, run until a hang is caught or 5 runs pass. Also check criterion 4 (release build clean). Single-threaded recording is still unapproved.

- 2026-10-30 (task 97; one fix kept, no experiments). Hang **not fixed**.
  - GBV run 1 (`20261030_151513`, before the fix): passed, hash `0x898657a046b0a396`. One real finding, 240 times (once per frame): `vbd_gather_snapshot` read a **UAV descriptor through an SRV register** ("Descriptor type doesn't match shader register type"). `snapshot_source` was declared `ssbo_readonly` (a `StructuredBuffer` in the shader), but `m_body_buffer.slot()` is the body buffer's UAV slot.
  - Kept: `snapshot_source` is now `ssbo_readwrite` in `GpuSolver.cpp`, which matches every other binding of `m_body_buffer`. The hash is unchanged.
  - GBV runs 2–5 after the fix (`152054`, `152450`, `152833`, `153240`): 4 of 4 passed, hash `0x898657a046b0a396`, and the only validation warnings left are the known barrier-only ones. **Totals: 5 of 5 GBV runs passed, 0 hangs, no GBV errors after the fix.** That **refutes resource-state, barrier and out-of-bounds misuse** as the cause. Caveat: GBV slows frames a lot (p95 62–182 ms), and the hang is timing-dependent, so 0 hangs under GBV does not prove the rate changed.
  - Every run stalls ~150–170 s in `shutdown::systems` and then still exits with "bench: PASSED". This is the same known stall.
  - Judgement after the fix, GBV off, checkpoints and DRED on: run 1 (`20261030_153635`) hung at once. It hit an `engine::update` watchdog stall ~3.4 s into measurement, and the process died partway through the stack dump with no device-lost record (the task-94 symptom). The editor restarted as well (pid 18884 → 19176). **The descriptor fix does not cure the hang.** The other 4 judgement runs were not made, because each hang risks the owner's machine.
  - Criterion 4: the `x64-mingw-gcc-Release` build succeeded, with 0 errors attributed to this chat.
  - What's left: the hang only shows up at full speed, and nothing in GBV, DRED page faults or ordered checkpoints points at application misuse. The remaining hypothesis is a driver/hardware fault under register or scratch pressure, possibly with parallel recording (single-threaded recording is still unapproved).
- 2026-10-30 (steering after task 97). Descriptor fix kept (hash unchanged); GBV 5/5 clean, but the first full-speed run after the fix hung, so criterion 1 still fails. Criterion 4 met (Release clean). Criterion 2 unchecked. **Not met.** Every in-charter diagnostic (checkpoints, DRED page faults, GBV, wave size, int64, ExecuteIndirect, sleep, pipeline split) is now exhausted without finding app misuse; the only remaining hypothesis needs single-threaded recording, which is unapproved. **Escalated.**
- 2026-10-31 (steering after escalation). Owner picked option 1: approved a temporary single-threaded command-list recording experiment in task-pool/backend code (revert after, unless a narrower serialization is then designed). Further 5-run judgements are implicitly consented by this approval. Nothing run. Next task: serialize recording, run scene_reset at full speed with checkpoints+DRED until a hang or 5 passes.

- 2026-10-31 (task 98, experiment reverted). Parallel-recording hypothesis **refuted**.
  - Experiment: `record_range` in `Gpu/Graph/RenderGraph.cpp` recorded every pass's command list serially on the calling thread (a plain loop replacing `task::parallel_invoke_range`). Submit order and GPU work were unchanged. The build succeeded.
  - Run 1 (`20261031_163309`, checkpoints and DRED on, no GBV): an `engine::update` watchdog stall 3 s in, about 4.6 s into measurement. The process then died partway through the stack dump, with no device-lost record. This is the same hang symptom as tasks 94 and 97. **0 of 1 passes.** Stopped at the first hang, as planned.
  - Recording single-threaded does not prevent the hang, so scratch sharing across command lists recorded in parallel is not the cause. Reverted, with the hunk restored byte for byte.
  - Every hypothesis the charter and the escalations allowed is now exhausted without finding app misuse: checkpoints, DRED page faults, GBV, wave size, int64, ExecuteIndirect, sleep, pipeline split, the descriptor fix, and serial recording. What's left points at a driver or hardware fault on this Intel DX12 box under the solver's workload.
- 2026-10-31 (steering after task 98). Serial recording refuted (0/1, same hang). Criterion 1 fails; criterion 2 unchecked; criterion 4 met. **Not met.** All in-charter and approved diagnostics exhausted with no app misuse found; remaining cause looks like an Intel driver/hardware fault. **Escalated** for a decision outside the charter (driver update/rollback, filing an Intel bug, or reducing per-dispatch workload).

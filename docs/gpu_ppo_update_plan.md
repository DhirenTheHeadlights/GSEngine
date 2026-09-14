# GPU PPO update for the locomotion trainer: design (2026-09-12, draft for approval)

## Status

- **Step 0 done (2026-09-12 evening).** `trainer::frame` (`[[= system_frame{}]]`, game side)
  records one 4096-thread dispatch of `Bodies/Nn/nn_smoke.slang` on `queue_type::graphics` in
  chain `gpu_update_chain`, copies the result into a `readback_channel` and verifies every
  generation on the host. Headless, both backends, flag `--ppo-gpu-update`:

  | backend | smoke result | state hash (1024 envs, 100 updates) |
  |---|---|---|
  | Vulkan | initialized, generation 1 verified, 0 mismatches over the run | `83219603…` = reference |
  | DX12 | initialized, generation 1 verified, 0 mismatches over the run | `D02B4F03…` = reference |

  Decision 1 is therefore feasible: the headless graphics queue accepts compute passes without a
  swapchain. Build note: `compute_entry<...>` aliases and shader binding structs must live in an
  implementation partition (`Source/Locomotion/LocomotionGpuUpdate.cpp`), not in the exported
  trainer interface, or the umbrella `gs.cppm` re-export fails to compile with no diagnostic.
- **Step 1 done (2026-09-12 late evening).** Family `Nn` (`Shaders/Nn/nn_shared.slang` layout
  helper + bodies `nn_forward`, `nn_output_delta` (mode 0 PPO / 1 MSE / 2 LSGAN),
  `nn_backprop`, `nn_weight_grad` (one thread per parameter, fixed sample order, no atomics),
  `nn_adam`, `nn_reduce_loss`). Three bindings (`nn_state` = params|m|v, `nn_inputs`,
  `nn_scratch`) plus an 80-byte push constant; dims travel in the push constant so one pipeline
  set serves every net. Unit parity harness (`run_parity` in `LocomotionGpuUpdate.cpp`, runs
  once under `--ppo-gpu-update`): one 256-sample actor step and one critic step on copies of
  the live nets, CPU vs GPU, random inputs, seed 1234:

  | backend | net | max abs param diff (ref magnitude) | max abs Adam m / v diff | loss CPU / GPU |
  |---|---|---|---|---|
  | Vulkan | actor (21780 params) | 7.5e-9 (7.1e-2) | 9.1e-6 / 2.3e-5 | 26.612562 / 26.612558 |
  | Vulkan | critic (20609) | 3.7e-9 (3.2e-2) | 1.0e-7 / 4.2e-9 | 29.786905 / 29.786907 |
  | DX12 | actor | 2.5e-7 (7.1e-2) | 1.6e-4 / 3.4e-4 | 26.612562 / 26.612442 |
  | DX12 | critic | 3.0e-8 (1.5e-1) | 1.0e-6 / 3.6e-8 | 29.786905 / 29.786890 |

  All within float summation-order noise (DX12's `tanh`/`exp` lowering is a little further from
  the CPU polynomial than Vulkan's). Exe `HumanoidLocomotion.nnparity.exe`.
- **Step 2 done (2026-09-12, 22:55).** Added bodies `nn_adam_prep` (Adam step counter lives in
  `state[3P]`, bias corrections and the LSGAN gate flag in the scratch scalars, so the CPU never
  has to know whether a discriminator step applied), `nn_r1_sample` + `nn_r1_grad` (R1 double
  backprop, one workgroup per real sample then one thread per parameter, folded into the LSGAN
  gradient with `r1_weight·total/n_real`), `nn_style_reward` (reward blend in place),
  `nn_gae` (one thread per env over CPU-built per-env index lists), `nn_adv_stats` /
  `nn_adv_norm`. `nn_inputs` is now read-write; the push constant carries explicit region
  offsets so one pipeline set serves actor, critic, discriminator and the rollout buffer. Same
  parity harness, one 256+256 LSGAN step with R1 and 4096-sample style/GAE/normalisation:

  | backend | disc params (23169) | disc m / v | disc loss, mean_real, mean_fake, r1 | reward / adv / return max diff |
  |---|---|---|---|---|
  | Vulkan | 3.0e-8 (ref 2.8e-1) | 1.3e-7 / 5.1e-9 | all equal to 6 digits | 1.9e-6 / 1.2e-6 / 7.6e-6 |
  | DX12 | 6.0e-8 | 3.4e-7 / 1.4e-8 | all equal to 6 digits | 2.6e-5 / 6.3e-6 / 4.6e-5 |

  Mean style, advantage mean and std equal to 6 digits on both. Short-run hashes with
  `--ppo-gpu-update` unchanged from the step-1 build (9E55BF49 vk / E1234CE4 dx12), so the CPU
  training path is untouched. Exe `HumanoidLocomotion.nnparity2.exe`.
- **Step 3 done (2026-09-12, 23:15).** The whole update is one graphics-queue pass recorded from
  the trainer's `system_frame` hook (`record_chain` in `LocomotionGpuUpdate.cpp`): zero scalars,
  first-time state upload, `disc_updates` × LSGAN+R1 step on CPU-sampled real/fake rows, style
  reward blend, GAE, then `n_epochs` × contiguous minibatches × actor+critic, followed by three
  readbacks (actor, critic+disc states, rollout scalars). The recording context's per-buffer
  hazard tracking supplies the barriers between dispatches. Net states stay resident on the GPU
  and are read back each update so checkpoints and the inference snapshot keep working; the
  run-hook wait spins on `gpu_poll_update` and falls back to the CPU update only if the chain was
  never recorded before the next rollout filled (counted and logged per update). 10 updates at
  1024 envs on both backends complete through the chain with plausible losses (actor ≈ −0.005,
  critic ≈ 25, D_loss ≈ 0.65, mean_style ≈ 0.65), no fallbacks. Run-to-run determinism 3/3
  identical state hashes on both backends (Vulkan 1C01981C…, DX12 C586C2B3…). Exe
  `HumanoidLocomotion.gpuchain.exe`.
- **Step 4 learning parity passed (2026-09-12, 23:30).** Seeds 2/3/4 at 1024 envs, 20 M samples,
  `--ppo-gpu-update` vs the carry1024 CPU rows:

  | row | surv_last5M | eval_mean |
  |---|---|---|
  | carry1024_s2 / s3 / s4 (CPU update) | 73.7 / 69.2 / 69.8 | 76.8 / 83.8 / 76.0 |
  | gpu1024_s2 / s3 / s4 (GPU update) | 72.9 / 73.2 / 71.7 | 68.0 / 69.6 / 89.6 |

  Zero fallbacks and zero stalls across all three runs. Throughput was a wash at 1024 envs and
  a loss at 2048 (72 ms per-update stall): the chain occupied the GPU ~25 ms per update and
  displaced the compute-queue solve.
- **Kernel pass (2026-09-13, 00:05).** Per-dispatch GPU marks (`rec.mark`, enabled with
  `Graphics.gpu_intra_pass_marks_enabled=true`) put the cost in `nn_weight_grad` (74 µs), the
  stride-128 weight reads in `nn_forward` (51 µs) and launch floors (~7 µs per dispatch,
  7 dispatches per net step). Changes, all bit-identical to the gated build (Vulkan state hash
  91950805… at 50 updates unchanged): the forward reads a transposed weight copy that lives in
  the state buffer after the Adam moments and is refreshed by `nn_adam` / `nn_transpose`;
  forward + output delta + backprop are one per-sample kernel (`nn_train_sample`); the loss
  reduction and Adam bias-correction prep ride as an extra workgroup of `nn_weight_grad`. A
  net step is now 3 dispatches. Chain GPU time 25.0 → 7.5 ms per update at 1024 envs
  (`nn_train_sample` 24 µs, `nn_weight_grad` 23 µs, `nn_adam` 8 µs). A tiled weight-gradient
  kernel was tried and rejected: 4× slower than one thread per parameter because every row
  group re-read the whole minibatch. Exe `HumanoidLocomotion.gpuchain7.exe`.
- **Default flipped (2026-09-13, 00:25).** A second 2048-env stall turned out to be the sample
  cap (4096) making every 8192-sample update fall back to a synchronous CPU update; the cap is
  8192 and a declined shape now takes the asynchronous CPU job. Interleaved A/B, same exe,
  4 M samples per arm: 1024 envs 41.9/40.7 s CPU vs 38.3/37.3 s GPU (+8 %), 2048 envs
  49.0/45.5 s vs 40.3/38.9 s (+16 %). `gpu_update` defaults to true; `--no-ppo-gpu-update`
  reproduces the CPU refs (`83219603…` / `D02B4F03…`) on both backends. New default refs at
  409600 steps: Vulkan `AC3D5339…` (2/2 identical), DX12 `F838905F…`. Owner decisions 1–3
  remain as taken: graphics queue, engine `Nn` family, buffers sized for 8192 samples.
- **Adam fused into the weight gradient (2026-09-13, 00:35).** For the actor and critic each
  parameter thread of `nn_weight_grad` now applies the Adam step in place (moments, weight,
  transposed copy) and the reduce workgroup writes the step counter from a new push-constant
  field `adam_step`, which the trainer tracks per net and seeds from the CPU optimizers. The
  discriminator keeps the separate `nn_adam` dispatch because its apply gate depends on the
  reduced loss. A net step is 2 dispatches for the actor and critic. Bit-identical on both
  backends: Vulkan `91950805…` at 204800 steps with `--ppo-gpu-update`, default `AC3D5339…`,
  DX12 `F838905F…`. Chain GPU time 7.96 → 6.51 ms avg per update at 1024 envs
  (`nn_weight_grad` 25 µs incl. Adam, `nn_train_sample` 23 µs). Exe
  `HumanoidLocomotion.gpuchain9.exe`. Older exes (`gpudef`, `gpuchain7/8`) no longer run: the
  disk shaders reference `pc.adam_step`, which their `nn_push` lacks.
- **Per-net scratch and read-only bindings (2026-09-13, evening).** At 8192 envs the chain
  costs 19.7 ms of GPU per rollout frame (128 minibatches × 4 epochs × 2 nets × two ~40 µs
  latency-bound dispatches). Each net now owns its scratch buffer, `nn_train_sample`,
  `nn_forward` and `nn_r1_sample` bind `nn_state`/`nn_inputs` read-only, and the minibatch
  records both `train_sample` dispatches before both `weight_grad` dispatches. On its own this
  changes nothing measurable: the recording context still barriers before every dispatch
  because it never learns that a flushed barrier covered earlier writes. The tracker change
  that lets the actor and critic pairs overlap is `gpu_record_barrier_coverage_plan.md`.

## Why now

HumanoidLocomotion, 1024 envs, GPU VBD solver, asynchronous PPO, 15 worker threads, carry-over
rollout (the default since 2026-09-12 evening): ~96.7k steps/s, cycle ~43.5 ms for 4096 samples.

What binds the cycle today, measured (profile `HumanoidLocomotion.carrydef.35368`, CPU floor
probes, skeleton solve probe):

| item | cost | evidence |
|---|---:|---|
| asynchronous PPO update, wall | ~32 ms per update | `locomotion::async_update` avg 31.9 ms |
| the same update, thread-time | ~250 ms per update | forward 76, actor backward 64, critic backward 61, disc R1 26, disc backward 12, serial `grad_reduce` 3.2 + `adam_apply` 1.3 |
| physics tick, CPU serial chain floor | 7.3 ms | no-op solve shader, `--no-engine-trace` |
| physics tick, GPU chain | ~7.5–9 ms | solve stage 7.19 ms/frame; island dispatch is latency-bound, ~1.5 % lane utilisation |
| first tick after an update | +2–3 ms | `tick_ms=11.0/9.0/8.8/9.0`: update fan-outs contend with the tick's own |
| update wall not hidden by the rollout | 4–5 ms per cycle | `update_ms` 0.5 → 4.1–4.7 once the fifth tick was removed |

The box is an 8-core / 16-thread 7800X3D: ~250 ms of SMT-inflated thread-time is roughly the
whole machine for ~21 ms per cycle, while the ticks need the same cores. Stubbing the island
sweeps (GPU solve 7.19 → 1.10 ms/frame) moved the tick 8.6 → 8.5 ms: further GPU-solve work
returns nothing until CPU time per cycle falls. The update is the single largest CPU item and
it is arithmetic the GPU is idle for — the card runs ~1–2 % of its arithmetic capacity during
the solve.

Expected result of moving the update: the ~180 core-ms per cycle leave the CPU, the update
wall and the first-tick contention disappear, the cycle approaches 4.2 ticks × the 7.3 ms
serial floor ≈ 31 ms → ~130k steps/s at 1024 envs, and env count stops scaling linearly-bad
(the update was the part that grew with samples). The CPU serial chain and the GPU solve then
sit on top of each other again; that is the next plan, not this one.

## What moves and what stays

Moves to the GPU (all of `begin_update`'s background job except sampling):

1. Discriminator update × `disc_updates` (4): forward on 512 rows (256 real + 256 fake),
   backward, R1 gradient penalty on the 256 real rows (double backprop, gated on the loss
   threshold), reduce, Adam.
2. Style rewards: discriminator forward on all 4096 `amp_transitions`, reward blend
   `w_task * r + w_style * shaped(logit)`.
3. GAE per env (1024 chains of ≤ 5 samples, bootstrap per env), advantage mean/variance,
   normalisation.
4. PPO: `n_epochs` (4) × 16 minibatches of 256: actor forward, clipped Gaussian surrogate +
   entropy, actor backward, Adam; critic forward, MSE, backward, Adam.
5. Loss/metric sums for the update log line.

Stays on the CPU:

- Everything in the tick: observation packing, rewards, resets, the serial env loop with its
  shared RNG, **actor inference** (the per-env forward is a small slice of the tick and it is
  what the state hash certifies). Inference reads a CPU copy of the weights, so the GPU update
  must publish weights back to the host each update (~300 KB for three nets).
- Sampling of real reference pairs and fake indices (`update_rng`, `sample_amp_transition`):
  the RNG stream stays exactly as it is; the CPU packs the 4 × 512 discriminator rows.
- Checkpoints: weights and Adam moments are read back after every update, so
  `checkpoint_save` / `checkpoint_save_full` and resume are unchanged (resume uploads the
  loaded weights and moments before the first update).
- The CPU update path stays intact behind the flag for parity work and as the reference.

## Where it runs

- **A new `[[= system_frame{}]]` hook on the trainer**, `trainer::frame`, taking
  `shared_view<gpu::context::data>` and `channel_write<gpu::render_pass_request>`. Recording is
  only possible from a frame hook on the main frame thread (`Runtime/Engine.cpp:287`,
  `scheduler::render`); the existing `post_background` job cannot dispatch. The headless frame
  pump already runs because `use_gpu_solver` is set.
- **Queue: `gpu::queue_type::graphics`**, not compute. The VBD solve occupies the compute queue
  with ~1500 barrier-separated dispatches per tick; on DX12 a hazard lowers to a global UAV
  barrier, so PPO dispatches interleaved on the same queue would serialise against the solve
  and simply add to it (~10 ms of small dependent dispatches on a queue that is already 80 %
  busy would make the cycle GPU-bound at about today's length). Headless has an idle graphics
  queue (`begin_frame::wait_fence::graphics` is present in the profile). Owner decision 1.
- **One chain per update.** `begin_update` packs the inputs into upload channels and sets
  `gpu_update_pending`; the next `trainer::frame` records the whole update as one
  `gpu::pass` chain (`in_chain<ppo_update_chain>`, monotonically increasing index) and ends
  with `copy_buffer` of weights, moments and metrics into a `readback_channel`
  (`readback_gate::frames_in_flight`, graphics queue). `finish_update` polls
  `channel.readable()`; the cycle already tolerates a ~2-frame result latency because the
  result is adopted at the next buffer-full point, exactly as the CPU job's result is today.
  If the chain is not readable by then, `finish_update` waits on the graphics queue fence for
  the recorded ring slot (same shape as `wait_for_latest_dispatch`).

## Kernels

Family `Nn` (engine, `Engine/Resources/Shaders/Bodies/Nn/*.slang`, shared math in
`Shaders/Nn/nn_shared.slang`): generic three-layer tanh MLP training with three loss heads.
Owner decision 2 covers whether the family lives in the engine or the game.

Sizes: `in ≤ 64` (30/40 obs, 54 disc), `hidden = 128`, `out ≤ 16` (10 actions, 1 value/logit);
compile-time constants via `emit_slang_constants` from the config, like the solver's capacities.

| kernel | threads | work | notes |
|---|---|---|---|
| `nn_forward` | one per sample | 3 layers, writes `h1`, `h2`, `out` | 256 or 512 or 4096 samples per dispatch |
| `nn_output_delta_ppo` | one per sample | Gaussian log-prob, ratio, clip, entropy term, `out_delta`, `log_std` partials, `sample_loss` | reads `actions`, `old_log_probs`, `advantages` |
| `nn_output_delta_mse` | one per sample | `value_coeff * (out - target)` | critic |
| `nn_output_delta_lsgan` | one per sample | `out - (real ? 1 : -1)`, `sample_loss` | discriminator |
| `nn_backprop_deltas` | one per (sample, hidden unit) | `h2_pre`, `h1_pre` per sample | two dispatches or one with a phase constant |
| `nn_weight_grad` | one per parameter | `Σ_s δ_s[j] · h_s[i]` over the minibatch in **fixed sample order** | 22k–24k threads; no atomics |
| `nn_r1_accumulate` | one per real sample → per-sample term buffers; then `nn_weight_grad`-style fixed-order fold | the double backprop of `discriminator_r1_accumulate` | only when `loss ≥ gate_loss` — gate is a readback-free branch: the fold kernel multiplies by a flag the loss-sum kernel wrote |
| `nn_reduce_scalars` | one workgroup | loss sums, `mean_real/fake`, `r1`, advantage mean/var | fixed-order tree in groupshared |
| `nn_adam` | one per parameter | scale by `1/n`, add R1 term, Adam step, write ping-pong weights | `step` from push constant |
| `ppo_gae` | one per env | walk the env's samples backwards, bootstrap | needs `env_of` → per-env index lists built on the CPU (already done by `compute_gae`) |
| `ppo_normalize_adv` | one per sample | `(adv - mean) / sqrt(var + 1e-8)` | after `nn_reduce_scalars` |
| `amp_style_blend` | one per sample | shaped logit → reward blend | after the 4096-row forward |

Dispatch count per update: discriminator 4 × ~8 + style 3 + GAE/normalise 4 + PPO 64 × 2 nets ×
~5 ≈ 680, each tens of µs of latency-bound work: ~10–14 ms of graphics-queue time, entirely
overlapped with the compute queue's solve. Arithmetic is ~10 GFLOP per update; irrelevant.

## Buffers (allocated once at init from capacities; never grown — GRAPHICS_REVIEW C1)

Inputs via `upload_channel` (bindless SSBOs written host-side in `begin_update`):
`obs[4096×in]`, `actions[4096×act]`, `old_log_probs`, `values`, `rewards`, `dones`, `env_of`,
per-env sample index lists, `bootstrap[1024]`, `amp_transitions[4096×54]`,
`disc_rows[4×512×54]`, and a `ppo_update_params` struct (lr, entropy, clip, coefficients,
counts, Adam step base).

Device-local, ping-pong where a dispatch reads and writes the same logical array:
per net `weights[2]`, `adam_m`, `adam_v` (Adam reads m/v and writes them for *different*
parameters per thread, so in-place is safe), `h1/h2/out` for the largest batch (4096 × 128),
`h1_pre/h2_pre/out_delta` (512 × 128), `grads`, `r1_terms`, `scalars`, `advantages`,
`returns`, `style_rewards`.

Readback via `readback_channel` per update: weights (3 nets), moments, `log_std`, metrics
(`actor_loss`, `critic_loss`, `disc` metrics, `mean_style`).

One binding `type_pack` for the family, order frozen; all bindless slots travel in
`binding_args`, never in push constants (Vulkan barrier tracking depends on it,
`solver_plan.md:1227`).

## Determinism

The CPU path's hash cannot survive: different arithmetic order and compilers. What the GPU
path must deliver instead is **run-to-run bit determinism on each backend** so the hash gate
keeps working for everything downstream:

- No float atomics, no atomic-append anywhere: every reduction is a single thread folding a
  fixed range in fixed order (`nn_weight_grad`, `nn_reduce_scalars`), or a fixed groupshared
  tree. The 16-slice CPU reduction has the same property and the GPU version keeps it.
- Every dispatch reads only buffers the previous dispatch finished writing (Jacobi shape); the
  NVIDIA determinism report's only bit-stable configuration on this card is exactly that.
  Weights ping-pong so a minibatch never reads weights that the same dispatch group writes.
- `-0.0` normalisation and count denominators in any diagnostic hash.
- Gate convention for the determinism claim: 3 rounds of 6 runs identical, per backend.

## Gates (in order; nothing is adopted before all pass)

1. **Unit parity.** A test mode runs one update on both paths from the same inputs and reports
   max |Δ| per tensor; expect ~1e-5 relative (float order), not zero. Catches wrong math.
2. **Run-to-run determinism**, Vulkan and DX12, 3 × 6 runs each, 100 updates.
3. **Learning parity**, three seeds at 20 M against `lag_async_s{2,3}` / `async1024_s4`,
   same bar 2048 envs and carry-over cleared (training survival level, eval mean not behind).
4. **Throughput**, interleaved `gaussian-ab`, 6 + 6 arms, 1024 and 2048 envs, 15 workers.
5. **Both backends** at every step; DX12 via `--engine-setting Graphics.backend=dx12`, verified
   by `[dx12]` log lines.
6. Flag `--ppo-gpu-update`, off by default until 1–5 pass; then flipped like `async_update`.

## Risks and open questions I can answer myself

- **Graphics queue in headless.** The fence exists; whether `pass_builder.on(graphics)` is
  accepted without a swapchain needs a smoke test before any kernel work (step 0 below).
- **Readback latency vs. cycle.** Recording happens in the frame after `begin_update`, results
  are readable `max_frames_in_flight` frames after the chain's frame. With ~9 ms ticks and a
  4.2-tick cycle there is slack; the fallback is the queue-fence wait.
- **`n = 4096` exactly.** Carry-over guarantees the batch size, so the minibatch grid is static
  (16 × 256) and the per-update dispatch list is fixed — no indirect dispatch needed.
- **Resume.** Upload weights and moments on `checkpoint_load_full`; `opt.step` continues.
- **R1 gate.** The CPU path returns early when `loss < gate_loss`; the GPU path evaluates the
  branch as a multiplier written by the scalar-reduce kernel, so the dispatch list stays static.
- **Inference snapshot timing.** `snapshot_inference` copies the *read-back* weights; the
  behaviour policy still trails by exactly one update.

## Open decisions for the owner

1. **Queue.** Graphics queue for the update chain (recommended, gives true overlap with the
   solve on both backends) vs compute queue in its own chain (simpler, but DX12's global UAV
   barriers serialise it against the solve).
2. **Ownership of the kernels.** Engine family `Nn` (generic MLP training, reusable; shaders
   resolve from `Engine/Resources/Shaders` today) vs game-side shaders (would need a project
   shader search path in `PipelineBuilder`, an engine change on its own).
3. **Sample count.** Confirm the design may assume the carry-over default (`n = rollout_steps`
   always); otherwise the last minibatch needs an indirect size.
4. **Scope.** This plan stops at the update. Inference, observations and rewards on the GPU
   (the step that removes the per-tick readback) is a separate plan after this one lands.

## Work plan

0. ~~Smoke: an empty graphics-queue compute pass from a trainer `system_frame` hook, headless,
   both backends, hash unchanged.~~ Done, see Status.
1. `Nn` family: forward, deltas, weight grad, Adam; unit parity harness against the CPU nets
   on random data. Both backends.
2. Discriminator + R1 + style rewards; GAE + normalisation kernels.
3. `trainer::frame` chain assembly, upload/readback channels, resume path, flag, metrics.
4. Gates 1–5, then default flip and doc/memory updates (`HANDOFF.md`, `LOCOMOTION.md`).

Estimate: 3–5 working days including the gates, most of it in steps 1 and 4.

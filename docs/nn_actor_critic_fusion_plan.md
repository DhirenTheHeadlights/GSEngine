# Fusing the actor and critic net steps into one dispatch pair

Status: **REFUTED by its own probe, 2026-09-14. Do not build this.** The mechanism is real and the launch cost is fixed-dominated exactly as predicted — but the NN chain time it would save does not convert into frame time. Evidence in "Probe result" below. Written against `HumanoidLocomotion.revert1.exe` (gc30 + the island-solve state narrowing + the `vbd_solve_island` revert).

## Probe result (the reason this is dead)

One build, `mini_batch` 256 -> 512 in `LocomotionTrainer.cppm:21`, exe `HumanoidLocomotion.mb512.exe`, one marks arm at 8192 envs / 120 updates / 15 workers, Compilers 0, 206 frames, against the `revert1.exe` arm measured the same way on an unchanged tree.

**Half one — the mechanism is confirmed.** Doubling the workgroups in a dispatch costs far less than double:

| kernel | avg @ 256 groups | avg @ 512 groups | ratio for 2x the work | implied fixed cost |
| --- | ---: | ---: | ---: | ---: |
| `nn::train_sample` | 37.50 us | 44.42 us | **1.18x** | **30.58 us (82 %)** |
| `nn::weight_grad` | 31.65 us | 47.87 us | 1.51x | 15.43 us (49 %) |

Fusing actor and critic would therefore save `30.58 + 15.43 = 46.0 us` per minibatch pair over 125.6 pairs/frame = **5.8 ms/frame of GPU dispatch time**, in the middle of the 4–7.5 ms predicted below.

**Half two — the saving does not convert, and this is what kills it.** The same probe already delivers a 5.45 ms/frame cut to the NN chain, and the frame barely moves:

| | mb256 | mb512 | delta |
| --- | ---: | ---: | ---: |
| `nn_chain_stage` per/f | 17.80 ms | 12.35 ms | **−5.45 ms** |
| measured mean frame | 34.09 ms | 33.57 ms | **−0.52 ms** |
| main-thread scoped self | 21.41 ms | 20.88 ms | −0.53 ms |
| GPU top (`vbd::solve::island`) | 24.25 ms | 24.17 ms | −0.08 ms (noise) |

**The frame moved −0.52 ms and the main thread moved −0.53 ms.** The frame gain is fully accounted for by the CPU side doing half as many minibatch recordings; the 5.45 ms of GPU NN-chain time converted to approximately zero. The NN chain is already hidden behind the 24.25 ms island solve, so removing it buys nothing until the island solve moves.

Projecting the fusion's 5.8 ms through the same ~10 % conversion gives **~0.6 ms of frame, under 2 %** — the same order as gc30's +1.3 %, which was judged inside noise and closed as a lever. Not worth the bindings change, two shader rewrites and five gates on two backends.

Caveats on the probe, stated so the refutation can be re-examined rather than inherited: the mb512 arm is **n=1**, and `mini_batch` 512 is not the same algorithm — it changes both the GPU dispatch shape and the CPU recording count, so it is not a clean isolation of the GPU effect. What carries the argument is the size of the gap (5.45 ms of chain against 0.52 ms of frame), not the precision of either number.

**Incidental finding worth its own gate:** `mini_batch` 512 ran **238.1k steps/s against 232.3k (+2.5 %)** with frame 33.57 vs 34.09 ms. That is a real throughput gain available from a one-constant change — but the state hash is `56D084E7B325685B` against the reference `D47B9290B5093B17`, because 64 minibatches of 512 is a different PPO update from 128 of 256. It needs the three-seed learning-parity gate, not a hash gate. Filed, not adopted.

## The original plan follows, for the record

## The measurement this rests on

8192 envs, 15 workers, 120 updates, `Graphics.gpu_intra_pass_marks_enabled=true`, Compilers 0, 206 frames profiled. 232,300 steps/s, frame 34.09 ms, state hash `D47B9290B5093B17` (identical to the gc26/gc30 reference, so the tree under measurement is the gated one).

GPU rows, per frame:

| row | per/f | calls/f | avg |
| --- | ---: | ---: | ---: |
| `vbd::solve::island` | 24247 us | 80.00 | 303.08 us |
| `nn_chain_stage` | 17802 us | 0.24 | 74842 us |
| — `nn::train_sample` | 9419 us | 251.18 | 37.50 us |
| — `nn::weight_grad` | 7950 us | 251.18 | 31.65 us |
| everything else (17 VBD stages) | ~8500 us | | |

Main-thread scoped self is 21.41 ms, of which `begin_frame::wait_fence::graphics` is 6.13 ms and `wait_fence::compute` 0.41 ms — i.e. ~6.5 ms of the main thread is blocked on the GPU, leaving ~15 ms of actual CPU work under a 34.09 ms frame.

**The GPU is the binding side at 8192, not the CPU chain.** Roughly 50 ms of GPU dispatch time lands in a 34 ms frame, so the island solve and the async NN chain are already overlapping and the GPU is oversubscribed. This supersedes the 2026-09-13 reading that the frame is the CPU serial chain; that was true before gc19–gc23 cut the chain.

## The premise

`nn::train_sample` and `nn::weight_grad` are the two hot NN kernels and they run **1047 and 1047 times per update** (4 epochs x 128 minibatches x 2 nets, plus the discriminator's share). At 8192 envs the rollout is 32768 samples and `cfg.mini_batch` is 256, so `nn::train_sample` dispatches **256 workgroups of 128 threads** — 1.5 workgroups per SM on a 170-SM GB202. The dispatch cannot fill the machine and is dominated by launch and drain rather than by work.

The actor and critic steps inside one minibatch are **independent**: they read the same `nn_inputs` rollout buffer, and each writes only its own `nn_state` and `nn_scratch`. `LocomotionGpuUpdate.cpp:807-817` records them as four separate dispatches:

```
record_train_sample(rec, g, g.actor,  r.buffer, actor_pc);
record_train_sample(rec, g, g.critic, r.buffer, critic_pc);
record_weight_grad(rec, g, g.actor,  r.buffer, actor_pc);
record_weight_grad(rec, g, g.critic, r.buffer, critic_pc);
```

gc27–gc30 already tried to make these two *overlap* by removing the barriers between them. The barriers went away (292 flushes, 223 barriered -> 158) and it was worth +1.3 %, inside noise: the GPU does not run independent dispatches concurrently here. **Fusing sidesteps that entirely** — one dispatch of 512 workgroups instead of two of 256, same work, half the launches, and twice the workgroups per launch.

## Expected size, and the honest uncertainty

If the per-dispatch cost is `fixed + k * samples`, halving the launch count saves `fixed` per fused pair. Fitting `fixed` from this profile is where the estimate is soft:

- `nn::forward` (32768 samples, forward only) is 263.48 us; `nn::train_sample` (256 samples, forward + backward) is 37.50 us. These are **different kernels**, so the two points do not fit one line and the resulting `fixed ~ 30 us` is an extrapolation across kernels, not a measurement.
- `nn::r1_sample` at 12.71 us over 7.61 calls/f is a third per-sample kernel and gives a lower floor.

Taking `fixed` somewhere in 15–30 us: fusing saves that once per minibatch per kernel, i.e. `125.6 * (fixed_ts + fixed_wg)` us/frame, or **4–7.5 ms/frame out of the 17.8 ms NN chain**. Because the GPU is oversubscribed rather than idle, that time should convert to frame time rather than disappearing into slack.

**Kill it cheaply before building anything.** Run one arm with `cfg.mini_batch = 512` and read `nn::train_sample` avg us off the marks profile. If avg is roughly flat from 256 -> 512 groups, the cost is fixed-dominated and fusion pays about 2x on these rows. If avg roughly doubles, the kernel is already work-bound, `fixed` is small, and **this plan should be abandoned**. `mini_batch` is a config field (`LocomotionTrainer.cppm:21`), not a CLI flag, so the probe costs one build — still far less than the change. Note the probe arm is a *timing* probe only: changing `mini_batch` changes PPO semantics and its state hash is expected to differ.

## What changes

1. **`nn_bindings` gains a second net's slots.** Today `bindings_for(net, inputs)` (`LocomotionGpuUpdate.cpp:539`) binds `nn_state`, `nn_inputs`, `nn_scratch` for one net. A fused entry needs `nn_state_b` and `nn_scratch_b` alongside. The read-tagged variants (`nn_read_bindings`, `nn_write_bindings`) need the same treatment, and the `ssbo_read_use` tag added in gc30 must be carried over verbatim — it is what keeps the `RWStructuredBuffer` declaration and so keeps DXIL unchanged.

2. **`nn_push` carries both halves.** `in_dim`, `out_dim`, `param_count`, `mode`, `target_offset`, `aux_offset`, `adv_offset`, `adam_step` and `inv_n` all differ between actor and critic. Either widen `nn_push` to a second block or index a small constant array by net. Push-constant size limits apply; `nn_push` is already large, so check the backend limit before assuming a second block fits.

3. **`nn_train_sample.slang` routes on `group_id.x`.** `uint s = group_id.x;` becomes a split: `s < pc.sample_count` selects net A, otherwise `s - pc.sample_count` selects net B, with the corresponding state/scratch buffer and `mode`. Every `nn_state[...]` and `nn_scratch[...]` reference in the body has to go through that selection — 118 lines, and the groupshared arrays are per-group so they need no change.

4. **`nn_weight_grad.slang` routes the same way**, but its group count differs per net (`groups_for(param_count + nn_reduce_width, nn_reduce_width)`), so the split point is the actor's group count rather than `sample_count`. The extra reduce group that rides at the end of each net's range (`nn_reduce_group`, which also does the Adam bias correction) must stay one-per-net.

5. **The record loop collapses** to two calls per minibatch instead of four.

## Why this should be bit-identical

Each workgroup does exactly the work it did before, against the same buffers, in the same order:

- `nn_train_sample` has **no cross-group reduction** — group `s` reads `nn_inputs` and its net's `nn_state`, and writes only `nn_scratch` at `sample`-indexed offsets. Merging two dispatches into one interleaves groups in launch order but never in data.
- `nn_weight_grad`'s reduction is within a group (`gs_loss`/`gs_out` over `nn_reduce_width`, summed serially by lane 0). Keeping each group's sample range unchanged keeps the summation order and therefore the float result.
- The two nets write disjoint `nn_state` and `nn_scratch` buffers, so there is no new write hazard between the fused halves.

This is a launch-shape change, not a math change, so it should gate on hashes rather than on learning. **That is the property that makes it worth doing** — contrast `mini_batch` batching across minibatches, which changes PPO's sequential weight updates and would need the three-seed learning gate.

The one real risk to that claim: gc26 already found that **declaring `nn_state`/`nn_inputs` read-only moved the DX12 hash** (`AC3D5339` held on Vulkan, DX12 went to `344F8A89`) because the `StructuredBuffer` load shape changes DXIL math. Adding buffer slots is not the same as changing their access class, but it touches the same declarations, so DX12 must be gated explicitly and early — a Vulkan-only gate would hide exactly this failure. See `dx12-partial-struct-loads-change-hash`.

## Gates

Per `gpu-backend-changes-need-parity-and-design`, both backends, and every `.slang` edit gated on both:

| gate | expected |
| --- | --- |
| 1024 envs / 409600 steps, Vulkan | `AC3D5339...` |
| 1024 envs / 409600 steps, DX12 | `F838905F...` |
| 8192 envs / 120 updates | `D47B9290B5093B17` |
| opt-in kernel ref | `91950805...` |
| `--no-ppo-gpu-update` opt-out, both backends | `83219603...` / `D02B4F03...` |

Throughput A/B at 8192, 4+4 interleaved arms against `revert1.exe`, Compilers 0 verified per arm, plus a marks profile to confirm `nn::train_sample` and `nn::weight_grad` calls/f actually halved and their avg did not double.

## What this does not touch

`vbd::solve::island` is **24.25 ms of the 34.09 ms frame — 71 %, and the single largest row by 1.4x over the whole NN chain.** This plan does not address it. Everything cheap on that kernel is refuted: island-per-lane, iteration cap, `solve_fold`, the LDS constraint cache, prefetch, the dedicated island entry, the per-level barrier, and register pressure (three separate attacks on 2026-09-14, RF-to-warp flat at 3.93–3.94). At 303 us per dispatch it is work-bound, not launch-bound, so fusion logic does not apply to it.

The remaining lever there is **iteration count**: 40 dispatches per tick with a convergence exit that rarely fires. Cutting it changes physics, so it needs the three-seed learning gate rather than a hash gate — a much heavier proposition, and it should be scoped separately.

For 600k steps/s at 8192 envs the frame must reach 13.65 ms. The island solve alone is currently 24.25 ms. **Fusing the NN chain is worth 4–7.5 ms and is the cheapest real win available, but it does not on its own get anywhere near the target** — the island solve has to roughly halve as well, and no known mechanism does that yet.

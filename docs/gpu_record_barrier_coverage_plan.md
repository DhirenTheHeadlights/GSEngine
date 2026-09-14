# GPU PPO update: let the actor and critic steps overlap

## Problem

At 8192 envs the PPO update chain costs 19.7 ms of GPU time per rollout frame (83 ms per
update, `nn_chain_stage` per-dispatch marks, `gpuchain25`), against 27.3 ms for the island solve.
The rollout tick that shares the GPU with the chain takes 58 ms where the others take 33 ms.
The chain is 128 minibatches × 4 epochs × 2 nets of two dispatches each: `nn_train_sample`
(256 workgroups of 128 lanes, 40 µs) and `nn_weight_grad` (~90 workgroups of 256 lanes, 37 µs).
On a 170-SM part both are latency floors, not throughput; the actor and critic steps of a
minibatch are independent but run strictly one after the other, for two reasons:

1. Every nn kernel binds the same `nn_bindings` pack, in which `nn_state`, `nn_inputs` and
   `nn_scratch` are all `ssbo_readwrite`, and all three nets share one `scratch` buffer. The
   recording context therefore sees a write hazard on every dispatch and emits a barrier
   before each one.
2. `recording_context::emit_intra_pass_barrier` tracks the last access per resource and
   never learns that a barrier it has already flushed covers earlier writes. After the barrier
   that precedes the actor's `train_sample`, the critic's `train_sample` reading
   `critic.state` (last written by the previous critic `weight_grad`) is still classed as a
   hazard and gets its own barrier, even though the flushed global memory barrier already
   made that write visible.

## Change

Game side (`Source/Locomotion/LocomotionGpuUpdate.cpp`, `LocomotionTrainer.cppm`):

- Each `gpu_net` owns its scratch buffer (`net.scratch`, same layout and size as today);
  `gpu_state::scratch` goes away. `record_zero_scalars` clears all three, `record_net_readback`
  copies the scalars from the net's own scratch, and `gpu_poll_update` reads the actor and
  critic losses from their own readbacks and the discriminator scalars from its readback.
- A second binding pack `nn_read_bindings` declares `nn_state` and `nn_inputs` `ssbo_readonly`
  (codegen emits `StructuredBuffer` instead of `RWStructuredBuffer`) and keeps `nn_scratch`
  read-write. `nn_train_sample`, `nn_forward` and `nn_r1_sample` use it; they never write
  state or inputs. Kernels that write state (`nn_weight_grad`, `nn_adam`, `nn_reduce_loss`,
  `nn_transpose`) or inputs (`nn_gae`, `nn_adv_norm`, `nn_style_reward`) keep `nn_bindings`.
- The minibatch loop records actor `train_sample`, critic `train_sample`, actor `weight_grad`,
  critic `weight_grad` instead of interleaving the two nets.

Engine side (`Engine/Engine/Source/GpuRecord/RecordingContext.cpp`,
`flush_pending_barriers`): after the pipeline barrier is recorded, walk `m_last_access` and,
for every entry whose `stages` and `access` are subsets of the flushed barrier's `src_stages`
and `src_access`, rewrite the entry as a read at the barrier's destination
(`stages = dst_stages`, `access = dst_access` with the write bits cleared, `memory_read` if
nothing remains). A later read of such a resource is then no hazard; a later write still is,
so write-after-read and write-after-write keep their barrier.

With both parts the minibatch records as: barrier, actor `train_sample`, critic `train_sample`,
barrier, actor `weight_grad`, critic `weight_grad`. Two barriers per minibatch instead of four,
and each pair of independent dispatches can overlap on the queue.

## What the first build showed (2026-09-13, `gpuchain26`)

Declaring `nn_state`/`nn_inputs` `ssbo_readonly` makes the codegen emit `StructuredBuffer`
instead of `RWStructuredBuffer`. Vulkan stayed on `AC3D5339…`, but DX12 moved to
`344F8A89…` while the same exe with `--no-ppo-gpu-update` still gives `D02B4F03…`: the DXIL
path changes floating-point results with the SRV load shape, exactly as the fold3 slim loads
did in the solver. The read-only declaration is therefore dropped from the game side.

The engine part grows by one tag: a binding annotation (working name `ssbo_readwrite_readonly_use`,
or an `access_read` companion to `ssbo_readwrite`) that keeps the `RWStructuredBuffer`
declaration in the generated wrapper, so the DXIL is byte-identical, but reports
`descriptor_access::read` to `binding_access_contribution` and `register_one_bindless`. The
game code keeps a `nn_read_bindings` alias (currently equal to `nn_bindings`) that switches to
the new tag once it exists. Without that tag the tracker sees every nn dispatch as a write and
the coverage rule alone cannot let the actor and critic steps overlap.

The shared helper `nn_store_transposed` wrote `nn_state` and would not compile under a
read-only declaration even in kernels that never call it, so it now lives in
`Shaders/Nn/nn_state_write.slang`, included only by `nn_weight_grad`, `nn_adam` and
`nn_transpose`. Exes older than `gpuchain26` need `.gse/ab/shaders/nn_shared_base.slang`
installed as `Shaders/Nn/nn_shared.slang` to run (`shader-ab.ps1 -ExtraShaders`).

## Why this is bit-identical

No kernel changes. Every dispatch reads and writes the same values as before; only the buffer a
net's intermediates live in and the number of barriers change. The tracker still emits a
barrier for every real hazard: the only barriers removed are those whose source writes were
already covered by an earlier global memory barrier on the same queue. Slang `StructuredBuffer`
reads and `RWStructuredBuffer` reads return the same data.

## Gate

- Default 1024-env gates on both backends: Vulkan `AC3D5339…`, DX12 `F838905F…`.
- Opt-in GPU-update kernel reference `91950805…` at 204800 steps.
- 8192-env hash `F8B86412…` at 80 updates, and an interleaved A/B against `gpuchain25` at 8192
  with the `chain_ms` log figure and the `nn_chain_stage` per/f row from a marks profile.
- Both backends' logs grepped for device loss and validation errors.
- The engine part lands only after this plan is approved; the game part is safe on its own but
  yields nothing measurable without it.

## Implementation (approved 2026-09-14, `gpuchain27`)

- `ShaderCodegen.cppm`: `ssbo_read_use` tag; `descriptor_access_of` reports `read` for it while
  the declaration stays `RWStructuredBuffer`. The game's `nn_read_bindings` carry
  `[[= ssbo_readwrite, = ssbo_read_use]]` for `nn_state`/`nn_inputs`.
- `RecordingContext`: `access_track` carries a `generation` stamped from `m_access_generation` on
  every `emit_intra_pass_barrier` path; `flush_pending_barriers` bumps the generation, emits the
  barrier, then `cover_barrier_sources` rewrites every entry not touched by this dispatch whose
  stages and access sit inside the flushed barrier's source union to the barrier's destination
  stages and read-only destination access.
- The bindings-repeat fast path (`note_bindings_repeat`) does not re-register its resources, so
  a flush that follows it sets no coverage (`m_repeat_barrier_pending`); the entries keep their
  write state and stay conservative.
- `gpuchain27` (coverage alone) passed both gates (`AC3D5339…` / `F838905F…`) and the 8192 hash
  matched `gpuchain26` (`D47B9290…` at 120 updates) but `nn_chain_stage` stayed at 17.0 ms per
  frame: a covered entry reads as a plain read, so the critic's next write to its scratch is a
  write-after-read hazard and still flushes a barrier. `gpuchain28` marks covered entries
  (`access_track::covered`). The first real touch of a covered entry at stages inside the covering
  barrier's destination stages, with a read inside its destination access or any write, records
  the access and emits nothing, because the covering barrier already ordered every earlier
  command against those stages. Any other touch falls back to a hazard against
  `memory_read | memory_write` at the covering stages.

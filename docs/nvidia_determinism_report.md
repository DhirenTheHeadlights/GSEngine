# Non-deterministic compute results in a barrier-separated dispatch sequence (RTX 5090)

## Summary

A Vulkan compute workload — a coloured Gauss-Seidel physics solver issuing ~1,500 barrier-separated `vkCmdDispatch`/`vkCmdDispatchIndirect` calls per simulation tick — intermittently produces different floating-point results from bit-identical inputs on identical API command streams. The divergence begins as a one-ULP-scale perturbation of a single body's state at a reproducible simulation event, resolves bistably (affected runs agree with each other, not with clean runs), and its per-run probability changes with driver version and with the number of recorded barrier commands, but not with GPU clocks. Synchronization validation reports zero hazards. An equivalent Jacobi-style variant of the same workload (no cross-dispatch read-after-write within an iteration) is bit-exactly deterministic across every configuration tested.

## Environment

- GPU: NVIDIA GeForce RTX 5090
- Drivers tested: 596.75 and 596.99 (behavior differs between them — see below)
- OS: Windows 11 Pro 10.0.26200
- API: Vulkan 1.4 (also reproduced through the D3D12 backend of the same engine at the same simulation events)
- Validation: VK_LAYER_KHRONOS_validation from SDK 1.4.350.0 with `VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`: zero hazards reported on the repro content.

## Workload shape

Per simulation tick: collision passes build contact data with atomic-append (order-nondeterministic buffer layout, content-deterministic), then a solver loop runs 40 iterations × 2 substeps; each iteration dispatches one compute shader per graph colour (~7 live colours) plus a per-contact update and a single-workgroup convergence check. Consecutive dispatches are separated by `vkCmdPipelineBarrier2` with per-buffer `VkBufferMemoryBarrier2` (COMPUTE→COMPUTE, storage read/write both sides). Colour partitioning guarantees no two threads in one dispatch read or write the same body; each thread's arithmetic per dispatch is a fixed sequence (the accumulation over each body's contact list is integer fixed-point, i.e. order-invariant). All inputs to the diverging dispatch sequence were verified bit-identical between a diverging and a clean run on the same tick via GPU-side whole-buffer hashes.

## Observations

1. **Bistable single-event divergence.** Runs diverge at exact, repeatable simulation events (e.g. one marginal box-on-box first-impact at frame 10 of our stress scene). Affected runs agree bit-exactly *with each other* for tens of frames afterwards — the event resolves one of exactly two ways.
2. **Driver-version dependence.** On 596.75 the frame-10 event flipped in roughly 1 of 6 runs (300-frame runs); on 596.99, with the same executable, shaders, and scene, it flipped in 2-3 of 6, and 2 of 12 forty-frame runs. Event onset frames also shifted between drivers on the same executable (expected: new SASS, different rounding), but the *nondeterminism rate* changing with driver version is the key datum.
3. **Barrier-command pressure dependence.** Our engine originally emitted one `vkCmdPipelineBarrier2` per hazarded buffer per dispatch boundary (~25-30 barrier commands per boundary, ~40,000 per tick). After batching these into one `vkCmdPipelineBarrier2` per boundary with identical scopes (~1,500 per tick), the most frequent divergence event disappeared entirely (45 run-pairings across three rounds, zero occurrences; previously 8-9 of 15 pairings per round) and remaining events became rarer. Sync semantics are identical; only the command count changed.
4. **Clock independence.** Locking GPU clocks (`nvidia-smi -lgc`, verified held at 1972 MHz) does not change the rate.
5. **Jacobi control.** Replacing the coloured Gauss-Seidel iteration with a Jacobi variant (each dispatch reads a snapshot written by the previous dispatch only — no live neighbour reads within an iteration) is bit-exactly deterministic: 15/15 run pairings identical over 300 frames, on both driver versions.
6. **Serialization control.** Executing each colour's body list from a single thread (all else unchanged) is deterministic 6/6.
7. **In-shader detectors find nothing.** A per-body iteration-stamp check (each solve writes a stamp; each neighbour read verifies the stamp the colour schedule promises) found zero behind-by-one reads across ~5.6M checked reads per run. A write-then-`AllMemoryBarrier`-then-readback self-check on the written body state found zero mismatches.

## Why we believe this is below the API

Every software mechanism reachable by instrumentation has been measured and excluded: inputs bit-identical on one clock up to the diverging dispatch sequence, colour partition exact (no two contact-neighbours share a colour; verified per tick), walk order canonical, accumulations order-invariant, sync validation clean, no wave intrinsics or groupshared in the affected shaders, shader/host struct strides verified. What remains correlates only with execution conditions the API contract says should not matter: driver version, recorded barrier-command count, and host/GPU submission cadence (tracing readbacks and 17×-slower serialized frames both suppress the rate).

## Repro availability

We can provide: the application binary plus a scripted repro (a 40-frame run reproduces at ~17-25% per run on 596.99, seconds per attempt; a byte-comparing state-dump harness scores divergence automatically), GFXReconstruct captures of clean and diverging runs, and the Slang/SPIR-V for the dispatched shaders. The two-way discriminator (coloured Gauss-Seidel diverges, Jacobi identical, same buffers and barriers) is included in the repro as a command-line toggle (`Physics.use_jacobi`).
